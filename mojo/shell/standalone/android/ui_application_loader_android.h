// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STANDALONE_ANDROID_UI_APPLICATION_LOADER_ANDROID_H_
#define MOJO_SHELL_STANDALONE_ANDROID_UI_APPLICATION_LOADER_ANDROID_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/application_loader.h"

namespace base {
class MessageLoop;
}

namespace mojo {
namespace shell {

class ApplicationManager;

// ApplicationLoader implementation that creates a background thread and issues
// load
// requests there.
class UIApplicationLoader : public ApplicationLoader {
 public:
  UIApplicationLoader(scoped_ptr<ApplicationLoader> real_loader,
                      base::MessageLoop* ui_message_loop);
  ~UIApplicationLoader() override;

  // ApplicationLoader overrides:
  void Load(const GURL& url,
            InterfaceRequest<Application> application_request) override;

 private:
  class UILoader;

  // These functions are exected on the background thread. They call through
  // to |background_loader_| to do the actual loading.
  // TODO: having this code take a |manager| is fragile (as ApplicationManager
  // isn't thread safe).
  void LoadOnUIThread(const GURL& url,
                      InterfaceRequest<Application> application_request);
  void ShutdownOnUIThread();

  scoped_ptr<ApplicationLoader> loader_;
  base::MessageLoop* ui_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(UIApplicationLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_STANDALONE_ANDROID_UI_APPLICATION_LOADER_ANDROID_H_
