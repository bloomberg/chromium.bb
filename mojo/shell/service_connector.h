// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SERVICE_CONNECTOR_H_
#define MOJO_SHELL_SERVICE_CONNECTOR_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "mojo/public/system/core_cpp.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ServiceConnector {
 public:
  // Interface to allowing default loading behavior to be overridden for a
  // specific url.
  class Loader {
   public:
    virtual ~Loader();
    virtual void Load(const GURL& url,
                      ScopedMessagePipeHandle service_handle) = 0;
   protected:
    Loader();
  };

  // API for testing.
  class TestAPI {
   private:
    friend class ServiceConnectorTest;
    explicit TestAPI(ServiceConnector* connector) : connector_(connector) {}
    // Returns true if there is a ServiceFactory for this URL.
    bool HasFactoryForURL(const GURL& url) const;

    ServiceConnector* connector_;
  };

  ServiceConnector();
  ~ServiceConnector();

  // Sets the default Loader to be used if not overridden by SetLoaderForURL().
  // Does not take ownership of |loader|.
  void set_default_loader(Loader* loader) { default_loader_ = loader; }
  // Sets a Loader to be used for a specific url.
  // Does not take ownership of |loader|.
  void SetLoaderForURL(Loader* loader, const GURL& gurl);
  // Returns the Loader to use for a url (using default if not overridden.)
  Loader* GetLoaderForURL(const GURL& gurl);
  // Loads a service if necessary and establishes a new client connection.
  void Connect(const GURL& url, ScopedMessagePipeHandle client_handle);
 private:
  class ServiceFactory;

  // Removes a ServiceFactory when it no longer has any connections.
  void RemoveServiceFactory(ServiceFactory* service_factory);

  Loader* default_loader_;
  typedef std::map<GURL, ServiceFactory*> ServiceFactoryMap;
  ServiceFactoryMap url_to_service_factory_;
  typedef std::map<GURL, Loader*> LoaderMap;
  LoaderMap url_to_loader_;
  DISALLOW_COPY_AND_ASSIGN(ServiceConnector);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SERVICE_CONNECTOR_H_
