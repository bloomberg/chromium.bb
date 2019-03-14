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

LoadRequestData::LoadRequestData(ExtensionId extension_id, RulesetInfo ruleset)
    : extension_id(std::move(extension_id)), ruleset(std::move(ruleset)) {}
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

void FileSequenceHelper::LoadRuleset(LoadRequestData load_data,
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
      base::BindOnce(&FileSequenceHelper::OnRulesetReindexed,
                     weak_factory_.GetWeakPtr(), std::move(load_data),
                     std::move(ui_callback));
  source_copy.IndexAndPersistRules(connector_.get(),
                                   base::nullopt /* decoder_batch_id */,
                                   std::move(ruleset_reindexed_callback));
}

void FileSequenceHelper::OnRulesetReindexed(
    LoadRequestData load_data,
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

}  // namespace declarative_net_request
}  // namespace extensions
