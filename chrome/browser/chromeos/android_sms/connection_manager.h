// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_CONNECTION_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/chromeos/android_sms/connection_establisher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"

namespace chromeos {

namespace android_sms {

// ConnectionManager checks to see when the user has no Android Messages for Web
// pages open (in a web page or PWA) and notifies the corresponding
// service worker to start a background connection.
//
// This class is an observer for Android Messages for Web service worker
// lifecycle events and uses ConnectionEstablisher to initiate a background
// connection with Tachyon server when appropriate. Specifically, in the
// following cases
// 1. Startup.
// 2. Activation: This occurs on service worker update, when a new updated
//    version of the service worker replaces the current active version or on
//    service worker cold start.
// 3. NoControllees: When all pages controlled by the service worker are closed.
// The connection establishment may be attempted in more cases than actually
// required. It's left to the service worker to ignore or establish a background
// connection as required. E.g., The service worker will not establish a
// connection if it's is already connected to the Android Messages for Web page
// or if a connection already exists.
class ConnectionManager : public content::ServiceWorkerContextObserver {
 public:
  ConnectionManager(
      content::ServiceWorkerContext* service_worker_context,
      std::unique_ptr<ConnectionEstablisher> connection_establisher);
  ~ConnectionManager() override;

 private:
  // content::ServiceWorkerContextObserver:
  void OnVersionActivated(int64_t version_id, const GURL& scope) override;
  void OnVersionRedundant(int64_t version_id, const GURL& scope) override;
  void OnNoControllees(int64_t version_id, const GURL& scope) override;

  content::ServiceWorkerContext* service_worker_context_;
  std::unique_ptr<ConnectionEstablisher> connection_establisher_;

  // Version ID of the Android Messages for Web service worker that's currently
  // active i.e., capable of handling messages and controlling pages.
  base::Optional<int64_t> active_version_id_;

  // Version ID of the previously active Android Messages for Web
  // service worker.
  base::Optional<int64_t> prev_active_version_id_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionManager);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_CONNECTION_MANAGER_H_
