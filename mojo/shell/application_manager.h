// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APPLICATION_MANAGER_H_
#define MOJO_SHELL_APPLICATION_MANAGER_H_

#include <map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/common/weak_interface_ptr_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/fetcher.h"
#include "mojo/shell/identity.h"
#include "mojo/shell/native_runner.h"
#include "mojo/shell/public/interfaces/application.mojom.h"
#include "mojo/shell/public/interfaces/application_manager.mojom.h"
#include "mojo/shell/public/interfaces/service_provider.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
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

  // Creates an ApplicationManager.
  // |package_manager| is an instance of an object that handles URL resolution,
  // fetching and updating of applications. See package_manager.h.
  explicit ApplicationManager(scoped_ptr<PackageManager> package_manager);
  // |native_runner_factory| is an instance of an object capable of vending
  // implementations of NativeRunner, e.g. for in or out-of-process execution.
  // See native_runner.h and RunNativeApplication().
  // |task_runner| provides access to a thread to perform file copy operations
  // on. This may be null only in testing environments where applications are
  // loaded via ApplicationLoader implementations.
  ApplicationManager(scoped_ptr<PackageManager> package_manager,
                     scoped_ptr<NativeRunnerFactory> native_runner_factory,
                     base::TaskRunner* task_runner);
  ~ApplicationManager();

  // Loads a service if necessary and establishes a new client connection.
  // Please see the comments in connect_to_application_params.h for more details
  // about the parameters.
  void ConnectToApplication(scoped_ptr<ConnectToApplicationParams> params);

  // Sets the default Loader to be used if not overridden by SetLoaderForURL().
  void set_default_loader(scoped_ptr<ApplicationLoader> loader) {
    default_loader_ = std::move(loader);
  }

  // Sets a Loader to be used for a specific url.
  void SetLoaderForURL(scoped_ptr<ApplicationLoader> loader, const GURL& url);

  // Destroys all Shell-ends of connections established with Applications.
  // Applications connected by this ApplicationManager will observe pipe errors
  // and have a chance to shutdown.
  void TerminateShellConnections();

  // Removes a ApplicationInstance when it encounters an error.
  void OnApplicationInstanceError(ApplicationInstance* instance);

  ApplicationInstance* GetApplicationInstance(const Identity& identity) const;

  void CreateInstanceForHandle(
      ScopedHandle channel,
      const GURL& url,
      mojom::CapabilityFilterPtr filter,
      InterfaceRequest<mojom::PIDReceiver> pid_receiver);
  void AddListener(mojom::ApplicationManagerListenerPtr listener);
  void GetRunningApplications(
      const Callback<void(Array<mojom::ApplicationInfoPtr>)>& callback);

  void ApplicationPIDAvailable(uint32_t id, base::ProcessId pid);

 private:
  using IdentityToInstanceMap = std::map<Identity, ApplicationInstance*>;
  using URLToLoaderMap = std::map<GURL, ApplicationLoader*>;

  // Takes the contents of |params| only when it returns true.
  bool ConnectToRunningApplication(
      scoped_ptr<ConnectToApplicationParams>* params);

  InterfaceRequest<mojom::Application> CreateAndConnectToInstance(
      scoped_ptr<ConnectToApplicationParams> params,
      ApplicationInstance** instance);
  InterfaceRequest<mojom::Application> CreateInstance(
      const Identity& target_id,
      const base::Closure& on_application_end,
      ApplicationInstance** resulting_instance);

  // Called once |fetcher| has found app. |params->app_url()| is the url of
  // the requested application before any mappings/resolution have been applied.
  // The corresponding URLRequest struct in |params| has been taken.
  void HandleFetchCallback(scoped_ptr<ConnectToApplicationParams> params,
                           scoped_ptr<Fetcher> fetcher);

  void RunNativeApplication(
      InterfaceRequest<mojom::Application> application_request,
      bool start_sandboxed,
      scoped_ptr<Fetcher> fetcher,
      ApplicationInstance* instance,
      const base::FilePath& file_path,
      bool path_exists);

  // Returns the appropriate loader for |url|, or the default loader if there is
  // no loader configured for the URL.
  ApplicationLoader* GetLoaderForURL(const GURL& url);

  void CleanupRunner(NativeRunner* runner);

  mojom::ApplicationInfoPtr CreateApplicationInfoForInstance(
      ApplicationInstance* instance) const;

  scoped_ptr<PackageManager> const package_manager_;
  // Loader management.
  // Loaders are chosen in the order they are listed here.
  URLToLoaderMap url_to_loader_;
  scoped_ptr<ApplicationLoader> default_loader_;

  IdentityToInstanceMap identity_to_instance_;

  WeakInterfacePtrSet<mojom::ApplicationManagerListener> listeners_;

  base::TaskRunner* task_runner_;
  scoped_ptr<NativeRunnerFactory> native_runner_factory_;
  std::vector<scoped_ptr<NativeRunner>> native_runners_;
  base::WeakPtrFactory<ApplicationManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManager);
};

mojom::Shell::ConnectToApplicationCallback EmptyConnectCallback();

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APPLICATION_MANAGER_H_
