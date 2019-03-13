// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_service_factory.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/extension_id.h"
#include "services/service_manager/public/cpp/connector.h"

namespace extensions {
namespace declarative_net_request {

namespace {

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<RulesMonitorService>>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

void LoadRulesetOnIOThread(ExtensionId extension_id,
                           std::unique_ptr<RulesetMatcher> ruleset_matcher,
                           URLPatternSet allowed_pages,
                           InfoMap* info_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info_map);

  // Only a single ruleset per extension is supported currently.
  CompositeMatcher::MatcherList matchers;
  matchers.push_back(std::move(ruleset_matcher));

  info_map->GetRulesetManager()->AddRuleset(
      extension_id, std::make_unique<CompositeMatcher>(std::move(matchers)),
      std::move(allowed_pages));
}

void UnloadRulesetOnIOThread(ExtensionId extension_id, InfoMap* info_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info_map);
  info_map->GetRulesetManager()->RemoveRuleset(extension_id);
}

// A helper class to hold the data relating to the loading of a single ruleset.
class RulesetInfo {
 public:
  explicit RulesetInfo(RulesetSource source) : source_(std::move(source)) {}
  ~RulesetInfo() = default;
  RulesetInfo(RulesetInfo&&) = default;
  RulesetInfo& operator=(RulesetInfo&&) = default;

  const RulesetSource& source() const { return source_; }

  // Returns the ownership of the ruleset matcher to the caller. Must only be
  // called for a successful load.
  std::unique_ptr<RulesetMatcher> TakeMatcher() {
    DCHECK(did_load_successfully());
    return std::move(matcher_);
  }

  // Clients should set a new checksum if the checksum stored in prefs should
  // be updated.
  void set_new_checksum(int new_checksum) { new_checksum_ = new_checksum; }
  base::Optional<int> new_checksum() const { return new_checksum_; }

  // The expected checksum for the indexed ruleset.
  void set_expected_checksum(int checksum) { expected_checksum_ = checksum; }
  int expected_checksum() const {
    DCHECK(expected_checksum_);
    return *expected_checksum_;
  }

  // Must be called after CreateVerifiedMatcher.
  RulesetMatcher::LoadRulesetResult load_ruleset_result() const {
    DCHECK(load_ruleset_result_);
    // |matcher_| is valid only on success.
    DCHECK_EQ(load_ruleset_result_ == RulesetMatcher::kLoadSuccess, !!matcher_);
    return *load_ruleset_result_;
  }

  // Must be called after CreateVerifiedMatcher.
  bool did_load_successfully() const {
    return load_ruleset_result() == RulesetMatcher::kLoadSuccess;
  }

  // Must be invoked on the file sequence. Must only be called after the
  // expected checksum is set.
  void CreateVerifiedMatcher() {
    DCHECK(expected_checksum_);
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    load_ruleset_result_ = RulesetMatcher::CreateVerifiedMatcher(
        source_, *expected_checksum_, &matcher_);

    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.DeclarativeNetRequest.LoadRulesetResult",
        load_ruleset_result(), RulesetMatcher::kLoadResultMax);
  }

 private:
  RulesetSource source_;

  // The expected checksum of the indexed ruleset.
  base::Optional<int> expected_checksum_;

  // Stores the result of creating a verified matcher from the |source_|.
  std::unique_ptr<RulesetMatcher> matcher_;
  base::Optional<RulesetMatcher::LoadRulesetResult> load_ruleset_result_;

  // The new checksum to be persisted to prefs. A new checksum should only be
  // set in case of flatbuffer version mismatch.
  base::Optional<int> new_checksum_;

  DISALLOW_COPY_AND_ASSIGN(RulesetInfo);
};

}  // namespace

// static
BrowserContextKeyedAPIFactory<RulesMonitorService>*
RulesMonitorService::GetFactoryInstance() {
  return g_factory.Pointer();
}

bool RulesMonitorService::HasAnyRegisteredRulesets() const {
  return !extensions_with_rulesets_.empty();
}

bool RulesMonitorService::HasRegisteredRuleset(
    const ExtensionId& extension_id) const {
  return extensions_with_rulesets_.find(extension_id) !=
         extensions_with_rulesets_.end();
}

void RulesMonitorService::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void RulesMonitorService::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

// Helper to pass information related to the ruleset being loaded.
struct RulesMonitorService::LoadRequestData {
  LoadRequestData(ExtensionId extension_id, RulesetInfo ruleset)
      : extension_id(std::move(extension_id)), ruleset(std::move(ruleset)) {}

  ~LoadRequestData() = default;
  LoadRequestData(LoadRequestData&&) = default;
  LoadRequestData& operator=(LoadRequestData&&) = default;

  ExtensionId extension_id;
  RulesetInfo ruleset;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoadRequestData);
};

// Maintains state needed on |file_task_runner_|. Created on the UI thread, but
// should only be accessed on the extension file task runner.
class RulesMonitorService::FileSequenceState {
 public:
  FileSequenceState()
      : connector_(content::ServiceManagerConnection::GetForProcess()
                       ->GetConnector()
                       ->Clone()),
        weak_factory_(this) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~FileSequenceState() {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  }

  using LoadRulesetUICallback = base::OnceCallback<void(LoadRequestData)>;
  // Loads ruleset for |load_data|. Invokes |ui_callback| with the
  // RulesetMatcher instance created, passing null on failure.
  void LoadRuleset(LoadRequestData load_data,
                   LoadRulesetUICallback ui_callback) const {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    load_data.ruleset.CreateVerifiedMatcher();

    if (load_data.ruleset.did_load_successfully()) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(std::move(ui_callback), std::move(load_data)));
      return;
    }

    // Clone the RulesetSource before moving |load_data|.
    RulesetSource source_copy = load_data.ruleset.source().Clone();

    // Attempt to reindex the extension ruleset.
    // Using a weak pointer here is safe since |ruleset_reindexed_callback| will
    // be called on this sequence itself.
    RulesetSource::IndexAndPersistRulesCallback ruleset_reindexed_callback =
        base::BindOnce(&FileSequenceState::OnRulesetReindexed,
                       weak_factory_.GetWeakPtr(), std::move(load_data),
                       std::move(ui_callback));
    source_copy.IndexAndPersistRules(connector_.get(),
                                     base::nullopt /* decoder_batch_id */,
                                     std::move(ruleset_reindexed_callback));
  }

 private:
  // Callback invoked when the JSON ruleset is reindexed.
  void OnRulesetReindexed(LoadRequestData load_data,
                          LoadRulesetUICallback ui_callback,
                          IndexAndPersistRulesResult result) const {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    // Only ruleset which can't be loaded are reindexed.
    DCHECK(!load_data.ruleset.did_load_successfully());

    // The checksum of the reindexed ruleset should have been the same as the
    // expected checksum obtained from prefs, in all cases except when the
    // ruleset version changes. If this is not the case, then there is some
    // other issue (like the JSON rules file has been modified from the one used
    // during installation or preferences are corrupted). But taking care of
    // these is beyond our scope here, so simply signal a failure.
    bool reindexing_success =
        result.success &&
        load_data.ruleset.expected_checksum() == result.ruleset_checksum;

    // In case of updates to the ruleset version, the change of ruleset checksum
    // is expected.
    if (result.success &&
        load_data.ruleset.load_ruleset_result() ==
            RulesetMatcher::LoadRulesetResult::kLoadErrorVersionMismatch) {
      load_data.ruleset.set_new_checksum(result.ruleset_checksum);
      // Also change the |expected_checksum| so that the subsequent load
      // succeeds.
      load_data.ruleset.set_expected_checksum(result.ruleset_checksum);
      reindexing_success = true;
    }

    UMA_HISTOGRAM_BOOLEAN(
        "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
        reindexing_success);

    // If the reindexing was successful, try to load the ruleset again.
    if (reindexing_success)
      load_data.ruleset.CreateVerifiedMatcher();

    // The UI thread will handle success or failure.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(std::move(ui_callback), std::move(load_data)));
  }

  const std::unique_ptr<service_manager::Connector> connector_;

  // Must be the last member variable. See WeakPtrFactory documentation for
  // details. Mutable to allow GetWeakPtr() usage from const methods.
  mutable base::WeakPtrFactory<FileSequenceState> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSequenceState);
};

// Helper to bridge tasks to FileSequenceState. Lives on the UI thread.
class RulesMonitorService::FileSequenceBridge {
 public:
  FileSequenceBridge()
      : file_task_runner_(GetExtensionFileTaskRunner()),
        file_sequence_state_(std::make_unique<FileSequenceState>()) {}

  ~FileSequenceBridge() {
    file_task_runner_->DeleteSoon(FROM_HERE, std::move(file_sequence_state_));
  }

  void LoadRuleset(
      LoadRequestData load_data,
      FileSequenceState::LoadRulesetUICallback load_ruleset_callback) const {
    // base::Unretained is safe here because we trigger the destruction of
    // |file_sequence_state_| on |file_task_runner_| from our destructor. Hence
    // it is guaranteed to be alive when |load_ruleset_task| is run.
    base::OnceClosure load_ruleset_task =
        base::BindOnce(&FileSequenceState::LoadRuleset,
                       base::Unretained(file_sequence_state_.get()),
                       std::move(load_data), std::move(load_ruleset_callback));
    file_task_runner_->PostTask(FROM_HERE, std::move(load_ruleset_task));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Created on the UI thread. Accessed and destroyed on |file_task_runner_|.
  // Maintains state needed on |file_task_runner_|.
  std::unique_ptr<FileSequenceState> file_sequence_state_;

  DISALLOW_COPY_AND_ASSIGN(FileSequenceBridge);
};

RulesMonitorService::RulesMonitorService(
    content::BrowserContext* browser_context)
    : registry_observer_(this),
      file_sequence_bridge_(std::make_unique<FileSequenceBridge>()),
      info_map_(ExtensionSystem::Get(browser_context)->info_map()),
      prefs_(ExtensionPrefs::Get(browser_context)),
      extension_registry_(ExtensionRegistry::Get(browser_context)),
      warning_service_(WarningService::Get(browser_context)),
      weak_factory_(this) {
  registry_observer_.Add(extension_registry_);
}

RulesMonitorService::~RulesMonitorService() = default;

/* Description of thread hops for various scenarios:
   On ruleset load success:
      UI -> File -> UI -> IO.
   On ruleset load failure:
      UI -> File -> UI.
   On ruleset unload:
      UI -> IO.
*/

void RulesMonitorService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!declarative_net_request::DNRManifestData::HasRuleset(*extension))
    return;

  DCHECK(IsAPIAvailable());

  int expected_ruleset_checksum;
  bool has_checksum = prefs_->GetDNRRulesetChecksum(extension->id(),
                                                    &expected_ruleset_checksum);
  DCHECK(has_checksum);

  RulesetInfo ruleset(RulesetSource::Create(*extension));
  ruleset.set_expected_checksum(expected_ruleset_checksum);

  LoadRequestData load_data(extension->id(), std::move(ruleset));

  FileSequenceState::LoadRulesetUICallback load_ruleset_callback =
      base::BindOnce(&RulesMonitorService::OnRulesetLoaded,
                     weak_factory_.GetWeakPtr());

  file_sequence_bridge_->LoadRuleset(std::move(load_data),
                                     std::move(load_ruleset_callback));
}

void RulesMonitorService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // Return early if the extension does not have an active indexed ruleset.
  if (!extensions_with_rulesets_.erase(extension->id()))
    return;

  DCHECK(IsAPIAvailable());

  base::OnceClosure unload_ruleset_on_io_task = base::BindOnce(
      &UnloadRulesetOnIOThread, extension->id(), base::RetainedRef(info_map_));
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           std::move(unload_ruleset_on_io_task));
}

void RulesMonitorService::OnRulesetLoaded(LoadRequestData load_data) {
  // Update the ruleset checksum if needed.
  if (load_data.ruleset.new_checksum()) {
    prefs_->SetDNRRulesetChecksum(load_data.extension_id,
                                  *load_data.ruleset.new_checksum());
  }

  // It's possible that the extension has been disabled since the initial load
  // ruleset request. If it's disabled, do nothing.
  if (!extension_registry_->enabled_extensions().Contains(
          load_data.extension_id))
    return;

  if (!load_data.ruleset.did_load_successfully()) {
    // The ruleset failed to load. Notify the user.
    warning_service_->AddWarnings(
        {Warning::CreateRulesetFailedToLoadWarning(load_data.extension_id)});
    return;
  }

  extensions_with_rulesets_.insert(load_data.extension_id);
  for (auto& observer : observers_)
    observer.OnRulesetLoaded();

  base::OnceClosure load_ruleset_on_io =
      base::BindOnce(&LoadRulesetOnIOThread, load_data.extension_id,
                     load_data.ruleset.TakeMatcher(),
                     prefs_->GetDNRAllowedPages(load_data.extension_id),
                     base::RetainedRef(info_map_));
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           std::move(load_ruleset_on_io));
}

}  // namespace declarative_net_request

template <>
void BrowserContextKeyedAPIFactory<
    declarative_net_request::RulesMonitorService>::
    DeclareFactoryDependencies() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(WarningServiceFactory::GetInstance());
}

}  // namespace extensions
