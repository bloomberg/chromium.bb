// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"

class GURL;

namespace safe_browsing {

// A request for checking if an unfamiliar login form or a password reuse event
// is safe. PasswordProtectionRequest objects are owned by
// PasswordProtectionService indicated by |password_protection_service_|.
// PasswordProtectionService is RefCountedThreadSafe such that it can post task
// safely between IO and UI threads. It can only be destroyed on UI thread.
//
// PasswordProtectionRequest flow:
// Step| Thread |                    Task
// (1) |   UI   | If incognito or !SBER, quit request.
// (2) |   UI   | Add task to IO thread for whitelist checking.
// (3) |   IO   | Check whitelist and return the result back to UI thread.
// (4) |   UI   | If whitelisted, check verdict cache; else quit request.
// (5) |   UI   | If verdict cached, quit request; else prepare request proto.
// (6) |   UI   | Start a timeout task, and send network request.
// (7) |   UI   | On receiving response, handle response and finish.
//     |        | On request timeout, cancel request.
//     |        | On deletion of |password_protection_service_|, cancel request.
class PasswordProtectionRequest : public base::RefCountedThreadSafe<
                                      PasswordProtectionRequest,
                                      content::BrowserThread::DeleteOnUIThread>,
                                  public net::URLFetcherDelegate {
 public:
  // The outcome of the request. These values are used for UMA.
  // DO NOT CHANGE THE ORDERING OF THESE VALUES.
  enum RequestOutcome {
    UNKNOWN = 0,
    SUCCEEDED = 1,
    CANCELED = 2,
    TIMEDOUT = 3,
    MATCHED_WHITELIST = 4,
    RESPONSE_ALREADY_CACHED = 5,
    DEPRECATED_NO_EXTENDED_REPORTING = 6,
    DEPRECATED_INCOGNITO = 7,
    REQUEST_MALFORMED = 8,
    FETCH_FAILED = 9,
    RESPONSE_MALFORMED = 10,
    SERVICE_DESTROYED = 11,
    MAX_OUTCOME
  };

  PasswordProtectionRequest(const GURL& main_frame_url,
                            const GURL& password_form_action,
                            const GURL& password_form_frame_url,
                            LoginReputationClientRequest::TriggerType type,
                            PasswordProtectionService* pps,
                            int request_timeout_in_ms);

  base::WeakPtr<PasswordProtectionRequest> GetWeakPtr() {
    return weakptr_factory_.GetWeakPtr();
  }

  // Starts processing request by checking extended reporting and incognito
  // conditions.
  void Start();

  // Cancels the current request. |timed_out| indicates if this cancellation is
  // due to timeout. This function will call Finish() to destroy |this|.
  void Cancel(bool timed_out);

  // net::URLFetcherDelegate override.
  // Processes the received response.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  GURL main_frame_url() const { return main_frame_url_; }

 private:
  friend class base::RefCountedThreadSafe<PasswordProtectionRequest>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<PasswordProtectionRequest>;
  ~PasswordProtectionRequest() override;

  void CheckWhitelistOnUIThread();

  // If |main_frame_url_| matches whitelist, call Finish() immediately;
  // otherwise call CheckCachedVerdicts(). It is the task posted back to UI
  // thread by the PostTaskAndReply() in CheckWhitelistOnUIThread().
  // |match_whitelist| boolean pointer is used to pass whitelist checking result
  // between UI and IO thread. The object it points to will be deleted at the
  // end of OnWhitelistCheckDone(), since base::Owned() transfers its ownership
  // to this callback function.
  void OnWhitelistCheckDone(const bool* match_whitelist);

  // Looks up cached verdicts. If verdict is already cached, call SendRequest();
  // otherwise call Finish().
  void CheckCachedVerdicts();

  // Fill |request_proto_| with appropriate values.
  void FillRequestProto();

  // Initiates network request to Safe Browsing backend.
  void SendRequest();

  // Start a timer to cancel the request if it takes too long.
  void StartTimeout();

  // |this| will be destroyed after calling this function.
  void Finish(RequestOutcome outcome,
              std::unique_ptr<LoginReputationClientResponse> response);

  // Main frame URL of the login form.
  const GURL main_frame_url_;

  // The action URL of the password form.
  const GURL password_form_action_;

  // Frame url of the detected password form.
  const GURL password_form_frame_url_;

  // If this request is for unfamiliar login page or for a password reuse event.
  const LoginReputationClientRequest::TriggerType request_type_;

  // When request is sent.
  base::TimeTicks request_start_time_;

  // URLFetcher instance for sending request and receiving response.
  std::unique_ptr<net::URLFetcher> fetcher_;

  // The PasswordProtectionService instance owns |this|.
  // Can only be accessed on UI thread.
  PasswordProtectionService* password_protection_service_;

  // Safe Browsing database manager used to look up CSD whitelist.
  // Can only be accessed on IO thread.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // If we haven't receive response after this period of time, we cancel this
  // request.
  const int request_timeout_in_ms_;

  std::unique_ptr<LoginReputationClientRequest> request_proto_;

  // Needed for canceling tasks posted to different threads.
  base::CancelableTaskTracker tracker_;

  base::WeakPtrFactory<PasswordProtectionRequest> weakptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionRequest);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_
