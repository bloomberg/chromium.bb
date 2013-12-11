// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class EmbeddedWorkerInstance;
class EmbeddedWorkerRegistry;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;

// This class corresponds to a specific version of a ServiceWorker
// script for a given pattern. When a script is upgraded, there may be
// more than one ServiceWorkerVersion "running" at a time, but only
// one of them is active. This class connects the actual script with a
// running worker.
// Instances of this class are in one of two install states:
// - Pending: The script is in the process of being installed. There
//            may be another active script running.
// - Active: The script is the only worker handling requests for the
//           registration's pattern.
//
// In addition, a version has a running state (this is a rough
// sketch). Since a service worker can be stopped and started at any
// time, it will transition among these states multiple times during
// its lifetime.
// - Stopped: The script is not running
// - Starting: A request to fire an event against the version has been
//             queued, but the worker is not yet
//             loaded/initialized/etc.
// - Started: The worker is ready to receive events
// - Stopping: The worker is returning to the stopped state.
//
// The worker can "run" in both the Pending and the Active
// install states above. During the Pending state, the worker is only
// started in order to fire the 'install' and 'activate'
// events. During the Active state, it can receive other events such
// as 'fetch'.
//
// And finally, is_shutdown_ is detects the live-ness of the object
// itself. If the object is shut down, then it is in the process of
// being deleted from memory. This happens when a version is replaced
// as well as at browser shutdown.
class CONTENT_EXPORT ServiceWorkerVersion
    : NON_EXPORTED_BASE(public base::RefCounted<ServiceWorkerVersion>) {
 public:
  ServiceWorkerVersion(
      ServiceWorkerRegistration* registration,
      EmbeddedWorkerRegistry* worker_registry,
      int64 version_id);

  int64 version_id() const { return version_id_; }

  void Shutdown();
  bool is_shutdown() const { return is_shutdown_; }

  // Starts and stops an embedded worker for this version.
  void StartWorker();
  void StopWorker();

  // Called when this version is associated to a provider host.
  // Non-null |provider_host| must be given.
  void OnAssociateProvider(ServiceWorkerProviderHost* provider_host);
  void OnUnassociateProvider(ServiceWorkerProviderHost* provider_host);

 private:
  friend class base::RefCounted<ServiceWorkerVersion>;

  ~ServiceWorkerVersion();

  const int64 version_id_;

  bool is_shutdown_;
  scoped_refptr<ServiceWorkerRegistration> registration_;

  scoped_ptr<EmbeddedWorkerInstance> embedded_worker_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersion);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
