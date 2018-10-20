// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_ui_service.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "url/gurl.h"

namespace previews {

PreviewsUIService::PreviewsUIService(
    PreviewsDeciderImpl* previews_decider_impl,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    std::unique_ptr<blacklist::OptOutStore> previews_opt_out_store,
    std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
    const PreviewsIsEnabledCallback& is_enabled_callback,
    std::unique_ptr<PreviewsLogger> logger,
    blacklist::BlacklistData::AllowedTypesAndVersions allowed_previews,
    network::NetworkQualityTracker* network_quality_tracker)
    : io_task_runner_(io_task_runner),
      logger_(std::move(logger)),
      network_quality_tracker_(network_quality_tracker),
      weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(logger_);
  previews_decider_impl->Initialize(
      weak_factory_.GetWeakPtr(), std::move(previews_opt_out_store),
      std::move(previews_opt_guide), is_enabled_callback,
      std::move(allowed_previews));
  network_quality_tracker_->AddEffectiveConnectionTypeObserver(this);
}

PreviewsUIService::~PreviewsUIService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_quality_tracker_->RemoveEffectiveConnectionTypeObserver(this);
}

void PreviewsUIService::SetIOData(
    base::WeakPtr<PreviewsDeciderImpl> previews_decider_impl) {
  DCHECK(thread_checker_.CalledOnValidThread());
  previews_decider_impl_ = previews_decider_impl;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PreviewsDeciderImpl::SetEffectiveConnectionType,
                     previews_decider_impl_,
                     current_effective_connection_type_));
}

void PreviewsUIService::AddPreviewNavigation(const GURL& url,
                                             PreviewsType type,
                                             bool opt_out,
                                             uint64_t page_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PreviewsDeciderImpl::AddPreviewNavigation,
                     previews_decider_impl_, url, opt_out, type, page_id));
}

void PreviewsUIService::LogPreviewNavigation(const GURL& url,
                                             PreviewsType type,
                                             bool opt_out,
                                             base::Time time,
                                             uint64_t page_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  logger_->LogPreviewNavigation(url, type, opt_out, time, page_id);
}

void PreviewsUIService::LogPreviewDecisionMade(
    PreviewsEligibilityReason reason,
    const GURL& url,
    base::Time time,
    PreviewsType type,
    std::vector<PreviewsEligibilityReason>&& passed_reasons,
    uint64_t page_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  logger_->LogPreviewDecisionMade(reason, url, time, type,
                                  std::move(passed_reasons), page_id);
}

void PreviewsUIService::OnNewBlacklistedHost(const std::string& host,
                                             base::Time time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  logger_->OnNewBlacklistedHost(host, time);
}

void PreviewsUIService::OnUserBlacklistedStatusChange(bool blacklisted) {
  DCHECK(thread_checker_.CalledOnValidThread());
  logger_->OnUserBlacklistedStatusChange(blacklisted);
}

void PreviewsUIService::OnBlacklistCleared(base::Time time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  logger_->OnBlacklistCleared(time);
}

void PreviewsUIService::SetIgnorePreviewsBlacklistDecision(bool ignored) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PreviewsDeciderImpl::SetIgnorePreviewsBlacklistDecision,
                     previews_decider_impl_, ignored));
}

void PreviewsUIService::OnIgnoreBlacklistDecisionStatusChanged(bool ignored) {
  DCHECK(thread_checker_.CalledOnValidThread());
  logger_->OnIgnoreBlacklistDecisionStatusChanged(ignored);
}

void PreviewsUIService::SetResourceLoadingHintsResourcePatternsToBlock(
    const GURL& document_gurl,
    const std::vector<std::string>& patterns) {
  DCHECK(thread_checker_.CalledOnValidThread());
  resource_loading_hints_document_gurl_ = document_gurl;
  resource_loading_hints_patterns_to_block_ = patterns;
}

std::vector<std::string>
PreviewsUIService::GetResourceLoadingHintsResourcePatternsToBlock(
    const GURL& document_gurl) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(tbansal): https://crbug.com/856243. Read patterns from the proto
  // optimizations file from the disk, and populate the return value.
  if (document_gurl != resource_loading_hints_document_gurl_)
    return std::vector<std::string>();
  return resource_loading_hints_patterns_to_block_;
}

PreviewsLogger* PreviewsUIService::previews_logger() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return logger_.get();
}

// When triggering previews, prevent long term black list rules.
void PreviewsUIService::SetIgnoreLongTermBlackListForServerPreviews(
    bool ignore_long_term_black_list_rules_allowed) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PreviewsDeciderImpl::SetIgnoreLongTermBlackListForServerPreviews,
          previews_decider_impl_, ignore_long_term_black_list_rules_allowed));
}

void PreviewsUIService::ClearBlackList(base::Time begin_time,
                                       base::Time end_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PreviewsDeciderImpl::ClearBlackList,
                                previews_decider_impl_, begin_time, end_time));
}

void PreviewsUIService::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  current_effective_connection_type_ = type;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PreviewsDeciderImpl::SetEffectiveConnectionType,
                     previews_decider_impl_, type));
}

}  // namespace previews
