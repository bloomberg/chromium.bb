// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_io_data.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_clock.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_opt_out_store.h"
#include "components/previews/core/previews_ui_service.h"
#include "net/base/load_flags.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace previews {

namespace {

void LogPreviewsEligibilityReason(PreviewsEligibilityReason status,
                                  PreviewsType type) {
  int32_t max_limit = static_cast<int32_t>(PreviewsEligibilityReason::LAST);
  base::LinearHistogram::FactoryGet(
      base::StringPrintf("Previews.EligibilityReason.%s",
                         GetStringNameForType(type).c_str()),
      1, max_limit, max_limit + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(static_cast<int>(status));
}

bool AllowedOnReload(PreviewsType type) {
  switch (type) {
    // These types return new content on refresh.
    case PreviewsType::LITE_PAGE:
    case PreviewsType::LOFI:
    case PreviewsType::AMP_REDIRECTION:
      return true;
    // Loading these types will always be stale when refreshed.
    case PreviewsType::OFFLINE:
      return false;
    case PreviewsType::NONE:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return false;
}

}  // namespace

PreviewsIOData::PreviewsIOData(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : ui_task_runner_(ui_task_runner),
      io_task_runner_(io_task_runner),
      weak_factory_(this) {}

PreviewsIOData::~PreviewsIOData() {}

void PreviewsIOData::Initialize(
    base::WeakPtr<PreviewsUIService> previews_ui_service,
    std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store,
    const PreviewsIsEnabledCallback& is_enabled_callback) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  is_enabled_callback_ = is_enabled_callback;
  previews_ui_service_ = previews_ui_service;

  // Set up the IO thread portion of |this|.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PreviewsIOData::InitializeOnIOThread, base::Unretained(this),
                 base::Passed(&previews_opt_out_store)));
}

void PreviewsIOData::InitializeOnIOThread(
    std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  previews_black_list_.reset(
      new PreviewsBlackList(std::move(previews_opt_out_store),
                            base::MakeUnique<base::DefaultClock>()));
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PreviewsUIService::SetIOData, previews_ui_service_,
                            weak_factory_.GetWeakPtr()));
}

void PreviewsIOData::LogPreviewNavigation(const GURL& url,
                                          bool opt_out,
                                          PreviewsType type,
                                          base::Time time) {
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PreviewsUIService::LogPreviewNavigation,
                            previews_ui_service_, url, type, opt_out, time));
}

void PreviewsIOData::AddPreviewNavigation(const GURL& url,
                                          bool opt_out,
                                          PreviewsType type) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  base::Time time =
      previews_black_list_->AddPreviewNavigation(url, opt_out, type);
  LogPreviewNavigation(url, opt_out, type, time);
}

void PreviewsIOData::ClearBlackList(base::Time begin_time,
                                    base::Time end_time) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  previews_black_list_->ClearBlackList(begin_time, end_time);
}

bool PreviewsIOData::ShouldAllowPreview(const net::URLRequest& request,
                                        PreviewsType type) const {
  return ShouldAllowPreviewAtECT(request, type,
                                 params::GetECTThresholdForPreview(type),
                                 std::vector<std::string>());
}

bool PreviewsIOData::ShouldAllowPreviewAtECT(
    const net::URLRequest& request,
    PreviewsType type,
    net::EffectiveConnectionType effective_connection_type_threshold,
    const std::vector<std::string>& host_blacklist_from_server) const {
  if (!request.url().has_host()) {
    // Don't capture UMA on this case, as it is not important and can happen
    // when navigating to files on disk, etc.
    return false;
  }
  if (is_enabled_callback_.is_null() || !previews_black_list_) {
    LogPreviewsEligibilityReason(
        PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE, type);
    return false;
  }
  if (!is_enabled_callback_.Run(type))
    return false;

  // The blacklist will disallow certain hosts for periods of time based on
  // user's opting out of the preview.
  PreviewsEligibilityReason status =
      previews_black_list_->IsLoadedAndAllowed(request.url(), type);
  if (status != PreviewsEligibilityReason::ALLOWED) {
    LogPreviewsEligibilityReason(status, type);
    return false;
  }

  if (effective_connection_type_threshold !=
      net::EFFECTIVE_CONNECTION_TYPE_LAST) {
    net::NetworkQualityEstimator* network_quality_estimator =
        request.context()->network_quality_estimator();
    if (!network_quality_estimator ||
        network_quality_estimator->GetEffectiveConnectionType() <
            net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
      LogPreviewsEligibilityReason(
          PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE, type);
      return false;
    }

    if (network_quality_estimator->GetEffectiveConnectionType() >
        effective_connection_type_threshold) {
      LogPreviewsEligibilityReason(PreviewsEligibilityReason::NETWORK_NOT_SLOW,
                                   type);
      return false;
    }
  }

  // LOAD_VALIDATE_CACHE or LOAD_BYPASS_CACHE mean the user reloaded the page.
  // If this is a query for offline previews, reloads should be disallowed.
  if (!AllowedOnReload(type) &&
      request.load_flags() &
          (net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE)) {
    LogPreviewsEligibilityReason(PreviewsEligibilityReason::RELOAD_DISALLOWED,
                                 type);
    return false;
  }

  if (std::find(host_blacklist_from_server.begin(),
                host_blacklist_from_server.end(), request.url().host_piece()) !=
      host_blacklist_from_server.end()) {
    LogPreviewsEligibilityReason(
        PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER, type);
    return false;
  }

  LogPreviewsEligibilityReason(PreviewsEligibilityReason::ALLOWED, type);
  return true;
}

}  // namespace previews
