// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_UI_SERVICE_LOADER_ANDROID_H_
#define MOJO_SHELL_UI_SERVICE_LOADER_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/service_manager/service_loader.h"

namespace mojo {

class ServiceManager;

namespace shell {
class Context;
}

// ServiceLoader implementation that creates a background thread and issues load
// requests there.
class UIServiceLoader : public ServiceLoader {
 public:
  UIServiceLoader(scoped_ptr<ServiceLoader> real_loader,
                  shell::Context* context);
  virtual ~UIServiceLoader();

  // ServiceLoader overrides:
  virtual void Load(ServiceManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE;
  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE;

 private:
  class UILoader;

  // These functions are exected on the background thread. They call through
  // to |background_loader_| to do the actual loading.
  // TODO: having this code take a |manager| is fragile (as ServiceManager isn't
  // thread safe).
  void LoadOnUIThread(ServiceManager* manager,
                      const GURL& url,
                      ScopedMessagePipeHandle* shell_handle);
  void OnServiceErrorOnUIThread(ServiceManager* manager, const GURL& url);
  void ShutdownOnUIThread();

  scoped_ptr<ServiceLoader> loader_;
  shell::Context* context_;

  // Lives on the UI thread. Trivial interface that calls through to |loader_|.
  UILoader* ui_loader_;

  DISALLOW_COPY_AND_ASSIGN(UIServiceLoader);
};

}  // namespace mojo

#endif  // MOJO_SHELL_UI_SERVICE_LOADER_ANDROID_H_
