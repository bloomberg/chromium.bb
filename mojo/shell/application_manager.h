// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APPLICATION_MANAGER_H_
#define MOJO_SHELL_APPLICATION_MANAGER_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"
#include "mojo/public/cpp/bindings/weak_interface_ptr_set.h"
#include "mojo/services/package_manager/public/interfaces/shell_resolver.mojom.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/identity.h"
#include "mojo/shell/native_runner.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/application_manager.mojom.h"
#include "mojo/shell/public/interfaces/interface_provider.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"
#include "mojo/shell/public/interfaces/shell_client_factory.mojom.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class SequencedWorkerPool;
}

namespace mojo {
namespace shell {

class ApplicationInstance;
class ShellClientFactoryConnection;

class ApplicationManager : public ShellClient,
                           public InterfaceFactory<mojom::ApplicationManager>,
                           public mojom::ApplicationManager {
 public:
  // API for testing.
  class TestAPI {
   public:
    explicit TestAPI(ApplicationManager* manager);
    ~TestAPI();

    // Returns true if there is a ApplicationInstance for this URL.
    bool HasRunningInstanceForURL(const GURL& url) const;
   private:
    ApplicationManager* manager_;

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  // Creates an ApplicationManager.
  explicit ApplicationManager(bool register_mojo_url_schemes);
  // |native_runner_factory| is an instance of an object capable of vending
  // implementations of NativeRunner, e.g. for in or out-of-process execution.
  // See native_runner.h and RunNativeApplication().
  // |task_runner| provides access to a thread to perform file copy operations
  // on. This may be null only in testing environments where applications are
  // loaded via ApplicationLoader implementations.
  // When |register_mojo_url_schemes| is true, mojo: and exe: URL schems are
  // registered as "standard" which faciliates resolving.
  ApplicationManager(scoped_ptr<NativeRunnerFactory> native_runner_factory,
                     base::TaskRunner* task_runner,
                     bool register_mojo_url_schemes);
  ~ApplicationManager() override;

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

  void ApplicationPIDAvailable(uint32_t id, base::ProcessId pid);

 private:
  using IdentityToInstanceMap = std::map<Identity, ApplicationInstance*>;
  using URLToLoaderMap = std::map<GURL, ApplicationLoader*>;
  using IdentityToShellClientFactoryMap =
      std::map<Identity, mojom::ShellClientFactoryPtr>;

  // ShellClient:
  bool AcceptConnection(Connection* connection) override;

  // InterfaceFactory<mojom::ApplicationManager>:
  void Create(
      Connection* connection,
      InterfaceRequest<mojom::ApplicationManager> request) override;

  // mojom::ApplicationManager:
  void CreateInstanceForHandle(
      ScopedHandle channel,
      const String& url,
      mojom::CapabilityFilterPtr filter,
      InterfaceRequest<mojom::PIDReceiver> pid_receiver) override;
  void AddListener(
      mojom::ApplicationManagerListenerPtr listener) override;

  // Takes the contents of |params| only when it returns true.
  bool ConnectToRunningApplication(
      scoped_ptr<ConnectToApplicationParams>* params);

  ApplicationInstance* CreateAndConnectToInstance(
      scoped_ptr<ConnectToApplicationParams> params,
      Identity* source,
      Identity* target,
      const std::string& application_name,
      mojom::ShellClientRequest* request);
  ApplicationInstance* CreateInstance(
      const Identity& target_id,
      const mojom::Shell::ConnectToApplicationCallback& connect_callback,
      const base::Closure& on_application_end,
      const String& application_name,
      mojom::ShellClientRequest* request);

  void CreateShellClient(const Identity& source,
                         const Identity& shell_client_factory,
                         const GURL& url,
                         mojom::ShellClientRequest request);
  // Returns a running ShellClientFactory for |shell_client_factory_identity|,
  // if there is not one running one is started for |source_identity|.
  mojom::ShellClientFactory* GetShellClientFactory(
      const Identity& shell_client_factory_identity,
      const Identity& source_identity);
  void OnShellClientFactoryLost(const Identity& which);;

  // Callback when remote PackageManager resolves mojo:foo to mojo:bar.
  // |params| are the params passed to Connect().
  // |resolved_url| is the mojo: url identifying the physical package
  // application.
  // |file_url| is the resolved file:// URL of the physical package.
  // |application_name| is the requested application's pretty name, from its
  // manifest.
  // |base_filter| is the CapabilityFilter the requested application should be
  // run with, from its manifest.
  void OnGotResolvedURL(scoped_ptr<ConnectToApplicationParams> params,
                        const String& resolved_url,
                        const String& file_url,
                        const String& application_name,
                        mojom::CapabilityFilterPtr base_filter);

  // In response to a request via Connect() with |params|, creates an
  // ApplicationInstance and runs the application at |file_url|.
  void CreateAndRunLocalApplication(
      scoped_ptr<ConnectToApplicationParams> params,
      const String& application_name,
      const GURL& file_url);

  void RunNativeApplication(InterfaceRequest<mojom::ShellClient> request,
                            bool start_sandboxed,
                            ApplicationInstance* instance,
                            const base::FilePath& file_path);

  // Returns the appropriate loader for |url|, or the default loader if there is
  // no loader configured for the URL.
  ApplicationLoader* GetLoaderForURL(const GURL& url);

  void CleanupRunner(NativeRunner* runner);

  mojom::ApplicationInfoPtr CreateApplicationInfoForInstance(
      ApplicationInstance* instance) const;

  package_manager::mojom::ShellResolverPtr shell_resolver_;

  // Loader management.
  // Loaders are chosen in the order they are listed here.
  URLToLoaderMap url_to_loader_;
  scoped_ptr<ApplicationLoader> default_loader_;

  IdentityToInstanceMap identity_to_instance_;

  IdentityToShellClientFactoryMap shell_client_factories_;
  // Counter used to assign ids to content handlers.
  uint32_t shell_client_factory_id_counter_;

  WeakInterfacePtrSet<mojom::ApplicationManagerListener> listeners_;

  base::TaskRunner* task_runner_;
  scoped_ptr<NativeRunnerFactory> native_runner_factory_;
  std::vector<scoped_ptr<NativeRunner>> native_runners_;
  WeakBindingSet<mojom::ApplicationManager> bindings_;
  base::WeakPtrFactory<ApplicationManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManager);
};

mojom::Shell::ConnectToApplicationCallback EmptyConnectCallback();

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APPLICATION_MANAGER_H_
