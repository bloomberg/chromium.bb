// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_APPLICATION_MANAGER_APPLICATION_LOADER_H_
#define SHELL_APPLICATION_MANAGER_APPLICATION_LOADER_H_

#include "base/callback.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/public/interfaces/application.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "url/gurl.h"

namespace mojo {

class Application;

namespace shell {

class ApplicationManager;

// Interface to implement special application loading behavior for a particular
// URL or scheme.
class ApplicationLoader {
 public:
  virtual ~ApplicationLoader() {}

  virtual void Load(
      const GURL& url,
      InterfaceRequest<mojom::Application> application_request) = 0;

 protected:
  ApplicationLoader() {}
};

}  // namespace shell
}  // namespace mojo

#endif  // SHELL_APPLICATION_MANAGER_APPLICATION_LOADER_H_
