// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autocheckout/whitelist_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/browser/browser_context.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace {

// Back off in seconds after each whitelist download is attempted.
const int kDownloadIntervalSeconds = 86400;  // 1 day

// The delay in seconds after startup before download whitelist. This helps
// to reduce contention at startup time.
const int kInitialDownloadDelaySeconds = 3;

const net::BackoffEntry::Policy kBackoffPolicy = {
  // Number of initial errors to ignore before starting to back off.
  0,

  // Initial delay in ms: 3 seconds.
  3000,

  // Factor by which the waiting time is multiplied.
  6,

  // Fuzzing percentage: no fuzzing logic.
  0,

  // Maximum delay in ms: 1 hour.
  1000 * 60 * 60,

  // When to discard an entry: 3 hours.
  1000 * 60 * 60 * 3,

  // |always_use_initial_delay|; false means that the initial delay is
  // applied after the first error, and starts backing off from there.
  false,
};

const char kDefaultWhitelistUrl[] =
    "https://www.gstatic.com/commerce/autocheckout/whitelist.csv";

const char kWhiteListKeyName[] = "autocheckout_whitelist_manager";

std::string GetWhitelistUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string whitelist_url = command_line.GetSwitchValueASCII(
      autofill::switches::kAutocheckoutWhitelistUrl);

  return whitelist_url.empty() ? kDefaultWhitelistUrl : whitelist_url;
}

} //  namespace


namespace autofill {
namespace autocheckout {

WhitelistManager::WhitelistManager()
    : callback_is_pending_(false),
      experimental_form_filling_enabled_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableExperimentalFormFilling) ||
          base::FieldTrialList::FindFullName("Autocheckout") == "Yes"),
      bypass_autocheckout_whitelist_(
          CommandLine::ForCurrentProcess()->HasSwitch(
                        switches::kBypassAutocheckoutWhitelist)),
      retry_entry_(&kBackoffPolicy) {
}

WhitelistManager::~WhitelistManager() {}

void WhitelistManager::Init(net::URLRequestContextGetter* context_getter) {
  DCHECK(context_getter);
  context_getter_ = context_getter;
  ScheduleDownload(base::TimeDelta::FromSeconds(kInitialDownloadDelaySeconds));
}

void WhitelistManager::ScheduleDownload(base::TimeDelta interval) {
  if (!experimental_form_filling_enabled_) {
    // The feature is not enabled: do not do the request.
    return;
  }
  if (download_timer_.IsRunning() || callback_is_pending_) {
    // A download activity is already scheduled or happening.
    return;
  }
  StartDownloadTimer(interval);
}

void WhitelistManager::StartDownloadTimer(base::TimeDelta interval) {
  download_timer_.Start(FROM_HERE,
                        interval,
                        this,
                        &WhitelistManager::TriggerDownload);
}

const AutofillMetrics& WhitelistManager::GetMetricLogger() const {
  return metrics_logger_;
}

void WhitelistManager::TriggerDownload() {
  callback_is_pending_ = true;

  request_started_timestamp_ = base::Time::Now();

  request_.reset(net::URLFetcher::Create(
      0, GURL(GetWhitelistUrl()), net::URLFetcher::GET, this));
  request_->SetRequestContext(context_getter_);
  request_->SetAutomaticallyRetryOn5xx(false);
  request_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                         net::LOAD_DO_NOT_SEND_COOKIES);
  request_->Start();
}

void WhitelistManager::StopDownloadTimer() {
  download_timer_.Stop();
  callback_is_pending_ = false;
}

void WhitelistManager::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(callback_is_pending_);
  callback_is_pending_ = false;
  scoped_ptr<net::URLFetcher> old_request = request_.Pass();
  DCHECK_EQ(source, old_request.get());

  AutofillMetrics::AutocheckoutWhitelistDownloadStatus status;
  base::TimeDelta duration = base::Time::Now() - request_started_timestamp_;

  // Refresh the whitelist after kDownloadIntervalSeconds (24 hours).
  base::TimeDelta next_download_time =
      base::TimeDelta::FromSeconds(kDownloadIntervalSeconds);

  if (source->GetResponseCode() == net::HTTP_OK) {
    std::string data;
    source->GetResponseAsString(&data);
    BuildWhitelist(data);
    status = AutofillMetrics::AUTOCHECKOUT_WHITELIST_DOWNLOAD_SUCCEEDED;
    retry_entry_.Reset();
  } else {
    status = AutofillMetrics::AUTOCHECKOUT_WHITELIST_DOWNLOAD_FAILED;
    retry_entry_.InformOfRequest(false);
    if (!retry_entry_.CanDiscard())
      next_download_time = retry_entry_.GetTimeUntilRelease();
  }

  GetMetricLogger().LogAutocheckoutWhitelistDownloadDuration(duration, status);
  ScheduleDownload(next_download_time);
}

std::string WhitelistManager::GetMatchedURLPrefix(const GURL& url) const {
  if (!experimental_form_filling_enabled_ || url.is_empty())
    return std::string();

  for (std::vector<std::string>::const_iterator it = url_prefixes_.begin();
      it != url_prefixes_.end(); ++it) {
    // This is only for ~20 sites initially, liner search is sufficient.
    // TODO(benquan): Look for optimization options when we support
    // more sites.
    if (StartsWithASCII(url.spec(), *it, true)) {
      DVLOG(1) << "WhitelistManager matched URLPrefix: " << *it;
      return *it;
    }
  }
  return bypass_autocheckout_whitelist_ ? url.spec() : std::string();
}

void WhitelistManager::BuildWhitelist(const std::string& data) {
  std::vector<std::string> new_url_prefixes;

  std::vector<std::string> lines;
  base::SplitString(data, '\n', &lines);

  for (std::vector<std::string>::const_iterator line = lines.begin();
      line != lines.end(); ++line) {
    if (!line->empty()) {
      std::vector<std::string> fields;
      base::SplitString(*line, ',', &fields);
      // Currently we have only one column in the whitelist file, if we decide
      // to add more metadata as additional columns, previous versions of
      // Chrome can ignore them and continue to work.
      if (!fields[0].empty())
        new_url_prefixes.push_back(fields[0]);
    }
  }
  url_prefixes_ = new_url_prefixes;
}

}  // namespace autocheckout
}  // namespace autofill
