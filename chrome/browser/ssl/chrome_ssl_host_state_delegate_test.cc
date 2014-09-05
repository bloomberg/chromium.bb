// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/test_data_directory.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kGoogleCertFile[] = "google.single.der";

const char kWWWGoogleHost[] = "www.google.com";
const char kGoogleHost[] = "google.com";
const char kExampleHost[] = "example.com";

const char kForgetAtSessionEnd[] = "-1";
const char kForgetInstantly[] = "0";
const char kDeltaSecondsString[] = "86400";
const uint64_t kDeltaOneDayInSeconds = UINT64_C(86400);

scoped_refptr<net::X509Certificate> GetGoogleCert() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), kGoogleCertFile);
}

}  // namespace

class ChromeSSLHostStateDelegateTest : public InProcessBrowserTest {};

// ChromeSSLHostStateDelegateTest tests basic unit test functionality of the
// SSLHostStateDelegate class.  For example, tests that if a certificate is
// accepted, then it is added to queryable, and if it is revoked, it is not
// queryable. Even though it is effectively a unit test, in needs to be an
// InProcessBrowserTest because the actual functionality is provided by
// ChromeSSLHostStateDelegate which is provided per-profile.
//
// QueryPolicy unit tests the expected behavior of calling QueryPolicy on the
// SSLHostStateDelegate class after various SSL cert decisions have been made.
IN_PROC_BROWSER_TEST_F(ChromeSSLHostStateDelegateTest, QueryPolicy) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // Verifying that all three of the certs we will be looking at are denied
  // before any action has been taken.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kExampleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost.
  state->AllowCert(
      kWWWGoogleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);

  // Verify that only kWWWGoogleHost is allowed and that the other two certs
  // being tested still are denied.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kExampleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));

  // Simulate a user decision to allow an invalid certificate exception for
  // kExampleHost.
  state->AllowCert(
      kExampleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);

  // Verify that both kWWWGoogleHost and kExampleHost have allow exceptions
  // while kGoogleHost still is denied.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kExampleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
}

// HasPolicyAndRevoke unit tests the expected behavior of calling
// HasAllowException before and after calling RevokeUserAllowExceptions on the
// SSLHostStateDelegate class.
IN_PROC_BROWSER_TEST_F(ChromeSSLHostStateDelegateTest, HasPolicyAndRevoke) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  ChromeSSLHostStateDelegate* state =
      ChromeSSLHostStateDelegateFactory::GetForProfile(profile);
  bool unused_value;

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost and for kExampleHost.
  state->AllowCert(
      kWWWGoogleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);
  state->AllowCert(
      kExampleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);

  // Verify that HasAllowException correctly acknowledges that a user decision
  // has been made about kWWWGoogleHost. Then verify that HasAllowException
  // correctly identifies that the decision has been revoked.
  EXPECT_TRUE(state->HasAllowException(kWWWGoogleHost));
  state->RevokeUserAllowExceptions(kWWWGoogleHost);
  EXPECT_FALSE(state->HasAllowException(kWWWGoogleHost));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));

  // Verify that the revocation of the kWWWGoogleHost decision does not affect
  // the Allow for kExampleHost.
  EXPECT_TRUE(state->HasAllowException(kExampleHost));

  // Verify the revocation of the kWWWGoogleHost decision does not affect the
  // non-decision for kGoogleHost. Then verify that a revocation of a URL with
  // no decision has no effect.
  EXPECT_FALSE(state->HasAllowException(kGoogleHost));
  state->RevokeUserAllowExceptions(kGoogleHost);
  EXPECT_FALSE(state->HasAllowException(kGoogleHost));
}

// Clear unit tests the expected behavior of calling Clear to forget all cert
// decision state on the SSLHostStateDelegate class.
IN_PROC_BROWSER_TEST_F(ChromeSSLHostStateDelegateTest, Clear) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  ChromeSSLHostStateDelegate* state =
      ChromeSSLHostStateDelegateFactory::GetForProfile(profile);
  bool unused_value;

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost and for kExampleHost.
  state->AllowCert(
      kWWWGoogleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);

  // Do a full clear, then make sure that both kWWWGoogleHost, which had a
  // decision made, and kExampleHost, which was untouched, are now in a denied
  // state.
  state->Clear();
  EXPECT_FALSE(state->HasAllowException(kWWWGoogleHost));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
  EXPECT_FALSE(state->HasAllowException(kExampleHost));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kExampleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
}

// DidHostRunInsecureContent unit tests the expected behavior of calling
// DidHostRunInsecureContent as well as HostRanInsecureContent to check if
// insecure content has been run and to mark it as such.
IN_PROC_BROWSER_TEST_F(ChromeSSLHostStateDelegateTest,
                       DidHostRunInsecureContent) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  EXPECT_FALSE(state->DidHostRunInsecureContent("www.google.com", 42));
  EXPECT_FALSE(state->DidHostRunInsecureContent("www.google.com", 191));
  EXPECT_FALSE(state->DidHostRunInsecureContent("example.com", 42));

  state->HostRanInsecureContent("www.google.com", 42);

  EXPECT_TRUE(state->DidHostRunInsecureContent("www.google.com", 42));
  EXPECT_FALSE(state->DidHostRunInsecureContent("www.google.com", 191));
  EXPECT_FALSE(state->DidHostRunInsecureContent("example.com", 42));

  state->HostRanInsecureContent("example.com", 42);

  EXPECT_TRUE(state->DidHostRunInsecureContent("www.google.com", 42));
  EXPECT_FALSE(state->DidHostRunInsecureContent("www.google.com", 191));
  EXPECT_TRUE(state->DidHostRunInsecureContent("example.com", 42));
}

// Tests the basic behavior of cert memory in incognito.
class IncognitoSSLHostStateDelegateTest
    : public ChromeSSLHostStateDelegateTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ChromeSSLHostStateDelegateTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRememberCertErrorDecisions,
                                    kDeltaSecondsString);
  }
};

IN_PROC_BROWSER_TEST_F(IncognitoSSLHostStateDelegateTest, PRE_AfterRestart) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // Add a cert exception to the profile and then verify that it still exists
  // in the incognito profile.
  state->AllowCert(
      kWWWGoogleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);

  scoped_ptr<Profile> incognito(profile->CreateOffTheRecordProfile());
  content::SSLHostStateDelegate* incognito_state =
      incognito->GetSSLHostStateDelegate();

  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            incognito_state->QueryPolicy(kWWWGoogleHost,
                                         *google_cert.get(),
                                         net::CERT_STATUS_DATE_INVALID,
                                         &unused_value));

  // Add a cert exception to the incognito profile. It will be checked after
  // restart that this exception does not exist. Note the different cert URL and
  // error than above thus mapping to a second exception. Also validate that it
  // was not added as an exception to the regular profile.
  incognito_state->AllowCert(
      kGoogleHost, *google_cert.get(), net::CERT_STATUS_COMMON_NAME_INVALID);

  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_COMMON_NAME_INVALID,
                               &unused_value));
}

// AfterRestart ensures that any cert decisions made in an incognito profile are
// forgetten after a session restart even if given a command line flag to
// remember cert decisions after restart.
IN_PROC_BROWSER_TEST_F(IncognitoSSLHostStateDelegateTest, AfterRestart) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // Verify that the exception added before restart to the regular
  // (non-incognito) profile still exists and was not cleared after the
  // incognito session ended.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));

  scoped_ptr<Profile> incognito(profile->CreateOffTheRecordProfile());
  content::SSLHostStateDelegate* incognito_state =
      incognito->GetSSLHostStateDelegate();

  // Verify that the exception added before restart to the incognito profile was
  // cleared when the incognito session ended.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            incognito_state->QueryPolicy(kGoogleHost,
                                         *google_cert.get(),
                                         net::CERT_STATUS_COMMON_NAME_INVALID,
                                         &unused_value));
}

// Tests to make sure that if the remember value is set to -1, any decisions
// won't be remembered over a restart.
class ForGetSSLHostStateDelegateTest : public ChromeSSLHostStateDelegateTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ChromeSSLHostStateDelegateTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRememberCertErrorDecisions,
                                    kForgetAtSessionEnd);
  }
};

IN_PROC_BROWSER_TEST_F(ForGetSSLHostStateDelegateTest, PRE_AfterRestart) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  state->AllowCert(
      kWWWGoogleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
}

IN_PROC_BROWSER_TEST_F(ForGetSSLHostStateDelegateTest, AfterRestart) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // The cert should now be |DENIED| because the profile is set to forget cert
  // exceptions after session end.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
}

// Tests to make sure that if the remember value is set to 0, any decisions made
// will be forgetten immediately.
class ForgetInstantlySSLHostStateDelegateTest
    : public ChromeSSLHostStateDelegateTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ChromeSSLHostStateDelegateTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRememberCertErrorDecisions,
                                    kForgetInstantly);
  }
};

IN_PROC_BROWSER_TEST_F(ForgetInstantlySSLHostStateDelegateTest,
                       MakeAndForgetException) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // chrome_state takes ownership of this clock
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  ChromeSSLHostStateDelegate* chrome_state =
      static_cast<ChromeSSLHostStateDelegate*>(state);
  chrome_state->SetClock(scoped_ptr<base::Clock>(clock));

  // Start the clock at standard system time but do not advance at all to
  // emphasize that instant forget works.
  clock->SetNow(base::Time::NowFromSystemTime());

  state->AllowCert(
      kWWWGoogleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
}

// Tests to make sure that if the remember value is set to a non-zero value,
// any decisions will be remembered over a restart, but only for the length
// specified.
class RememberSSLHostStateDelegateTest : public ChromeSSLHostStateDelegateTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ChromeSSLHostStateDelegateTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRememberCertErrorDecisions,
                                    kDeltaSecondsString);
  }
};

IN_PROC_BROWSER_TEST_F(RememberSSLHostStateDelegateTest, PRE_AfterRestart) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  state->AllowCert(
      kWWWGoogleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
}

IN_PROC_BROWSER_TEST_F(RememberSSLHostStateDelegateTest, AfterRestart) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // chrome_state takes ownership of this clock
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  ChromeSSLHostStateDelegate* chrome_state =
      static_cast<ChromeSSLHostStateDelegate*>(state);
  chrome_state->SetClock(scoped_ptr<base::Clock>(clock));

  // Start the clock at standard system time.
  clock->SetNow(base::Time::NowFromSystemTime());

  // This should only pass if the cert was allowed before the test was restart
  // and thus has now been rememebered across browser restarts.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));

  // Simulate the clock advancing by the specified delta.
  clock->Advance(base::TimeDelta::FromSeconds(kDeltaOneDayInSeconds + 1));

  // The cert should now be |DENIED| because the specified delta has passed.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
}

// QueryPolicyExpired unit tests to make sure that if a certificate decision has
// expired, the return value from QueryPolicy returns the correct vaule.
IN_PROC_BROWSER_TEST_F(RememberSSLHostStateDelegateTest, QueryPolicyExpired) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool expired_previous_decision;

  // chrome_state takes ownership of this clock
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  ChromeSSLHostStateDelegate* chrome_state =
      static_cast<ChromeSSLHostStateDelegate*>(state);
  chrome_state->SetClock(scoped_ptr<base::Clock>(clock));

  // Start the clock at standard system time but do not advance at all to
  // emphasize that instant forget works.
  clock->SetNow(base::Time::NowFromSystemTime());

  // The certificate has never been seen before, so it should be UNKONWN and
  // should also indicate that it hasn't expired.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &expired_previous_decision));
  EXPECT_FALSE(expired_previous_decision);

  // After allowing the certificate, a query should say that it is allowed and
  // also specify that it hasn't expired.
  state->AllowCert(
      kWWWGoogleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &expired_previous_decision));
  EXPECT_FALSE(expired_previous_decision);

  // Simulate the clock advancing by the specified delta.
  clock->Advance(base::TimeDelta::FromSeconds(kDeltaOneDayInSeconds + 1));

  // The decision expiration time has come, so it should indicate that the
  // certificate and error are DENIED but also that they expired since the last
  // query.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &expired_previous_decision));
  EXPECT_TRUE(expired_previous_decision);

  // However, with a new query, it should indicate that no new expiration has
  // occurred.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &expired_previous_decision));
  EXPECT_FALSE(expired_previous_decision);
}

// Tests to make sure that if the user deletes their browser history, SSL
// exceptions will be deleted as well.
class RemoveBrowsingHistorySSLHostStateDelegateTest
    : public ChromeSSLHostStateDelegateTest {
 public:
  void RemoveAndWait(Profile* profile) {
    BrowsingDataRemover* remover = BrowsingDataRemover::CreateForPeriod(
        profile, BrowsingDataRemover::LAST_HOUR);
    BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->Remove(BrowsingDataRemover::REMOVE_HISTORY,
                    BrowsingDataHelper::UNPROTECTED_WEB);
    completion_observer.BlockUntilCompletion();
  }
};

IN_PROC_BROWSER_TEST_F(RemoveBrowsingHistorySSLHostStateDelegateTest,
                       DeleteHistory) {
  scoped_refptr<net::X509Certificate> google_cert = GetGoogleCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  bool unused_value;

  // Add an exception for an invalid certificate. Then remove the last hour's
  // worth of browsing history and verify that the exception has been deleted.
  state->AllowCert(
      kGoogleHost, *google_cert.get(), net::CERT_STATUS_DATE_INVALID);
  RemoveAndWait(profile);
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost,
                               *google_cert.get(),
                               net::CERT_STATUS_DATE_INVALID,
                               &unused_value));
}
