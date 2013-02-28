// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autocheckout/whitelist_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_context.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

// Back off in seconds after each whitelist download is attempted.
const int kDownloadIntervalSeconds = 86400;  // 1 day

// The delay in seconds after startup before download whitelist. This helps
// to reduce contention at startup time.
const int kInitialDownloadDelaySeconds = 3;

const char kWhitelistUrl[] =
    "http://www.gstatic.com/commerce/autocheckout/whitelist.csv";

const char kWhiteListKeyName[] = "autocheckout_whitelist_manager";

} //  namespace


namespace autofill {
namespace autocheckout {

// static
WhitelistManager* WhitelistManager::GetForBrowserContext(
    content::BrowserContext* context) {
  WhitelistManager* whitelist_manager = static_cast<WhitelistManager*>(
      context->GetUserData(kWhiteListKeyName));
  if (!whitelist_manager) {
    whitelist_manager =
        new WhitelistManager(context->GetRequestContext());
    whitelist_manager->ScheduleDownload(kInitialDownloadDelaySeconds);
    context->SetUserData(kWhiteListKeyName, whitelist_manager);
  }
  return whitelist_manager;
}

WhitelistManager::WhitelistManager(
    net::URLRequestContextGetter* context_getter)
    : callback_is_pending_(false),
      context_getter_(context_getter),
      experimental_form_filling_enabled_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableExperimentalFormFilling)){
  DCHECK(context_getter);
}

WhitelistManager::~WhitelistManager() {}

void WhitelistManager::ScheduleDownload(size_t interval_seconds) {
  if (!experimental_form_filling_enabled_) {
    // The feature is not enabled: do not do the request.
    return;
  }
  if (download_timer_.IsRunning() || callback_is_pending_) {
    // A download activity is already scheduled or happening.
    return;
  }
  StartDownloadTimer(interval_seconds);
}

void WhitelistManager::StartDownloadTimer(size_t interval_seconds) {
  download_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromSeconds(interval_seconds),
                        this,
                        &WhitelistManager::TriggerDownload);
}

void WhitelistManager::TriggerDownload() {
  callback_is_pending_ = true;

  request_.reset(net::URLFetcher::Create(
      0, GURL(kWhitelistUrl), net::URLFetcher::GET, this));
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

  if (source->GetResponseCode() == net::HTTP_OK) {
    std::string data;
    source->GetResponseAsString(&data);
    BuildWhitelist(data);
  }

  ScheduleDownload(kDownloadIntervalSeconds);
}

std::string WhitelistManager::GetMatchedURLPrefix(const GURL& url) const {
  if (!experimental_form_filling_enabled_ || url.is_empty())
    return std::string();

  for (std::vector<std::string>::const_iterator it = url_prefixes_.begin();
      it != url_prefixes_.end(); ++it) {
    // This is only for ~20 sites initially, liner search is sufficient.
    // TODO(benquan): Look for optimization options when we support
    // more sites.
    if (StartsWithASCII(url.spec(), *it, true))
      return *it;
  }
  return std::string();
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

