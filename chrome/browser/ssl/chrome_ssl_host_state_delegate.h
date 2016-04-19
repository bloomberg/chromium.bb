// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SSL_HOST_STATE_DELEGATE_H_
#define CHROME_BROWSER_SSL_CHROME_SSL_HOST_STATE_DELEGATE_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/public/browser/ssl_host_state_delegate.h"

class Profile;

namespace base {
class Clock;
class DictionaryValue;
}  //  namespace base

// Tracks whether the user has allowed a certificate error exception for a
// specific site, SSL fingerprint, and error. Based on command-line flags and
// experimental group, remembers this decision either until end-of-session or
// for a particular length of time.
class ChromeSSLHostStateDelegate : public content::SSLHostStateDelegate {
 public:
  explicit ChromeSSLHostStateDelegate(Profile* profile);
  ~ChromeSSLHostStateDelegate() override;

  // SSLHostStateDelegate:
  void AllowCert(const std::string& host,
                 const net::X509Certificate& cert,
                 net::CertStatus error) override;
  void Clear() override;
  CertJudgment QueryPolicy(const std::string& host,
                           const net::X509Certificate& cert,
                           net::CertStatus error,
                           bool* expired_previous_decision) override;
  void HostRanInsecureContent(const std::string& host, int pid) override;
  bool DidHostRunInsecureContent(const std::string& host,
                                 int pid) const override;

  // Revokes all SSL certificate error allow exceptions made by the user for
  // |host| in the given Profile.
  void RevokeUserAllowExceptions(const std::string& host) override;

  // RevokeUserAllowExceptionsHard is the same as RevokeUserAllowExceptions but
  // additionally may close idle connections in the process. This should be used
  // *only* for rare events, such as a user controlled button, as it may be very
  // disruptive to the networking stack.
  virtual void RevokeUserAllowExceptionsHard(const std::string& host);

  // Returns whether the user has allowed a certificate error exception for
  // |host|. This does not mean that *all* certificate errors are allowed, just
  // that there exists an exception. To see if a particular certificate and
  // error combination exception is allowed, use QueryPolicy().
  bool HasAllowException(const std::string& host) const override;

 protected:
  // SetClock takes ownership of the passed in clock.
  void SetClock(std::unique_ptr<base::Clock> clock);

 private:
  FRIEND_TEST_ALL_PREFIXES(DefaultMemorySSLHostStateDelegateTest, AfterRestart);
  FRIEND_TEST_ALL_PREFIXES(DefaultMemorySSLHostStateDelegateTest,
                           QueryPolicyExpired);

  // Used to specify whether new content setting entries should be created if
  // they don't already exist when querying the user's settings.
  enum CreateDictionaryEntriesDisposition {
    CREATE_DICTIONARY_ENTRIES,
    DO_NOT_CREATE_DICTIONARY_ENTRIES
  };

  // Specifies whether user SSL error decisions should be forgetten at the end
  // of this current session (the old style of remembering decisions), or
  // whether they should be remembered across session restarts for a specified
  // length of time, deteremined by
  // |default_ssl_cert_decision_expiration_delta_|.
  enum RememberSSLExceptionDecisionsDisposition {
    FORGET_SSL_EXCEPTION_DECISIONS_AT_SESSION_END,
    REMEMBER_SSL_EXCEPTION_DECISIONS_FOR_DELTA
  };

  // Returns a dictionary of certificate fingerprints and errors that have been
  // allowed as exceptions by the user.
  //
  // |dict| specifies the user's full exceptions dictionary for a specific site
  // in their content settings. Must be retrieved directly from a website
  // setting in the the profile's HostContentSettingsMap.
  //
  // If |create_entries| specifies CreateDictionaryEntries, then
  // GetValidCertDecisionsDict will create a new set of entries within the
  // dictionary if they do not already exist. Otherwise will fail and return if
  // NULL if they do not exist.
  //
  // |expired_previous_decision| is set to true if there had been a previous
  // decision made by the user but it has expired. Otherwise it is set to false.
  base::DictionaryValue* GetValidCertDecisionsDict(
      base::DictionaryValue* dict,
      CreateDictionaryEntriesDisposition create_entries,
      bool* expired_previous_decision);

  std::unique_ptr<base::Clock> clock_;
  RememberSSLExceptionDecisionsDisposition should_remember_ssl_decisions_;
  Profile* profile_;

  // A BrokenHostEntry is a pair of (host, process_id) that indicates the host
  // contains insecure content in that renderer process.
  typedef std::pair<std::string, int> BrokenHostEntry;

  // Hosts which have been contaminated with insecure content in the
  // specified process.  Note that insecure content can travel between
  // same-origin frames in one processs but cannot jump between processes.
  std::set<BrokenHostEntry> ran_insecure_content_hosts_;

  // This is a GUID to mark this unique session. Whenever a certificate decision
  // expiration is set, the GUID is saved as well so Chrome can tell if it was
  // last set during the current session. This is used by the
  // FORGET_SSL_EXCEPTION_DECISIONS_AT_SESSION_END experimental group to
  // determine if the expired_previous_decision bit should be set on queries.
  //
  // Why not just iterate over the set of current extensions and mark them all
  // as expired when the session starts, rather than storing a GUID for the
  // current session? Glad you asked! Unfortunately, content settings does not
  // currently support iterating over all current *compound* content setting
  // values (iteration only works for simple content settings). While this could
  // be added, it would be a fair amount of work for what amounts to a temporary
  // measurement problem, so it's not worth the complexity.
  //
  // TODO(jww): This is only used by the default and disable groups of the
  // certificate memory decisions experiment to tell if a decision has expired
  // since the last session. Since this is only used for UMA purposes, this
  // should be removed after the experiment has finished, and a call to Clear()
  // should be added to the constructor and destructor for members of the
  // FORGET_SSL_EXCEPTION_DECISIONS_AT_SESSION_END groups. See
  // https://crbug.com/418631 for more details.
  const std::string current_expiration_guid_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSSLHostStateDelegate);
};

#endif  // CHROME_BROWSER_SSL_CHROME_SSL_HOST_STATE_DELEGATE_H_
