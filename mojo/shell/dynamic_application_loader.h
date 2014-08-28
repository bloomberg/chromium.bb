// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_DYNAMIC_APPLICATION_LOADER_H_
#define MOJO_SHELL_DYNAMIC_APPLICATION_LOADER_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/application_manager/application_loader.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
#include "mojo/shell/dynamic_service_runner.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class Context;
class DynamicServiceRunnerFactory;
class DynamicServiceRunner;

// An implementation of ApplicationLoader that retrieves a dynamic library
// containing the implementation of the service and loads/runs it (via a
// DynamicServiceRunner).
class DynamicApplicationLoader : public ApplicationLoader {
 public:
  DynamicApplicationLoader(
      Context* context,
      scoped_ptr<DynamicServiceRunnerFactory> runner_factory);
  virtual ~DynamicApplicationLoader();

  void RegisterContentHandler(const std::string& mime_type,
                              const GURL& content_handler_url);

  // ApplicationLoader methods:
  virtual void Load(ApplicationManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE;
  virtual void OnApplicationError(ApplicationManager* manager,
                                  const GURL& url) OVERRIDE;

 private:
  class Loader;
  class LocalLoader;
  class NetworkLoader;

  typedef std::map<std::string, GURL> MimeTypeToURLMap;
  typedef base::Callback<void(Loader*)> LoaderCompleteCallback;

  void LoaderComplete(Loader* loader);

  Context* const context_;
  scoped_ptr<DynamicServiceRunnerFactory> runner_factory_;
  NetworkServicePtr network_service_;
  MimeTypeToURLMap mime_type_to_url_;
  ScopedVector<Loader> loaders_;
  LoaderCompleteCallback loader_complete_callback_;

  DISALLOW_COPY_AND_ASSIGN(DynamicApplicationLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_DYNAMIC_APPLICATION_LOADER_H_
