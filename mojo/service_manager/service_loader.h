// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICE_MANAGER_SERVICE_LOADER_H_
#define MOJO_SERVICE_MANAGER_SERVICE_LOADER_H_

#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/service_manager/service_manager_export.h"
#include "url/gurl.h"

namespace mojo {

class ServiceManager;

// Interface to allowing loading behavior to be established for schemes,
// specific urls or as the default.
// A ServiceLoader is responsible to using whatever mechanism is appropriate
// to load the application at url.
// TODO(davemoore): change name to ApplicationLoader.
// The handle to the shell is passed to that application so it can bind it to
// a Shell instance. This will give the Application a way to connect to other
// apps and services.
class MOJO_SERVICE_MANAGER_EXPORT ServiceLoader {
 public:
  virtual ~ServiceLoader() {}
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedMessagePipeHandle shell_handle) = 0;
  virtual void OnServiceError(ServiceManager* manager, const GURL& url) = 0;

 protected:
  ServiceLoader() {}
};

}  // namespace mojo

#endif  // MOJO_SERVICE_MANAGER_SERVICE_LOADER_H_
