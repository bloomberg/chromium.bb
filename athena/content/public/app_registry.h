// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_PUBLIC_APP_REGISTRY_H_
#define ATHENA_CONTENT_PUBLIC_APP_REGISTRY_H_

#include <string>
#include <vector>

#include "athena/athena_export.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace content {
class BrowserContext;
}

namespace athena {

class AppActivityRegistry;
class AppRegistryImpl;

// This class holds for each application, held by a user, a list of activities.
// The list of activities can be retrieved as |AppActivityRegistry|. It is used
// to associate activities with applications and allow the resource manager to
// (re)start and stop applications.
class ATHENA_EXPORT AppRegistry {
 public:
  // Creates the AppRegistry instance.
  static void Create();

  // Gets the instance of the controller.
  static AppRegistry* Get();

  // Shuts down the registry (all applications should be shut down by then).
  static void ShutDown();

  // Returns an |AppActivityRegistry| for a given activity |app_id| and
  // |browser_context|.
  virtual AppActivityRegistry* GetAppActivityRegistry(
      const std::string& app_id,
      content::BrowserContext* browser_context) = 0;

  // Returns the number of registered applications.
  virtual int NumberOfApplications() const = 0;

 protected:
  // Only the |AppActivityRegistry| can remove itself.
  friend AppActivityRegistry;

  // Removes an activity registry for an application from the list of known
  // applications.
  virtual void RemoveAppActivityRegistry(AppActivityRegistry* registry) = 0;

  // Constructor and destructor can only be called by the implementing class.
  AppRegistry();
  virtual ~AppRegistry();
};

}  // namespace athena

#endif  // ATHENA_CONTENT_PUBLIC_APP_REGISTRY_H_
