// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistrationInfo;
class ServiceWorkerVersion;

// This class manages all persistence of service workers:
//  - Registrations
//  - Mapping of caches to registrations / versions
//
// This is the place where we manage simultaneous
// requests for the same registrations and caches, making sure that
// two pages that are registering the same pattern at the same time
// have their registrations coalesced rather than overwriting each
// other.
//
// This class also manages the state of the upgrade process, which
// includes managing which ServiceWorkerVersion is "active" vs "in
// waiting".
class CONTENT_EXPORT ServiceWorkerRegistration
    : NON_EXPORTED_BASE(public base::RefCounted<ServiceWorkerRegistration>) {
 public:
  class Listener {
   public:
    virtual void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        ChangedVersionAttributesMask changed_mask,
        const ServiceWorkerRegistrationInfo& info)  = 0;
  };

  ServiceWorkerRegistration(const GURL& pattern,
                            const GURL& script_url,
                            int64 registration_id,
                            base::WeakPtr<ServiceWorkerContextCore> context);

  int64 id() const { return registration_id_; }
  const GURL& script_url() const { return script_url_; }
  const GURL& pattern() const { return pattern_; }

  ServiceWorkerVersion* active_version() const {
    return active_version_.get();
  }

  ServiceWorkerVersion* waiting_version() const {
    return waiting_version_.get();
  }

  ServiceWorkerVersion* installing_version() const {
    return installing_version_.get();
  }

  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  ServiceWorkerRegistrationInfo GetInfo();

  // Sets the corresposding version attribute and resets the position (if any)
  // left vacant (ie. by a waiting version being promoted).
  // Also notifies listeners via OnVersionAttributesChanged.
  void SetActiveVersion(ServiceWorkerVersion* version);
  void SetWaitingVersion(ServiceWorkerVersion* version);
  void SetInstallingVersion(ServiceWorkerVersion* version);

  // If version is the installing, waiting, active version of this
  // registation, the method will reset that field to NULL, and notify
  // listeners via OnVersionAttributesChanged.
  void UnsetVersion(ServiceWorkerVersion* version);

 private:
  ~ServiceWorkerRegistration();
  friend class base::RefCounted<ServiceWorkerRegistration>;

  void SetVersionInternal(
      ServiceWorkerVersion* version,
      scoped_refptr<ServiceWorkerVersion>* data_member,
      int change_flag);
  void UnsetVersionInternal(
      ServiceWorkerVersion* version,
      ChangedVersionAttributesMask* mask);

  const GURL pattern_;
  const GURL script_url_;
  const int64 registration_id_;
  scoped_refptr<ServiceWorkerVersion> active_version_;
  scoped_refptr<ServiceWorkerVersion> waiting_version_;
  scoped_refptr<ServiceWorkerVersion> installing_version_;
  ObserverList<Listener> listeners_;
  base::WeakPtr<ServiceWorkerContextCore> context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistration);
};
}  // namespace content
#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
