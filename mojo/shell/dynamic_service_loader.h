// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_
#define MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_

#include <map>

#include "base/basictypes.h"
#include "mojo/public/shell/shell.mojom.h"
#include "mojo/public/system/core_cpp.h"
#include "mojo/shell/keep_alive.h"
#include "mojo/shell/service_manager.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class Context;

// A subclass of ServiceManager::Loader that loads a dynamic library containing
// the implementation of the service.
class DynamicServiceLoader : public ServiceManager::Loader {
 public:
  explicit DynamicServiceLoader(Context* context);
  virtual ~DynamicServiceLoader();

  // Initiates the dynamic load. If the url is a mojo: scheme then the name
  // specified will be modified to the platform's naming scheme. Also the
  // value specified to the --origin command line argument will be used as the
  // host / port.
  virtual void Load(const GURL& url,
                    ScopedShellHandle service_handle) MOJO_OVERRIDE;

 private:
  class LoadContext;

  void AppCompleted(const GURL& url);

  typedef std::map<GURL, LoadContext*> LoadContextMap;
  LoadContextMap url_to_load_context_;
  Context* context_;

  DISALLOW_COPY_AND_ASSIGN(DynamicServiceLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_
