// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_JS_APP_H_
#define MOJO_APPS_JS_JS_APP_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "gin/public/isolate_holder.h"
#include "gin/shell_runner.h"
#include "mojo/apps/js/mojo_runner_delegate.h"
#include "mojo/services/public/interfaces/content_handler/content_handler.mojom.h"

namespace mojo {
namespace apps {

class JSApp;
class ApplicationDelegateImpl;

// Each JavaScript app started by content handler runs on its own thread and
// in its own V8 isolate. This class represents one running JS app.

class JSApp {
 public:
  JSApp(ApplicationDelegateImpl* app_delegate_impl);
  virtual ~JSApp();

  // Launch this app on a new thread. This method can be called on any thread.
  // This method causes Load() and then Run() to run on a new thread.
  bool Start();


  // Subclasses must return the JS source code for this app's main script and
  // the filename or URL that identifies the script's origin. This method will
  // be called from this app's thread.
  virtual bool Load(std::string* source, std::string* file_name) = 0;

  // Called by the JS mojo module to quit this JS app. See mojo_module.cc.
  void Quit();

  // Called by the JS mojo module to connect to a Mojo service.
  Handle ConnectToService(const std::string& application_url,
                          const std::string& interface_name);

 private:
  void Run();
  void Terminate();

  // Used to CHECK that we're running on the correct thread.
  bool on_app_delegate_impl_thread() const;
  bool on_js_app_thread() const;

  ApplicationDelegateImpl* app_delegate_impl_;
  base::Thread thread_;
  scoped_refptr<base::SingleThreadTaskRunner> app_delegate_impl_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> js_app_task_runner_;
  MojoRunnerDelegate runner_delegate_;
  scoped_ptr<gin::IsolateHolder> isolate_holder_;
  scoped_ptr<gin::ShellRunner> shell_runner_;

  DISALLOW_COPY_AND_ASSIGN(JSApp);
};

}  // namespace apps
}  // namespace mojo

#endif  // MOJO_APPS_JS_JS_APP_H_
