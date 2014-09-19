// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPLICATION_MANAGER_APPLICATION_MANAGER_H_
#define MOJO_APPLICATION_MANAGER_APPLICATION_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/application_manager/application_loader.h"
#include "mojo/application_manager/application_manager_export.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "url/gurl.h"

namespace mojo {

class MOJO_APPLICATION_MANAGER_EXPORT ApplicationManager {
 public:
  class MOJO_APPLICATION_MANAGER_EXPORT Delegate {
   public:
    virtual ~Delegate();
    // Send when the Applicaiton holding the handle on the other end of the
    // Shell pipe goes away.
    virtual void OnApplicationError(const GURL& url) = 0;
  };

  // API for testing.
  class MOJO_APPLICATION_MANAGER_EXPORT TestAPI {
   public:
    explicit TestAPI(ApplicationManager* manager);
    ~TestAPI();

    // Returns true if the shared instance has been created.
    static bool HasCreatedInstance();
    // Returns true if there is a ShellImpl for this URL.
    bool HasFactoryForURL(const GURL& url) const;

   private:
    ApplicationManager* manager_;

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  // Interface class for debugging only.
  class Interceptor {
   public:
    virtual ~Interceptor() {}
    // Called when ApplicationManager::Connect is called.
    virtual ServiceProviderPtr OnConnectToClient(
        const GURL& url,
        ServiceProviderPtr service_provider) = 0;
  };

  ApplicationManager();
  ~ApplicationManager();

  // Returns a shared instance, creating it if necessary.
  static ApplicationManager* GetInstance();

  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }

  // Loads a service if necessary and establishes a new client connection.
  void ConnectToApplication(const GURL& application_url,
                            const GURL& requestor_url,
                            ServiceProviderPtr service_provider);

  template <typename Interface>
  inline void ConnectToService(const GURL& application_url,
                               InterfacePtr<Interface>* ptr) {
    ScopedMessagePipeHandle service_handle =
        ConnectToServiceByName(application_url, Interface::Name_);
    ptr->Bind(service_handle.Pass());
  }

  ScopedMessagePipeHandle ConnectToServiceByName(
      const GURL& application_url,
      const std::string& interface_name);

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Sets the default Loader to be used if not overridden by SetLoaderForURL()
  // or SetLoaderForScheme().
  void set_default_loader(scoped_ptr<ApplicationLoader> loader) {
    default_loader_ = loader.Pass();
  }
  // Sets a Loader to be used for a specific url.
  void SetLoaderForURL(scoped_ptr<ApplicationLoader> loader, const GURL& url);
  // Sets a Loader to be used for a specific url scheme.
  void SetLoaderForScheme(scoped_ptr<ApplicationLoader> loader,
                          const std::string& scheme);
  // These strings will be passed to the Initialize() method when an
  // Application is instantiated.
  void SetArgsForURL(const std::vector<std::string>& args, const GURL& url);

  // Allows to interpose a debugger to service connections.
  void SetInterceptor(Interceptor* interceptor);

  // Destroys all Shell-ends of connections established with Applications.
  // Applications connected by this ApplicationManager will observe pipe errors
  // and have a chance to shutdown.
  void TerminateShellConnections();

 private:
  struct ContentHandlerConnection;
  class LoadCallbacksImpl;
  class ShellImpl;

  typedef std::map<std::string, ApplicationLoader*> SchemeToLoaderMap;
  typedef std::map<GURL, ApplicationLoader*> URLToLoaderMap;
  typedef std::map<GURL, ShellImpl*> URLToShellImplMap;
  typedef std::map<GURL, ContentHandlerConnection*> URLToContentHandlerMap;
  typedef std::map<GURL, std::vector<std::string> > URLToArgsMap;

  void ConnectToClient(ShellImpl* shell_impl,
                       const GURL& url,
                       const GURL& requestor_url,
                       ServiceProviderPtr service_provider);

  void RegisterLoadedApplication(const GURL& service_url,
                                 const GURL& requestor_url,
                                 ServiceProviderPtr service_provider,
                                 ScopedMessagePipeHandle* shell_handle);

  void LoadWithContentHandler(const GURL& content_url,
                              const GURL& requestor_url,
                              const GURL& content_handler_url,
                              URLResponsePtr url_response,
                              ServiceProviderPtr service_provider);

  // Returns the Loader to use for a url (using default if not overridden.)
  // The preference is to use a loader that's been specified for an url first,
  // then one that's been specified for a scheme, then the default.
  ApplicationLoader* GetLoaderForURL(const GURL& url);

  // Removes a ShellImpl when it encounters an error.
  void OnShellImplError(ShellImpl* shell_impl);

  Delegate* delegate_;
  // Loader management.
  URLToLoaderMap url_to_loader_;
  SchemeToLoaderMap scheme_to_loader_;
  scoped_ptr<ApplicationLoader> default_loader_;
  Interceptor* interceptor_;

  URLToShellImplMap url_to_shell_impl_;
  URLToContentHandlerMap url_to_content_handler_;
  URLToArgsMap url_to_args_;

  base::WeakPtrFactory<ApplicationManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManager);
};

}  // namespace mojo

#endif  // MOJO_APPLICATION_MANAGER_APPLICATION_MANAGER_H_
