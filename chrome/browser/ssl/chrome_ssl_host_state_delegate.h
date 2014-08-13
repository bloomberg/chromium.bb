// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SSL_HOST_STATE_DELEGATE_H_
#define CHROME_BROWSER_SSL_CHROME_SSL_HOST_STATE_DELEGATE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/ssl_host_state_delegate.h"

class Profile;

namespace base {
class Clock;
class DictionaryValue;
}  //  namespace base

// Implementation of the tracking of user decisions on SSL errors for sites.
// Tracks if the user has allowed, denied, or not seen an exception for the
// specified site, SSL fingerprint, and error. If the user makes a decision,
// stores the decision until either the session ends or for a length of time
// (across session restarts), based on command line flags.
class ChromeSSLHostStateDelegate : public content::SSLHostStateDelegate {
 public:
  explicit ChromeSSLHostStateDelegate(Profile* profile);
  virtual ~ChromeSSLHostStateDelegate();

  // SSLHostStateDelegate:
  virtual void DenyCert(const std::string& host,
                        net::X509Certificate* cert,
                        net::CertStatus error) OVERRIDE;
  virtual void AllowCert(const std::string& host,
                         net::X509Certificate* cert,
                         net::CertStatus error) OVERRIDE;
  virtual void Clear() OVERRIDE;
  virtual net::CertPolicy::Judgment QueryPolicy(const std::string& host,
                                                net::X509Certificate* cert,
                                                net::CertStatus error) OVERRIDE;
  virtual void HostRanInsecureContent(const std::string& host,
                                      int pid) OVERRIDE;
  virtual bool DidHostRunInsecureContent(const std::string& host,
                                         int pid) const OVERRIDE;

  // ChromeSSLHostStateDelegate implementation:
  // Revoke all user decisions for |host| in the given Profile. The
  // RevokeUserDecisionsHard version may close idle connections in the process.
  // This version should be used *only* for rare events, such as a user
  // controlled button, as it may be very disruptive to the networking stack.
  virtual void RevokeUserDecisions(const std::string& host);
  virtual void RevokeUserDecisionsHard(const std::string& host);

  // Returns true if any decisions has been recorded for |host| for the given
  // Profile, otherwise false.
  virtual bool HasUserDecision(const std::string& host);

  // Called on the UI thread when the profile is about to be destroyed.
  void ShutdownOnUIThread() {}

 protected:
  // SetClock takes ownership of the passed in clock.
  void SetClock(scoped_ptr<base::Clock> clock);

 private:
  FRIEND_TEST_ALL_PREFIXES(ForgetInstantlySSLHostStateDelegateTest,
                           MakeAndForgetException);
  FRIEND_TEST_ALL_PREFIXES(RememberSSLHostStateDelegateTest, AfterRestart);

  // Used to specify whether new content setting entries should be created if
  // they don't already exist when querying the user's settings.
  enum CreateDictionaryEntriesDisposition {
    CreateDictionaryEntries,
    DoNotCreateDictionaryEntries
  };

  // Specifies whether user SSL error decisions should be forgetten at the end
  // of this current session (the old style of remembering decisions), or
  // whether they should be remembered across session restarts for a specified
  // length of time, deteremined by
  // |default_ssl_cert_decision_expiration_delta_|.
  enum RememberSSLExceptionDecisionsDisposition {
    ForgetSSLExceptionDecisionsAtSessionEnd,
    RememberSSLExceptionDecisionsForDelta
  };

  // Modify the user's content settings to specify a judgement made for a
  // specific site and certificate, where |url| is the site in question, |cert|
  // is the certificate with an error, |error| is the error in the certificate,
  // and |judgement| is the user decision to be recorded.
  void ChangeCertPolicy(const std::string& host,
                        net::X509Certificate* cert,
                        net::CertStatus error,
                        net::CertPolicy::Judgment judgment);

  // Query the content settings to retrieve a dictionary of certificate
  // fingerprints and errors of certificates to user decisions, as set by
  // ChangeCertPolicy. Returns NULL on a failure.
  //
  // |dict| specifies the user's full exceptions dictionary for a specific site
  // in their content settings. Must be retrieved directly from a website
  // setting in the the profile's HostContentSettingsMap.
  //
  // If |create_entries| specifies CreateDictionaryEntries, then
  // GetValidCertDecisionsDict will create a new set of entries within the
  // dictionary if they do not already exist. Otherwise will fail and return if
  // NULL if they do not exist.
  base::DictionaryValue* GetValidCertDecisionsDict(
      base::DictionaryValue* dict,
      CreateDictionaryEntriesDisposition create_entries);

  scoped_ptr<base::Clock> clock_;
  RememberSSLExceptionDecisionsDisposition should_remember_ssl_decisions_;
  base::TimeDelta default_ssl_cert_decision_expiration_delta_;
  Profile* profile_;

  // A BrokenHostEntry is a pair of (host, process_id) that indicates the host
  // contains insecure content in that renderer process.
  typedef std::pair<std::string, int> BrokenHostEntry;

  // Hosts which have been contaminated with insecure content in the
  // specified process.  Note that insecure content can travel between
  // same-origin frames in one processs but cannot jump between processes.
  std::set<BrokenHostEntry> ran_insecure_content_hosts_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSSLHostStateDelegate);
};

#endif  // CHROME_BROWSER_SSL_CHROME_SSL_HOST_STATE_DELEGATE_H_
