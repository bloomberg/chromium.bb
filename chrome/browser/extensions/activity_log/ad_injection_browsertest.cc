// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/scoped_observer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/activity_log/ad_network_database.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace net {
namespace test_server {
struct HttpRequest;
}
}

namespace extensions {

namespace {

// The "ad network" that we are using. Any src or href equal to this should be
// considered an ad network.
const char kAdNetwork1[] = "http://www.known-ads.adnetwork";
const char kAdNetwork2[] = "http://www.also-known-ads.adnetwork";

// The current stage of the test.
enum Stage {
  BEFORE_RESET,  // We are about to reset the page.
  RESETTING,     // We are resetting the page.
  TESTING        // The reset is complete, and we are testing.
};

// The string sent by the test to indicate that the page reset will begin.
const char kResetBeginString[] = "Page Reset Begin";
// The string sent by the test to indicate that page reset is complete.
const char kResetEndString[] = "Page Reset End";
// The string sent by the test to indicate a JS error was caught in the test.
const char kJavascriptErrorString[] = "Testing Error";
// The string sent by the test to indicate that we have concluded the full test.
const char kTestCompleteString[] = "Test Complete";

std::string InjectionTypeToString(Action::InjectionType type) {
  switch (type) {
    case Action::NO_AD_INJECTION:
      return "No Ad Injection";
    case Action::INJECTION_NEW_AD:
      return "Injection New Ad";
    case Action::INJECTION_REMOVED_AD:
      return "Injection Removed Ad";
    case Action::INJECTION_REPLACED_AD:
      return "Injection Replaced Ad";
    case Action::INJECTION_LIKELY_NEW_AD:
      return "Injection Likely New Ad";
    case Action::INJECTION_LIKELY_REPLACED_AD:
      return "Injection Likely Replaced Ad";
    case Action::NUM_INJECTION_TYPES:
      return "Num Injection Types";
  }
  return std::string();
}

// An implementation of ActivityLog::Observer that, for every action, sends it
// through Action::DidInjectAd(). This will keep track of the observed
// injections, and can be enabled or disabled as needed (for instance, this
// should be disabled while we are resetting the page).
class ActivityLogObserver : public ActivityLog::Observer {
 public:
  explicit ActivityLogObserver(content::BrowserContext* context);
  virtual ~ActivityLogObserver();

  // Disable the observer (e.g., to reset the page).
  void disable() { enabled_ = false; }

  // Enable the observer, resetting the state.
  void enable() {
    injection_type_ = Action::NO_AD_INJECTION;
    found_multiple_injections_ = false;
    enabled_ = true;
  }

  Action::InjectionType injection_type() const { return injection_type_; }

  bool found_multiple_injections() const { return found_multiple_injections_; }

 private:
  virtual void OnExtensionActivity(scoped_refptr<Action> action) OVERRIDE;

  ScopedObserver<ActivityLog, ActivityLog::Observer> scoped_observer_;

  // The associated BrowserContext.
  content::BrowserContext* context_;

  // The type of the last injection.
  Action::InjectionType injection_type_;

  // Whether or not we found multiple injection types (which shouldn't happen).
  bool found_multiple_injections_;

  // Whether or not the observer is enabled.
  bool enabled_;
};

ActivityLogObserver::ActivityLogObserver(content::BrowserContext* context)
    : scoped_observer_(this),
      context_(context),
      injection_type_(Action::NO_AD_INJECTION),
      found_multiple_injections_(false),
      enabled_(false) {
  ActivityLog::GetInstance(context_)->AddObserver(this);
}

ActivityLogObserver::~ActivityLogObserver() {}

void ActivityLogObserver::OnExtensionActivity(scoped_refptr<Action> action) {
  if (!enabled_)
    return;

  Action::InjectionType type =
      action->DidInjectAd(NULL /* no rappor service */);
  if (type != Action::NO_AD_INJECTION) {
    if (injection_type_ != Action::NO_AD_INJECTION)
      found_multiple_injections_ = true;
    injection_type_ = type;
  }
}

// A mock for the AdNetworkDatabase. This simply says that the URL
// http://www.known-ads.adnetwork is an ad network, and nothing else is.
class TestAdNetworkDatabase : public AdNetworkDatabase {
 public:
  TestAdNetworkDatabase();
  virtual ~TestAdNetworkDatabase();

 private:
  virtual bool IsAdNetwork(const GURL& url) const OVERRIDE;

  GURL ad_network_url1_;
  GURL ad_network_url2_;
};

TestAdNetworkDatabase::TestAdNetworkDatabase() : ad_network_url1_(kAdNetwork1),
                                                 ad_network_url2_(kAdNetwork2) {
}

TestAdNetworkDatabase::~TestAdNetworkDatabase() {}

bool TestAdNetworkDatabase::IsAdNetwork(const GURL& url) const {
  return url == ad_network_url1_ || url == ad_network_url2_;
}

scoped_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  scoped_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_code(net::HTTP_OK);
  return response.PassAs<net::test_server::HttpResponse>();
}

}  // namespace

class AdInjectionBrowserTest : public ExtensionBrowserTest {
 protected:
  AdInjectionBrowserTest();
  virtual ~AdInjectionBrowserTest();

  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void TearDownOnMainThread() OVERRIDE;

  // Handle the "Reset Begin" stage of the test.
  testing::AssertionResult HandleResetBeginStage();

  // Handle the "Reset End" stage of the test.
  testing::AssertionResult HandleResetEndStage();

  // Handle the "Testing" stage of the test.
  testing::AssertionResult HandleTestingStage(const std::string& message);

  // Handle a JS error encountered in a test.
  testing::AssertionResult HandleJSError(const std::string& message);

  const base::FilePath& test_data_dir() { return test_data_dir_; }

  ExtensionTestMessageListener* listener() { return listener_.get(); }

  ActivityLogObserver* observer() { return observer_.get(); }

 private:
  // The name of the last completed test; used in case of unexpected failure for
  // debugging.
  std::string last_test_;

  // A listener for any messages from our ad-injecting extension.
  scoped_ptr<ExtensionTestMessageListener> listener_;

  // An observer to be alerted when we detect ad injection.
  scoped_ptr<ActivityLogObserver> observer_;

  // The current stage of the test.
  Stage stage_;
};

AdInjectionBrowserTest::AdInjectionBrowserTest() : stage_(BEFORE_RESET) {
}

AdInjectionBrowserTest::~AdInjectionBrowserTest() {
}

void AdInjectionBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  embedded_test_server()->RegisterRequestHandler(base::Bind(&HandleRequest));

  test_data_dir_ =
      test_data_dir_.AppendASCII("activity_log").AppendASCII("ad_injection");
  observer_.reset(new ActivityLogObserver(profile()));

  // We use a listener in order to keep the actions in the Javascript test
  // synchronous. At the end of each stage, the test will send us a message
  // with the stage and status, and will not advance until we reply with
  // a message.
  listener_.reset(new ExtensionTestMessageListener(true /* will reply */));

  // Enable the activity log for this test.
  ActivityLog::GetInstance(profile())->SetWatchdogAppActiveForTesting(true);

  // Set the ad network database.
  AdNetworkDatabase::SetForTesting(
      scoped_ptr<AdNetworkDatabase>(new TestAdNetworkDatabase));
}

void AdInjectionBrowserTest::TearDownOnMainThread() {
  observer_.reset(NULL);
  listener_.reset(NULL);
  ActivityLog::GetInstance(profile())->SetWatchdogAppActiveForTesting(false);

  ExtensionBrowserTest::TearDownOnMainThread();
}

testing::AssertionResult AdInjectionBrowserTest::HandleResetBeginStage() {
  if (stage_ != BEFORE_RESET) {
    return testing::AssertionFailure()
           << "In incorrect stage. Last Test: " << last_test_;
  }

  // Stop looking for ad injection, since some of the reset could be considered
  // ad injection.
  observer()->disable();
  stage_ = RESETTING;
  return testing::AssertionSuccess();
}

testing::AssertionResult AdInjectionBrowserTest::HandleResetEndStage() {
  if (stage_ != RESETTING) {
    return testing::AssertionFailure()
           << "In incorrect stage. Last test: " << last_test_;
  }

  // Look for ad injection again, now that the reset is over.
  observer()->enable();
  stage_ = TESTING;
  return testing::AssertionSuccess();
}

testing::AssertionResult AdInjectionBrowserTest::HandleTestingStage(
    const std::string& message) {
  if (stage_ != TESTING) {
    return testing::AssertionFailure()
           << "In incorrect stage. Last test: " << last_test_;
  }

  // The format for a testing message is:
  // "<test_name>:<expected_change>"
  // where <test_name> is the name of the test and <expected_change> is
  // either -1 for no ad injection (to test against false positives) or the
  // number corresponding to ad_detection::InjectionType.
  size_t sep = message.find(':');
  int expected_change = -1;
  if (sep == std::string::npos ||
      !base::StringToInt(message.substr(sep + 1), &expected_change) ||
      (expected_change < Action::NO_AD_INJECTION ||
       expected_change >= Action::NUM_INJECTION_TYPES)) {
    return testing::AssertionFailure()
           << "Invalid message received for testing stage: " << message;
  }

  last_test_ = message.substr(0, sep);

  Action::InjectionType expected_injection =
      static_cast<Action::InjectionType>(expected_change);
  std::string error;
  if (observer()->found_multiple_injections()) {
    error = "Found multiple injection types. "
            "Only one injection is expected per test.";
  } else if (expected_injection != observer()->injection_type()) {
    // We need these static casts, because size_t is different on different
    // architectures, and printf becomes unhappy.
    error = base::StringPrintf(
        "Incorrect Injection Found: Expected: %s, Actual: %s",
        InjectionTypeToString(expected_injection).c_str(),
        InjectionTypeToString(observer()->injection_type()).c_str());
  }

  stage_ = BEFORE_RESET;

  if (!error.empty()) {
    return testing::AssertionFailure()
        << "Error in Test '" << last_test_ << "': " << error;
  }

  return testing::AssertionSuccess();
}

testing::AssertionResult AdInjectionBrowserTest::HandleJSError(
    const std::string& message) {
  // The format for a testing message is:
  // "Testing Error:<test_name>:<error>"
  // where <test_name> is the name of the test and <error> is the error which
  // was encountered.
  size_t first_sep = message.find(':');
  size_t second_sep = message.find(':', first_sep + 1);
  if (first_sep == std::string::npos || second_sep == std::string::npos) {
    return testing::AssertionFailure()
           << "Invalid message received: " << message;
  }

  std::string test_name =
      message.substr(first_sep + 1, second_sep - first_sep - 1);
  std::string test_err = message.substr(second_sep + 1);

  // We set the stage here, so that subsequent tests don't fail.
  stage_ = BEFORE_RESET;

  return testing::AssertionFailure() << "Javascript Error in test '"
                                     << test_name << "': " << test_err;
}

// This is the primary Ad-Injection browser test. It loads an extension that
// has a content script that, in turn, injects ads left, right, and center.
// The content script waits after each injection for a response from this
// browsertest, in order to ensure synchronicity. After each injection, the
// content script cleans up after itself. For significantly more detailed
// comments, see
// chrome/test/data/extensions/activity_log/ad_injection/content_script.js.
IN_PROC_BROWSER_TEST_F(AdInjectionBrowserTest, DetectAdInjections) {
  const Extension* extension = LoadExtension(test_data_dir_);
  ASSERT_TRUE(extension);

  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL("/"));

  std::string message;
  while (message != "TestComplete") {
    listener()->WaitUntilSatisfied();
    message = listener()->message();
    if (message == kResetBeginString) {
      ASSERT_TRUE(HandleResetBeginStage());
    } else if (message == kResetEndString) {
      ASSERT_TRUE(HandleResetEndStage());
    } else if (!message.compare(
                   0, strlen(kJavascriptErrorString), kJavascriptErrorString)) {
      EXPECT_TRUE(HandleJSError(message));
    } else if (message == kTestCompleteString) {
      break;  // We're done!
    } else {  // We're in some kind of test.
      EXPECT_TRUE(HandleTestingStage(message));
    }

    // In all cases (except for "Test Complete", in which case we already
    // break'ed), we reply with a continue message.
    listener()->Reply("Continue");
    listener()->Reset();
  }
}

// TODO(rdevlin.cronin): We test a good amount of ways of injecting ads with
// the above test, but more is better in testing.
// See crbug.com/357204.

}  // namespace extensions
