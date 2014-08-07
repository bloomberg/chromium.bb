// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_
#define MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
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

  void RegisterContentHandler(const std::string& mime_type,
                              const GURL& content_handler_url);

  // ServiceLoader methods:
  virtual void Load(ServiceManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE;
  virtual void OnServiceError(ServiceManager* manager, const GURL& url)
      OVERRIDE;

 private:
  typedef std::map<std::string, GURL> MimeTypeToURLMap;

  void LoadLocalService(const GURL& resolved_url,
                        scoped_refptr<LoadCallbacks> callbacks);
  void LoadNetworkService(const GURL& resolved_url,
                          scoped_refptr<LoadCallbacks> callbacks);
  void OnLoadNetworkServiceComplete(scoped_refptr<LoadCallbacks> callbacks,
                                    URLResponsePtr url_response);
  void RunLibrary(const base::FilePath& response_file,
                  scoped_refptr<LoadCallbacks> callbacks,
                  bool delete_file_after,
                  bool response_path_exists);

  Context* const context_;
  scoped_ptr<DynamicServiceRunnerFactory> runner_factory_;
  NetworkServicePtr network_service_;
  URLLoaderPtr url_loader_;
  MimeTypeToURLMap mime_type_to_url_;
  base::WeakPtrFactory<DynamicServiceLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DynamicServiceLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_
