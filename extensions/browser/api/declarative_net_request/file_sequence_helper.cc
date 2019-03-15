// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "services/service_manager/public/cpp/connector.h"

namespace extensions {
namespace declarative_net_request {

namespace {

// A class to help in re-indexing multiple rulesets.
class ReindexHelper {
 public:
  // Starts re-indexing rulesets. Must be called on the extension file task
  // runner.
  using ReindexCallback = base::OnceCallback<void(LoadRequestData)>;
  static void Start(service_manager::Connector* connector,
                    LoadRequestData data,
                    ReindexCallback callback) {
    auto* helper = new ReindexHelper(std::move(data), std::move(callback));
    helper->Start(connector);
  }

 private:
  // We manage our own lifetime.
  ReindexHelper(LoadRequestData data, ReindexCallback callback)
      : data_(std::move(data)), callback_(std::move(callback)) {}
  ~ReindexHelper() = default;

  void Start(service_manager::Connector* connector) {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    base::Token token = base::Token::CreateRandom();

    // Post tasks to reindex individual rulesets.
    bool did_post_task = false;
    for (auto& ruleset : data_.rulesets) {
      if (ruleset.did_load_successfully())
        continue;

      // Using Unretained is safe since this class manages its own lifetime and
      // |this| won't be deleted until the |callback| returns.
      auto callback = base::BindOnce(&ReindexHelper::OnReindexCompleted,
                                     base::Unretained(this), &ruleset);
      callback_count_++;
      did_post_task = true;
      ruleset.source().IndexAndPersistRules(connector, token,
                                            std::move(callback));
    }

    // It's possible that the callbacks return synchronously and we are deleted
    // at this point. Hence don't use any member variables here. Also, if we
    // don't post any task, we'll leak. Ensure that's not the case.
    DCHECK(did_post_task);
  }

  // Callback invoked when a single ruleset is re-indexed.
  void OnReindexCompleted(RulesetInfo* ruleset,
                          IndexAndPersistRulesResult result) {
    DCHECK(ruleset);

    // The checksum of the reindexed ruleset should have been the same as the
    // expected checksum obtained from prefs, in all cases except when the
    // ruleset version changes. If this is not the case, then there is some
    // other issue (like the JSON rules file has been modified from the one used
    // during installation or preferences are corrupted). But taking care of
    // these is beyond our scope here, so simply signal a failure.
    bool reindexing_success = result.success && ruleset->expected_checksum() ==
                                                    result.ruleset_checksum;

    // In case of updates to the ruleset version, the change of ruleset checksum
    // is expected.
    if (result.success &&
        ruleset->load_ruleset_result() ==
            RulesetMatcher::LoadRulesetResult::kLoadErrorVersionMismatch) {
      ruleset->set_new_checksum(result.ruleset_checksum);

      // Also change the |expected_checksum| so that any subsequent load
      // succeeds.
      ruleset->set_expected_checksum(result.ruleset_checksum);
      reindexing_success = true;
    }

    ruleset->set_reindexing_successful(reindexing_success);

    // TODO(karandeepb): Update this histogram once we start supporting multiple
    // rulesets per extension.
    UMA_HISTOGRAM_BOOLEAN(
        "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
        reindexing_success);

    callback_count_--;
    DCHECK_GE(callback_count_, 0);

    if (callback_count_ == 0) {
      // Our job is done.
      std::move(callback_).Run(std::move(data_));
      delete this;
    }
  }

  LoadRequestData data_;
  ReindexCallback callback_;
  int callback_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ReindexHelper);
};

}  // namespace

RulesetInfo::RulesetInfo(RulesetSource source) : source_(std::move(source)) {}
RulesetInfo::~RulesetInfo() = default;
RulesetInfo::RulesetInfo(RulesetInfo&&) = default;
RulesetInfo& RulesetInfo::operator=(RulesetInfo&&) = default;

std::unique_ptr<RulesetMatcher> RulesetInfo::TakeMatcher() {
  DCHECK(did_load_successfully());
  return std::move(matcher_);
}

RulesetMatcher::LoadRulesetResult RulesetInfo::load_ruleset_result() const {
  DCHECK(load_ruleset_result_);
  // |matcher_| is valid only on success.
  DCHECK_EQ(load_ruleset_result_ == RulesetMatcher::kLoadSuccess, !!matcher_);
  return *load_ruleset_result_;
}

void RulesetInfo::CreateVerifiedMatcher() {
  DCHECK(expected_checksum_);
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  load_ruleset_result_ = RulesetMatcher::CreateVerifiedMatcher(
      source_, *expected_checksum_, &matcher_);

  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.DeclarativeNetRequest.LoadRulesetResult",
      load_ruleset_result(), RulesetMatcher::kLoadResultMax);
}

LoadRequestData::LoadRequestData(ExtensionId extension_id)
    : extension_id(std::move(extension_id)) {}
LoadRequestData::~LoadRequestData() = default;
LoadRequestData::LoadRequestData(LoadRequestData&&) = default;
LoadRequestData& LoadRequestData::operator=(LoadRequestData&&) = default;

FileSequenceHelper::FileSequenceHelper()
    : connector_(content::ServiceManagerConnection::GetForProcess()
                     ->GetConnector()
                     ->Clone()),
      weak_factory_(this) {}

FileSequenceHelper::~FileSequenceHelper() {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
}

void FileSequenceHelper::LoadRulesets(
    LoadRequestData load_data,
    LoadRulesetsUICallback ui_callback) const {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!load_data.rulesets.empty());

  bool success = true;
  for (auto& ruleset : load_data.rulesets) {
    ruleset.CreateVerifiedMatcher();
    success &= ruleset.did_load_successfully();
  }

  if (success) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(std::move(ui_callback), std::move(load_data)));
    return;
  }

  // Loading one or more rulesets failed. Re-index them.

  // Using a WeakPtr is safe since |reindex_callback| will be called on this
  // sequence itself.
  auto reindex_callback =
      base::BindOnce(&FileSequenceHelper::OnRulesetsReindexed,
                     weak_factory_.GetWeakPtr(), std::move(ui_callback));
  ReindexHelper::Start(connector_.get(), std::move(load_data),
                       std::move(reindex_callback));
}

void FileSequenceHelper::OnRulesetsReindexed(LoadRulesetsUICallback ui_callback,
                                             LoadRequestData load_data) const {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  // Load rulesets for which reindexing succeeded.
  for (auto& ruleset : load_data.rulesets) {
    if (ruleset.reindexing_successful().value_or(false)) {
      // Only rulesets which can't be loaded are re-indexed.
      DCHECK(!ruleset.did_load_successfully());
      ruleset.CreateVerifiedMatcher();
    }
  }

  // The UI thread will handle success or failure.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(std::move(ui_callback), std::move(load_data)));
}

}  // namespace declarative_net_request
}  // namespace extensions
