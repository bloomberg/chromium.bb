// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_background_services.pb.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;

// This class is responsible for persisting the debugging events for the
// relevant Web Platform Features. The contexts of the feature will have a
// reference to this, and perform the logging operation.
// This class is also responsible for reading back the data to the DevTools
// client, as the protocol handler will have access to an instance of the
// context.
class CONTENT_EXPORT DevToolsBackgroundServicesContext
    : public base::RefCountedThreadSafe<DevToolsBackgroundServicesContext> {
 public:
  using GetLoggedBackgroundServiceEventsCallback = base::OnceCallback<void(
      std::vector<devtools::proto::BackgroundServiceState>)>;

  DevToolsBackgroundServicesContext(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  // Queries all logged events for |service| and returns them in sorted order
  // (by timestamp). |callback| is called with an empty vector if there was an
  // error. Must be called from the IO thread.
  void GetLoggedBackgroundServiceEvents(
      devtools::proto::BackgroundService service,
      GetLoggedBackgroundServiceEventsCallback callback);

 private:
  friend class DevToolsBackgroundServicesContextTest;
  friend class base::RefCountedThreadSafe<DevToolsBackgroundServicesContext>;
  ~DevToolsBackgroundServicesContext();

  // Entry point for logging a test event.
  void LogTestBackgroundServiceEvent(
      uint64_t service_worker_registration_id,
      const url::Origin& origin,
      devtools::proto::TestBackgroundServiceEvent event);

  // Called after the Log*Event method creates the appropriate state proto.
  void LogBackgroundServiceState(
      uint64_t service_worker_registration_id,
      const url::Origin& origin,
      devtools::proto::BackgroundServiceState service_state);

  void DidGetUserData(
      GetLoggedBackgroundServiceEventsCallback callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      blink::ServiceWorkerStatusCode status);

  BrowserContext* browser_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // TODO(rayankans): Get this value from profile prefs.
  bool should_record_ = false;

  base::WeakPtrFactory<DevToolsBackgroundServicesContext> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsBackgroundServicesContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_
