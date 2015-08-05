// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_APPLICATION_MANAGER_APPLICATION_MANAGER_H_
#define SHELL_APPLICATION_MANAGER_APPLICATION_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/services/updater/updater.mojom.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/capability_filter.h"
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

class ApplicationInstance;
class ContentHandlerConnection;

class ApplicationManager {
 public:
  class Delegate {
   public:
    // Gives the delegate a chance to apply any mappings for the specified url.
    // This should not resolve 'mojo' urls, that is done by ResolveMojoURL().
    virtual GURL ResolveMappings(const GURL& url) = 0;

    // Used to map a url with the scheme 'mojo' to the appropriate url. Return
    // |url| if the scheme is not 'mojo'.
    virtual GURL ResolveMojoURL(const GURL& url) = 0;

    // Asks the delegate to create a Fetcher for the specified url. Return
    // true on success, false if the default fetcher should be created.
    virtual bool CreateFetcher(
        const GURL& url,
        const Fetcher::FetchCallback& loader_callback) = 0;

   protected:
    virtual ~Delegate() {}
  };

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

  explicit ApplicationManager(Delegate* delegate);
  ~ApplicationManager();

  // Loads a service if necessary and establishes a new client connection.
  // |originator| can be NULL (e.g. for the first application or in tests), but
  // typically is non-NULL and identifies the instance initiating the
  // connection.
  void ConnectToApplication(ApplicationInstance* originator,
                            URLRequestPtr requested_url,
                            const std::string& qualifier,
                            const GURL& requestor_url,
                            InterfaceRequest<ServiceProvider> services,
                            ServiceProviderPtr exposed_services,
                            const CapabilityFilter& capability_filter,
                            const base::Closure& on_application_end);

  // Must only be used by shell internals and test code as it does not forward
  // capability filters.
  template <typename Interface>
  inline void ConnectToService(const GURL& application_url,
                               InterfacePtr<Interface>* ptr) {
    ScopedMessagePipeHandle service_handle =
        ConnectToServiceByName(application_url, Interface::Name_);
    ptr->Bind(InterfacePtrInfo<Interface>(service_handle.Pass(), 0u));
  }

  void RegisterContentHandler(const std::string& mime_type,
                              const GURL& content_handler_url);

  // Registers a package alias. When attempting to load |alias|, it will
  // instead redirect to |content_handler_package|, which is a content handler
  // which will be passed the |alias| as the URLResponse::url. Different values
  // of |alias| with the same |qualifier| that are in the same
  // |content_handler_package| will run in the same process in multi-process
  // mode.
  void RegisterApplicationPackageAlias(const GURL& alias,
                                       const GURL& content_handler_package,
                                       const std::string& qualifier);

  // Sets the default Loader to be used if not overridden by SetLoaderForURL()
  // or SetLoaderForScheme().
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
  void set_disable_cache(bool disable_cache) { disable_cache_ = disable_cache; }
  // Sets a Loader to be used for a specific url.
  void SetLoaderForURL(scoped_ptr<ApplicationLoader> loader, const GURL& url);
  // Sets a Loader to be used for a specific url scheme.
  void SetLoaderForScheme(scoped_ptr<ApplicationLoader> loader,
                          const std::string& scheme);
  // These options will be used in running any native application at |url|
  // (which shouldn't contain a query string). (|url| will be mapped and
  // resolved, and any application whose base resolved URL matches it will have
  // |options| applied.)
  // TODO(vtl): This may not do what's desired if the resolved URL results in an
  // HTTP redirect. Really, we want options to be identified with a particular
  // implementation, maybe via a signed manifest or something like that.
  void SetNativeOptionsForURL(const NativeRunnerFactory::Options& options,
                              const GURL& url);

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
  using ApplicationPackagedAlias = std::map<GURL, std::pair<GURL, std::string>>;
  using IdentityToApplicationInstanceMap =
      std::map<Identity, ApplicationInstance*>;
  using MimeTypeToURLMap = std::map<std::string, GURL>;
  using SchemeToLoaderMap = std::map<std::string, ApplicationLoader*>;
  using URLToContentHandlerMap =
      std::map<std::pair<GURL, std::string>, ContentHandlerConnection*>;
  using URLToLoaderMap = std::map<GURL, ApplicationLoader*>;
  using URLToNativeOptionsMap = std::map<GURL, NativeRunnerFactory::Options>;

  bool ConnectToRunningApplication(ApplicationInstance* originator,
                                   const GURL& resolved_url,
                                   const std::string& qualifier,
                                   const GURL& requestor_url,
                                   InterfaceRequest<ServiceProvider>* services,
                                   ServiceProviderPtr* exposed_services,
                                   const CapabilityFilter& filter);

  bool ConnectToApplicationWithLoader(
      ApplicationInstance* originator,
      const GURL& requested_url,
      const std::string& qualifier,
      const GURL& resolved_url,
      const GURL& requestor_url,
      InterfaceRequest<ServiceProvider>* services,
      ServiceProviderPtr* exposed_services,
      const CapabilityFilter& filter,
      const base::Closure& on_application_end,
      ApplicationLoader* loader);

  InterfaceRequest<Application> RegisterInstance(
      ApplicationInstance* originator,
      const GURL& app_url,
      const std::string& qualifier,
      const GURL& requestor_url,
      InterfaceRequest<ServiceProvider> services,
      ServiceProviderPtr exposed_services,
      const CapabilityFilter& filter,
      const base::Closure& on_application_end);

  // Called once |fetcher| has found app. |requested_url| is the url of the
  // requested application before any mappings/resolution have been applied.
  void HandleFetchCallback(ApplicationInstance* originator,
                           const GURL& requested_url,
                           const std::string& qualifier,
                           const GURL& requestor_url,
                           InterfaceRequest<ServiceProvider> services,
                           ServiceProviderPtr exposed_services,
                           const CapabilityFilter& filter,
                           const base::Closure& on_application_end,
                           NativeApplicationCleanup cleanup,
                           scoped_ptr<Fetcher> fetcher);

  void RunNativeApplication(InterfaceRequest<Application> application_request,
                            bool start_sandboxed,
                            const NativeRunnerFactory::Options& options,
                            NativeApplicationCleanup cleanup,
                            scoped_ptr<Fetcher> fetcher,
                            const base::FilePath& file_path,
                            bool path_exists);

  void LoadWithContentHandler(ApplicationInstance* originator,
                              const GURL& content_handler_url,
                              const GURL& requestor_url,
                              const std::string& qualifier,
                              const CapabilityFilter& filter,
                              InterfaceRequest<Application> application_request,
                              URLResponsePtr url_response);

  // Returns the appropriate loader for |url|, or null if there is no loader
  // configured for the URL.
  ApplicationLoader* GetLoaderForURL(const GURL& url);

  void CleanupRunner(NativeRunner* runner);

  ScopedMessagePipeHandle ConnectToServiceByName(
      const GURL& application_url,
      const std::string& interface_name);

  Delegate* const delegate_;
  // Loader management.
  // Loaders are chosen in the order they are listed here.
  URLToLoaderMap url_to_loader_;
  SchemeToLoaderMap scheme_to_loader_;
  scoped_ptr<ApplicationLoader> default_loader_;
  scoped_ptr<NativeRunnerFactory> native_runner_factory_;

  ApplicationPackagedAlias application_package_alias_;
  IdentityToApplicationInstanceMap identity_to_instance_;
  URLToContentHandlerMap url_to_content_handler_;
  // Note: The keys are URLs after mapping and resolving.
  URLToNativeOptionsMap url_to_native_options_;

  base::SequencedWorkerPool* blocking_pool_;
  NetworkServicePtr network_service_;
  URLLoaderFactoryPtr url_loader_factory_;
  updater::UpdaterPtr updater_;
  MimeTypeToURLMap mime_type_to_url_;
  ScopedVector<NativeRunner> native_runners_;
  bool disable_cache_;
  base::WeakPtrFactory<ApplicationManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManager);
};

}  // namespace shell
}  // namespace mojo

#endif  // SHELL_APPLICATION_MANAGER_APPLICATION_MANAGER_H_
