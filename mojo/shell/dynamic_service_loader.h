// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_
#define MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_

#include <map>

#include "base/macros.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/shell/shell.mojom.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/shell/dynamic_service_runner.h"
#include "mojo/shell/keep_alive.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class Context;
class DynamicServiceRunnerFactory;

// An implementation of ServiceLoader that retrieves a dynamic library
// containing the implementation of the service and loads/runs it (via a
// DynamicServiceRunner).
class DynamicServiceLoader : public ServiceLoader {
 public:
  DynamicServiceLoader(Context* context,
                       scoped_ptr<DynamicServiceRunnerFactory> runner_factory);
  virtual ~DynamicServiceLoader();

  // Initiates the dynamic load. If the URL has a mojo: scheme, then the name
  // specified will be modified to the platform's naming scheme. Also, the
  // value specified to the --origin command line argument will be used as the
  // host/port.
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedShellHandle service_handle) OVERRIDE;
  virtual void OnServiceError(ServiceManager* manager, const GURL& url)
      OVERRIDE;

 private:
  class LoadContext;

  void AppCompleted(const GURL& url);

  Context* const context_;
  scoped_ptr<DynamicServiceRunnerFactory> runner_factory_;

  typedef std::map<GURL, LoadContext*> LoadContextMap;
  LoadContextMap url_to_load_context_;

  DISALLOW_COPY_AND_ASSIGN(DynamicServiceLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_
