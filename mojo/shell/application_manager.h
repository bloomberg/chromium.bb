// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APPLICATION_MANAGER_H_
#define MOJO_SHELL_APPLICATION_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/fetcher.h"
#include "mojo/shell/identity.h"
#include "mojo/shell/native_runner.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class SequencedWorkerPool;
}

namespace mojo {
namespace shell {

class PackageManager;
class ApplicationInstance;
class ContentHandlerConnection;

class ApplicationManager {
 public:
  // API for testing.
  class TestAPI {
   public:
    explicit TestAPI(ApplicationManager* manager);
    ~TestAPI();

    // Returns true if the shared instance has been created.
    static bool HasCreatedInstance();
    // Returns true if there is a ApplicationInstance for this URL.
    bool HasRunningInstanceForURL(const GURL& url) const;
   private:
    ApplicationManager* manager_;

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  explicit ApplicationManager(scoped_ptr<PackageManager> package_manager);
  ~ApplicationManager();

  // Loads a service if necessary and establishes a new client connection.
  // Please see the comments in connect_to_application_params.h for more details
  // about the parameters.
  void ConnectToApplication(scoped_ptr<ConnectToApplicationParams> params);

  // Sets the default Loader to be used if not overridden by SetLoaderForURL().
  void set_default_loader(scoped_ptr<ApplicationLoader> loader) {
    default_loader_ = loader.Pass();
  }
  void set_native_runner_factory(
      scoped_ptr<NativeRunnerFactory> runner_factory) {
    native_runner_factory_ = runner_factory.Pass();
  }
  void set_blocking_pool(base::SequencedWorkerPool* blocking_pool) {
    blocking_pool_ = blocking_pool;
  }
  // Sets a Loader to be used for a specific url.
  void SetLoaderForURL(scoped_ptr<ApplicationLoader> loader, const GURL& url);

  // Destroys all Shell-ends of connections established with Applications.
  // Applications connected by this ApplicationManager will observe pipe errors
  // and have a chance to shutdown.
  void TerminateShellConnections();

  // Removes a ApplicationInstance when it encounters an error.
  void OnApplicationInstanceError(ApplicationInstance* instance);

  // Removes a ContentHandler when its connection is closed.
  void OnContentHandlerConnectionClosed(
      ContentHandlerConnection* content_handler);

  ApplicationInstance* GetApplicationInstance(const Identity& identity) const;

 private:
  using IdentityToInstanceMap = std::map<Identity, ApplicationInstance*>;
  using IdentityToContentHandlerMap =
      std::map<Identity, ContentHandlerConnection*>;
  using URLToLoaderMap = std::map<GURL, ApplicationLoader*>;

  // Takes the contents of |params| only when it returns true.
  bool ConnectToRunningApplication(
      scoped_ptr<ConnectToApplicationParams>* params);

  InterfaceRequest<Application> CreateInstance(
      scoped_ptr<ConnectToApplicationParams> params,
      ApplicationInstance** instance);

  // Called once |fetcher| has found app. |params->app_url()| is the url of
  // the requested application before any mappings/resolution have been applied.
  // The corresponding URLRequest struct in |params| has been taken.
  void HandleFetchCallback(scoped_ptr<ConnectToApplicationParams> params,
                           scoped_ptr<Fetcher> fetcher);

  void RunNativeApplication(InterfaceRequest<Application> application_request,
                            bool start_sandboxed,
                            scoped_ptr<Fetcher> fetcher,
                            const base::FilePath& file_path,
                            bool path_exists);

  void LoadWithContentHandler(
      const Identity& source,
      const Identity& content_handler,
      const Shell::ConnectToApplicationCallback& connect_callback,
      ApplicationInstance* app,
      InterfaceRequest<Application> application_request,
      URLResponsePtr url_response);

  // Returns the appropriate loader for |url|, or the default loader if there is
  // no loader configured for the URL.
  ApplicationLoader* GetLoaderForURL(const GURL& url);

  void CleanupRunner(NativeRunner* runner);

  scoped_ptr<PackageManager> const package_manager_;
  // Loader management.
  // Loaders are chosen in the order they are listed here.
  URLToLoaderMap url_to_loader_;
  scoped_ptr<ApplicationLoader> default_loader_;
  scoped_ptr<NativeRunnerFactory> native_runner_factory_;

  IdentityToInstanceMap identity_to_instance_;
  IdentityToContentHandlerMap identity_to_content_handler_;

  base::SequencedWorkerPool* blocking_pool_;
  ScopedVector<NativeRunner> native_runners_;
  // Counter used to assign ids to content_handlers.
  uint32_t content_handler_id_counter_;
  base::WeakPtrFactory<ApplicationManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManager);
};

Shell::ConnectToApplicationCallback EmptyConnectCallback();

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APPLICATION_MANAGER_H_
