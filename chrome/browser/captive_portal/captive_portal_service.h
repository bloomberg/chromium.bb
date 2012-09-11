// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_SERVICE_H_
#define CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_SERVICE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/api/prefs/pref_member.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "googleurl/src/gurl.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;

namespace net {
class URLFetcher;
}

namespace captive_portal {

// Possible results of an attempt to detect a captive portal.
enum Result {
  // There's a confirmed connection to the Internet.
  RESULT_INTERNET_CONNECTED,
  // The URL request received a network or HTTP error, or a non-HTTP response.
  RESULT_NO_RESPONSE,
  // The URL request apparently encountered a captive portal.  It received a
  // a valid HTTP response with a 2xx, 3xx, or 511 status code, other than 204.
  RESULT_BEHIND_CAPTIVE_PORTAL,
  RESULT_COUNT
};

// Service that checks for captive portals when queried, and sends a
// NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT with the Profile as the source and
// a CaptivePortalService::Results as the details.
//
// Captive portal checks are rate-limited.  The CaptivePortalService may only
// be accessed on the UI thread.
// Design doc: https://docs.google.com/document/d/1k-gP2sswzYNvryu9NcgN7q5XrsMlUdlUdoW9WRaEmfM/edit
class CaptivePortalService : public ProfileKeyedService,
                             public net::URLFetcherDelegate,
                             public content::NotificationObserver,
                             public base::NonThreadSafe {
 public:
  enum TestingState {
    NOT_TESTING,
    DISABLED_FOR_TESTING,  // The service is always disabled.
    SKIP_OS_CHECK_FOR_TESTING  // The service can be enabled even if the OS has
                               // native captive portal detection.
  };

  // The details sent via a NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT.
  struct Results {
    // The result of the second most recent captive portal check.
    Result previous_result;
    // The result of the most recent captive portal check.
    Result result;
  };

  explicit CaptivePortalService(Profile* profile);
  virtual ~CaptivePortalService();

  // Triggers a check for a captive portal.  If there's already a check in
  // progress, does nothing.  Throttles the rate at which requests are sent.
  // Always sends the result notification asynchronously.
  void DetectCaptivePortal();

  // Returns the URL used for captive portal testing.  When a captive portal is
  // detected, this URL will take us to the captive portal landing page.
  const GURL& test_url() const { return test_url_; }

  // Result of the most recent captive portal check.
  Result last_detection_result() const { return last_detection_result_; }

  // Whether or not the CaptivePortalService is enabled.  When disabled, all
  // checks return INTERNET_CONNECTED.
  bool enabled() const { return enabled_; }

  // Used to disable captive portal detection so it doesn't interfere with
  // tests.  Should be called before the service is created.
  static void set_state_for_testing(TestingState testing_state) {
    testing_state_ = testing_state;
  }
  static TestingState get_state_for_testing() { return testing_state_; }

 private:
  friend class CaptivePortalServiceTest;
  friend class CaptivePortalBrowserTest;

  // Subclass of BackoffEntry that uses the CaptivePortalService's
  // GetCurrentTime function, for unit testing.
  class RecheckBackoffEntry;

  enum State {
    // No check is running or pending.
    STATE_IDLE,
    // The timer to check for a captive portal is running.
    STATE_TIMER_RUNNING,
    // There's an outstanding HTTP request to check for a captive portal.
    STATE_CHECKING_FOR_PORTAL,
  };

  // Contains all the information about the minimum time allowed between two
  // consecutive captive portal checks.
  struct RecheckPolicy {
    // Constructor initializes all values to defaults.
    RecheckPolicy();

    // The minimum amount of time between two captive portal checks, when the
    // last check found no captive portal.
    int initial_backoff_no_portal_ms;

    // The minimum amount of time between two captive portal checks, when the
    // last check found a captive portal.  This is expected to be less than
    // |initial_backoff_no_portal_ms|.  Also used when the service is disabled.
    int initial_backoff_portal_ms;

    net::BackoffEntry::Policy backoff_policy;
  };

  // Initiates a captive portal check, without any throttling.  If the service
  // is disabled, just acts like there's an Internet connection.
  void DetectCaptivePortalInternal();

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

  // ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  // Called when a captive portal check completes.  Passes the result to all
  // observers.
  void OnResult(Result result);

  // Updates BackoffEntry::Policy and creates a new BackoffEntry, which
  // resets the count used for throttling.
  void ResetBackoffEntry(Result result);

  // Updates |enabled_| based on command line flags and Profile preferences,
  // and sets |state_| to STATE_NONE if it's false.
  // TODO(mmenke): Figure out on which platforms, if any, should not use
  //               automatic captive portal detection.  Currently it's enabled
  //               on all platforms, though this code is not compiled on
  //               Android, since it lacks the Browser class.
  void UpdateEnabledState();

  // Takes a net::URLFetcher that has finished trying to retrieve the test
  // URL, and returns a captive_portal::Result based on its result.
  // If the response is a 503 with a Retry-After header, |retry_after| is
  // populated accordingly.  Otherwise, it's set to base::TimeDelta().
  Result GetCaptivePortalResultFromResponse(const net::URLFetcher* url_fetcher,
                                            base::TimeDelta* retry_after) const;

  // Returns the current time.  Used only when determining time until a
  // Retry-After date.  Overridden by unit tests.
  virtual base::Time GetCurrentTime() const;

  // Returns the current TimeTicks.  Overridden by unit tests.
  virtual base::TimeTicks GetCurrentTimeTicks() const;

  // Returns true if a captive portal check is currently running.
  bool FetchingURL() const;

  // Returns true if the timer to try and detect a captive portal is running.
  bool TimerRunning() const;

  State state() const { return state_; }

  RecheckPolicy& recheck_policy() { return recheck_policy_; }

  void set_test_url(const GURL& test_url) { test_url_ = test_url; }

  // The profile that owns this CaptivePortalService.
  Profile* profile_;

  State state_;

  // True if the service is enabled.  When not enabled, all checks will return
  // RESULT_INTERNET_CONNECTED.
  bool enabled_;

  // The result of the most recent captive portal check.
  Result last_detection_result_;

  // Number of sequential checks with the same captive portal result.
  int num_checks_with_same_result_;

  // Time when |last_detection_result_| was first received.
  base::TimeTicks first_check_time_with_same_result_;

  // Time the last captive portal check completed.
  base::TimeTicks last_check_time_;

  // Policy for throttling portal checks.
  RecheckPolicy recheck_policy_;

  // Implements behavior needed by |recheck_policy_|.  Whenever there's a new
  // captive_portal::Result, BackoffEntry::Policy is updated and
  // |backoff_entry_| is recreated.  Each check that returns the same Result
  // is considered a "failure", to trigger throttling.
  scoped_ptr<net::BackoffEntry> backoff_entry_;

  scoped_ptr<net::URLFetcher> url_fetcher_;

  // URL that returns a 204 response code when connected to the Internet.
  GURL test_url_;

  // The pref member for whether navigation errors should be resolved with a web
  // service.  Actually called "alternate_error_pages", since it's also used for
  // the Link Doctor.
  BooleanPrefMember resolve_errors_with_web_service_;

  base::OneShotTimer<CaptivePortalService> check_captive_portal_timer_;

  static TestingState testing_state_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalService);
};

}  // namespace captive_portal

#endif  // CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_SERVICE_H_
