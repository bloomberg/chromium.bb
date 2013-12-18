// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SERVICE_MANAGER_H_
#define MOJO_SHELL_SERVICE_MANAGER_H_

#include <map>

#include "base/callback.h"
#include "mojo/public/system/core_cpp.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class Context;

class ServiceManager {
 public:
  class Loader {
   public:
    virtual ~Loader();
    virtual void Load(const GURL& url,
                      ServiceManager* manager,
                      ScopedMessagePipeHandle service_handle) = 0;
   protected:
    Loader();
  };

  explicit ServiceManager(Context* context);
  ~ServiceManager();

  void SetLoaderForURL(Loader* loader, const GURL& gurl);
  Loader* GetLoaderForURL(const GURL& gurl);
  void Connect(const GURL& url, ScopedMessagePipeHandle client_handle);

 private:
  class Service;
  class DynamicLoader;

  Context* context_;
  scoped_ptr<Loader> default_loader_;
  typedef std::map<GURL, Service*> ServiceMap;
  ServiceMap url_to_service_;
  typedef std::map<GURL, Loader*> LoaderMap;
  LoaderMap url_to_loader_;
  DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SERVICE_MANAGER_H_
