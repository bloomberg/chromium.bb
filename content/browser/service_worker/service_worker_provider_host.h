// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "webkit/common/resource_type.h"

namespace ipc {
class Sender;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerDispatcherHost;
class ServiceWorkerVersion;

// This class is the browser-process representation of a serice worker
// provider. There is a provider per document and the lifetime of this
// object is tied to the lifetime of its document in the renderer process.
// This class holds service worker state this is scoped to an individual
// document.
class CONTENT_EXPORT ServiceWorkerProviderHost
    : public base::SupportsWeakPtr<ServiceWorkerProviderHost> {
 public:
  ServiceWorkerProviderHost(int process_id,
                            int provider_id,
                            base::WeakPtr<ServiceWorkerContextCore> context,
                            ServiceWorkerDispatcherHost* dispatcher_host);
  ~ServiceWorkerProviderHost();

  int process_id() const { return process_id_; }
  int provider_id() const { return provider_id_; }
  const std::set<int>& script_client_thread_ids() const {
    return script_client_thread_ids_;
  }

  // The service worker version that corresponds with
  // navigator.serviceWorker.active for our document.
  ServiceWorkerVersion* active_version() const {
    return active_version_.get();
  }

  // The service worker version that corresponds with
  // navigate.serviceWorker.pending for our document.
  ServiceWorkerVersion* pending_version() const {
    return pending_version_.get();
  }

  // The version, if any, that this provider is providing resource loads for.
  // This host observes resource loads made by the serviceworker itself.
  ServiceWorkerVersion* hosted_version() const {
    return hosted_version_.get();
  }

  void set_document_url(const GURL& url) { document_url_ = url; }
  const GURL& document_url() const { return document_url_; }

  // Adds and removes script client thread ID, who is listening events
  // dispatched from ServiceWorker to the document (and any of its dedicated
  // workers) corresponding to this provider.
  void AddScriptClient(int thread_id);
  void RemoveScriptClient(int thread_id);

  // Associate |version| to this provider as its '.active' or '.pending'
  // version.
  // Giving NULL to this method will unset the corresponding field.
  void SetActiveVersion(ServiceWorkerVersion* version);
  void SetPendingVersion(ServiceWorkerVersion* version);

  // Returns false if the version is not in the expected STARTING in our
  // our process state. That would be indicative of a bad IPC message.
  bool SetHostedVersionId(int64 versions_id);

  // Returns true if this provider host should handle requests for
  // |resource_type|.
  bool ShouldHandleRequest(ResourceType::Type resource_type) const;

 private:
  const int process_id_;
  const int provider_id_;
  GURL document_url_;
  std::set<int> script_client_thread_ids_;
  scoped_refptr<ServiceWorkerVersion> active_version_;
  scoped_refptr<ServiceWorkerVersion> pending_version_;
  scoped_refptr<ServiceWorkerVersion> hosted_version_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ServiceWorkerDispatcherHost* dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
