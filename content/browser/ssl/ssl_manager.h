// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_MANAGER_H_
#define CONTENT_BROWSER_SSL_SSL_MANAGER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/ssl/ssl_policy_backend.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "net/base/cert_status_flags.h"
#include "net/base/net_errors.h"

class LoadFromMemoryCacheDetails;
class NavigationController;
class NavigationEntry;
class ResourceDispatcherHost;
class ResourceRedirectDetails;
class ResourceRequestDetails;
class SSLPolicy;

namespace net {
class SSLInfo;
class URLRequest;
}  // namespace net

// The SSLManager SSLManager controls the SSL UI elements in a TabContents.  It
// listens for various events that influence when these elements should or
// should not be displayed and adjusts them accordingly.
//
// There is one SSLManager per tab.
// The security state (secure/insecure) is stored in the navigation entry.
// Along with it are stored any SSL error code and the associated cert.

class SSLManager : public content::NotificationObserver {
 public:
  // Entry point for SSLCertificateErrors.  This function begins the process
  // of resolving a certificate error during an SSL connection.  SSLManager
  // will adjust the security UI and either call |Cancel| or
  // |ContinueDespiteLastError| on the net::URLRequest.
  //
  // Called on the IO thread.
  static void OnSSLCertificateError(ResourceDispatcherHost* resource_dispatcher,
                                    net::URLRequest* request,
                                    const net::SSLInfo& ssl_info,
                                    bool is_hsts_host);

  // Called when SSL state for a host or tab changes.  Broadcasts the
  // SSL_INTERNAL_STATE_CHANGED notification.
  static void NotifySSLInternalStateChanged(NavigationController* controller);

  // Convenience methods for serializing/deserializing the security info.
  static std::string SerializeSecurityInfo(int cert_id,
                                           net::CertStatus cert_status,
                                           int security_bits,
                                           int connection_status);
  CONTENT_EXPORT static bool DeserializeSecurityInfo(
      const std::string& state,
      int* cert_id,
      net::CertStatus* cert_status,
      int* security_bits,
      int* connection_status);

  // Construct an SSLManager for the specified tab.
  // If |delegate| is NULL, SSLPolicy::GetDefaultPolicy() is used.
  explicit SSLManager(NavigationController* controller);
  virtual ~SSLManager();

  SSLPolicy* policy() { return policy_.get(); }
  SSLPolicyBackend* backend() { return &backend_; }

  // The navigation controller associated with this SSLManager.  The
  // NavigationController is guaranteed to outlive the SSLManager.
  NavigationController* controller() { return controller_; }

  // This entry point is called directly (instead of via the notification
  // service) because we need more precise control of the order in which folks
  // are notified of this event.
  void DidCommitProvisionalLoad(const content::NotificationDetails& details);

  // Insecure content entry point.
  void DidRunInsecureContent(const std::string& security_origin);

  // Called to determine if there were any processed SSL errors from request.
  CONTENT_EXPORT bool ProcessedSSLErrorFromRequest() const;

  // Entry point for navigation.  This function begins the process of updating
  // the security UI when the main frame navigates to a new URL.
  //
  // Called on the UI thread.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Entry points for notifications to which we subscribe. Note that
  // DidCommitProvisionalLoad uses the abstract NotificationDetails type since
  // the type we need is in NavigationController which would create a circular
  // header file dependency.
  void DidLoadFromMemoryCache(LoadFromMemoryCacheDetails* details);
  void DidStartResourceResponse(ResourceRequestDetails* details);
  void DidReceiveResourceRedirect(ResourceRedirectDetails* details);
  void DidChangeSSLInternalState();

  // Update the NavigationEntry with our current state.
  void UpdateEntry(NavigationEntry* entry);

  // The backend for the SSLPolicy to actuate its decisions.
  SSLPolicyBackend backend_;

  // The SSLPolicy instance for this manager.
  scoped_ptr<SSLPolicy> policy_;

  // The NavigationController that owns this SSLManager.  We are responsible
  // for the security UI of this tab.
  NavigationController* controller_;

  // Handles registering notifications with the NotificationService.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLManager);
};

#endif  // CONTENT_BROWSER_SSL_SSL_MANAGER_H_
