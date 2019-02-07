// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/content_export.h"

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
  DevToolsBackgroundServicesContext(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

 private:
  friend class base::RefCountedThreadSafe<DevToolsBackgroundServicesContext>;
  ~DevToolsBackgroundServicesContext();

  BrowserContext* browser_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsBackgroundServicesContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_
