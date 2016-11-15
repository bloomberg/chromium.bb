// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/fake_web_history_service.h"

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/sync/protocol/history_status.pb.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request_context_getter.h"

namespace history {

// FakeRequest -----------------------------------------------------------------

namespace {

// TODO(msramek): Find a way to keep these URLs in sync with what is used
// in WebHistoryService.

const char kLookupUrl[] = "https://history.google.com/history/api/lookup";

const char kDeleteUrl[] = "https://history.google.com/history/api/delete";

const char kChromeClient[] = "chrome";

const char kWebAndAppClient[] = "web_app";

const char kSyncServerHost[] = "clients4.google.com";

class FakeRequest : public WebHistoryService::Request {
 public:
  FakeRequest(FakeWebHistoryService* service,
              const GURL& url,
              bool emulate_success,
              int emulate_response_code,
              const WebHistoryService::CompletionCallback& callback,
              base::Time begin,
              base::Time end,
              int max_count);

  // WebHistoryService::Request:
  bool IsPending() override;
  int GetResponseCode() override;
  const std::string& GetResponseBody() override;
  void SetPostData(const std::string& post_data) override;
  void SetPostDataAndType(const std::string& post_data,
                          const std::string& mime_type) override;
  void SetUserAgent(const std::string& user_agent) override;
  void Start() override;

 private:
  FakeWebHistoryService* service_;
  GURL url_;
  bool emulate_success_;
  int emulate_response_code_;
  const WebHistoryService::CompletionCallback& callback_;
  base::Time begin_;
  base::Time end_;
  int max_count_;
  bool is_pending_;
  std::string response_body_;

  DISALLOW_COPY_AND_ASSIGN(FakeRequest);
};

FakeRequest::FakeRequest(
    FakeWebHistoryService* service,
    const GURL& url,
    bool emulate_success,
    int emulate_response_code,
    const WebHistoryService::CompletionCallback& callback,
    base::Time begin,
    base::Time end,
    int max_count)
        : service_(service),
          url_(url),
          emulate_success_(emulate_success),
          emulate_response_code_(emulate_response_code),
          callback_(callback),
          begin_(begin),
          end_(end),
          max_count_(max_count),
          is_pending_(false) {
}

bool FakeRequest::IsPending() {
  return is_pending_;
}

int FakeRequest::GetResponseCode() {
  return emulate_response_code_;
}

const std::string& FakeRequest::GetResponseBody() {
  std::string client;
  net::GetValueForKeyInQuery(url_, "client", &client);

  GURL::Replacements remove_query;
  remove_query.ClearQuery();
  GURL base_url = url_.ReplaceComponents(remove_query);

  if (base_url == kLookupUrl && client == kChromeClient) {
    // History query.
    int count = service_->GetNumberOfVisitsBetween(begin_, end_);
    if (max_count_ && max_count_ < count)
      count = max_count_;

    response_body_ = "{ \"event\": [";
    for (int i = 0; i < count; ++i)
      response_body_ += i ? ", {}" : "{}";
    response_body_ += "] }";

  } else if (base_url == kDeleteUrl && client == kChromeClient) {
    // Deletion query.
    response_body_ = "{ \"just needs to be\" : \"a valid JSON.\" }";

  } else if (base_url == kLookupUrl && client == kWebAndAppClient) {
    // Web and app activity query.
    response_body_ = base::StringPrintf(
        "{ \"history_recording_enabled\": %s }",
        service_->IsWebAndAppActivityEnabled() ? "true" : "false");

  } else if (url_.host() == kSyncServerHost) {
    // Other forms of browsing history query.
    std::unique_ptr<sync_pb::HistoryStatusResponse> history_status(
        new sync_pb::HistoryStatusResponse());
    history_status->set_has_derived_data(
        service_->AreOtherFormsOfBrowsingHistoryPresent());
    history_status->SerializeToString(&response_body_);
  }

  return response_body_;
}

void FakeRequest::SetPostData(const std::string& post_data) {
  // Unused.
};

void FakeRequest::SetPostDataAndType(const std::string& post_data,
                                     const std::string& mime_type) {
  // Unused.
};

void FakeRequest::SetUserAgent(const std::string& user_agent) {
  // Unused.
};

void FakeRequest::Start() {
  is_pending_ = true;
  callback_.Run(this, emulate_success_);
}

}  // namespace

// FakeWebHistoryService -------------------------------------------------------

FakeWebHistoryService::FakeWebHistoryService(
    OAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : history::WebHistoryService(
          token_service,
          signin_manager,
          request_context) {
}

FakeWebHistoryService::~FakeWebHistoryService() {
}

void FakeWebHistoryService::SetupFakeResponse(
    bool emulate_success, int emulate_response_code) {
  emulate_success_ = emulate_success;
  emulate_response_code_ = emulate_response_code;
}

void FakeWebHistoryService::AddSyncedVisit(
    std::string url, base::Time timestamp) {
  visits_.push_back(make_pair(url, timestamp));
}

void FakeWebHistoryService::ClearSyncedVisits() {
  visits_.clear();
}

int FakeWebHistoryService::GetNumberOfVisitsBetween(
    const base::Time& begin, const base::Time& end) {
  int result = 0;
  for (const Visit& visit : visits_) {
    if (visit.second >= begin && visit.second < end)
      ++result;
  }
  return result;
}

base::Time FakeWebHistoryService::GetTimeForKeyInQuery(
    const GURL& url, const std::string& key) {
  std::string value;
  if (!net::GetValueForKeyInQuery(url, key, &value))
    return base::Time();

  int64_t us;
  if (!base::StringToInt64(value, &us))
     return base::Time();
  return base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(us);
}

FakeWebHistoryService::Request* FakeWebHistoryService::CreateRequest(
    const GURL& url, const CompletionCallback& callback) {
  // Find the time range endpoints in the URL.
  base::Time begin = GetTimeForKeyInQuery(url, "min");
  base::Time end = GetTimeForKeyInQuery(url, "max");

  if (end.is_null())
    end = base::Time::Max();

  int max_count = 0;
  std::string max_count_str;
  if (net::GetValueForKeyInQuery(url, "num", &max_count_str))
    base::StringToInt(max_count_str, &max_count);

  return new FakeRequest(this, url, emulate_success_, emulate_response_code_,
                         callback, begin, end, max_count);
}

bool FakeWebHistoryService::IsWebAndAppActivityEnabled() {
  return web_and_app_activity_enabled_;
}

void FakeWebHistoryService::SetWebAndAppActivityEnabled(bool enabled) {
  web_and_app_activity_enabled_ = enabled;
}

bool FakeWebHistoryService::AreOtherFormsOfBrowsingHistoryPresent() {
  return other_forms_of_browsing_history_present_;
}

void FakeWebHistoryService::SetOtherFormsOfBrowsingHistoryPresent(
    bool present) {
  other_forms_of_browsing_history_present_ = present;
}

}  // namespace history
