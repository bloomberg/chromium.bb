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
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"
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

// Helper to bridge tasks to FileSequenceHelper. Lives on the UI thread.
class RulesMonitorService::FileSequenceBridge {
 public:
  FileSequenceBridge()
      : file_task_runner_(GetExtensionFileTaskRunner()),
        file_sequence_helper_(std::make_unique<FileSequenceHelper>()) {}

  ~FileSequenceBridge() {
    file_task_runner_->DeleteSoon(FROM_HERE, std::move(file_sequence_helper_));
  }

  void LoadRulesets(
      LoadRequestData load_data,
      FileSequenceHelper::LoadRulesetsUICallback ui_callback) const {
    // base::Unretained is safe here because we trigger the destruction of
    // |file_sequence_helper_| on |file_task_runner_| from our destructor. Hence
    // it is guaranteed to be alive when |load_ruleset_task| is run.
    base::OnceClosure load_ruleset_task =
        base::BindOnce(&FileSequenceHelper::LoadRulesets,
                       base::Unretained(file_sequence_helper_.get()),
                       std::move(load_data), std::move(ui_callback));
    file_task_runner_->PostTask(FROM_HERE, std::move(load_ruleset_task));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Created on the UI thread. Accessed and destroyed on |file_task_runner_|.
  // Maintains state needed on |file_task_runner_|.
  std::unique_ptr<FileSequenceHelper> file_sequence_helper_;

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

  // TODO(crbug.com/930961): Currently we only support a single ruleset per
  // extension.
  LoadRequestData load_data(extension->id());
  load_data.rulesets.push_back(std::move(ruleset));

  auto load_ruleset_callback = base::BindOnce(
      &RulesMonitorService::OnRulesetLoaded, weak_factory_.GetWeakPtr());
  file_sequence_bridge_->LoadRulesets(std::move(load_data),
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
  // TODO(crbug.com/930961): Currently we only support a single ruleset per
  // extension.
  DCHECK_EQ(1u, load_data.rulesets.size());
  RulesetInfo& ruleset = load_data.rulesets[0];

  // Update the ruleset checksums if needed.
  if (ruleset.new_checksum()) {
    prefs_->SetDNRRulesetChecksum(load_data.extension_id,
                                  *(ruleset.new_checksum()));
  }

  // It's possible that the extension has been disabled since the initial load
  // ruleset request. If it's disabled, do nothing.
  if (!extension_registry_->enabled_extensions().Contains(
          load_data.extension_id))
    return;

  if (!ruleset.did_load_successfully()) {
    // The ruleset failed to load. Notify the user.
    warning_service_->AddWarnings(
        {Warning::CreateRulesetFailedToLoadWarning(load_data.extension_id)});
    return;
  }

  extensions_with_rulesets_.insert(load_data.extension_id);
  for (auto& observer : observers_)
    observer.OnRulesetLoaded();

  base::OnceClosure load_ruleset_on_io = base::BindOnce(
      &LoadRulesetOnIOThread, load_data.extension_id, ruleset.TakeMatcher(),
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
