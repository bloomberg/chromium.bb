// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_UI_APPLICATION_LOADER_ANDROID_H_
#define MOJO_SHELL_UI_APPLICATION_LOADER_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/application_manager/application_loader.h"

namespace mojo {

class ApplicationManager;

namespace shell {
class Context;
}

// ApplicationLoader implementation that creates a background thread and issues
// load
// requests there.
class UIApplicationLoader : public ApplicationLoader {
 public:
  UIApplicationLoader(scoped_ptr<ApplicationLoader> real_loader,
                      shell::Context* context);
  virtual ~UIApplicationLoader();

  // ApplicationLoader overrides:
  virtual void Load(ApplicationManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE;
  virtual void OnServiceError(ApplicationManager* manager,
                              const GURL& url) OVERRIDE;

 private:
  class UILoader;

  // These functions are exected on the background thread. They call through
  // to |background_loader_| to do the actual loading.
  // TODO: having this code take a |manager| is fragile (as ApplicationManager
  // isn't thread safe).
  void LoadOnUIThread(ApplicationManager* manager,
                      const GURL& url,
                      ScopedMessagePipeHandle* shell_handle);
  void OnServiceErrorOnUIThread(ApplicationManager* manager, const GURL& url);
  void ShutdownOnUIThread();

  scoped_ptr<ApplicationLoader> loader_;
  shell::Context* context_;

  // Lives on the UI thread. Trivial interface that calls through to |loader_|.
  UILoader* ui_loader_;

  DISALLOW_COPY_AND_ASSIGN(UIApplicationLoader);
};

}  // namespace mojo

#endif  // MOJO_SHELL_UI_APPLICATION_LOADER_ANDROID_H_
