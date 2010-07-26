// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_POLICY_H_
#define CHROME_BROWSER_SSL_SSL_POLICY_H_
#pragma once

#include <string>

#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "webkit/glue/resource_type.h"

class NavigationEntry;
class SSLCertErrorHandler;
class SSLPolicyBackend;
class SSLRequestInfo;

// SSLPolicy
//
// This class is responsible for making the security decisions that concern the
// SSL trust indicators.  It relies on the SSLPolicyBackend to actually enact
// the decisions it reaches.
//
class SSLPolicy : public SSLBlockingPage::Delegate {
 public:
  explicit SSLPolicy(SSLPolicyBackend* backend);

  // An error occurred with the certificate in an SSL connection.
  void OnCertError(SSLCertErrorHandler* handler);

  void DidRunInsecureContent(NavigationEntry* entry,
                             const std::string& security_origin);

  // We have started a resource request with the given info.
  void OnRequestStarted(SSLRequestInfo* info);

  // Update the SSL information in |entry| to match the current state.
  // |tab_contents| is the TabContents associated with this entry.
  void UpdateEntry(NavigationEntry* entry, TabContents* tab_contents);

  SSLPolicyBackend* backend() const { return backend_; }

  // SSLBlockingPage::Delegate methods.
  virtual SSLErrorInfo GetSSLErrorInfo(SSLCertErrorHandler* handler);
  virtual void OnDenyCertificate(SSLCertErrorHandler* handler);
  virtual void OnAllowCertificate(SSLCertErrorHandler* handler);

 private:
  // Helper method for derived classes handling certificate errors.
  // If the error can be overridden by the user, show a blocking page that
  // lets the user continue or cancel the request.
  // For fatal certificate errors, show a blocking page that only lets the
  // user cancel the request.
  void OnCertErrorInternal(SSLCertErrorHandler* handler,
                           SSLBlockingPage::ErrorLevel error_level);

  // If the security style of |entry| has not been initialized, then initialize
  // it with the default style for its URL.
  void InitializeEntryIfNeeded(NavigationEntry* entry);

  // Mark |origin| as having run insecure content in the process with ID |pid|.
  void OriginRanInsecureContent(const std::string& origin, int pid);

  // The backend we use to enact our decisions.
  SSLPolicyBackend* backend_;

  DISALLOW_COPY_AND_ASSIGN(SSLPolicy);
};

#endif  // CHROME_BROWSER_SSL_SSL_POLICY_H_
