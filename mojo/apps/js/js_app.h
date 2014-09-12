// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_JS_APP_H_
#define MOJO_APPS_JS_JS_APP_H_

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
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
//
// The lifetime of this class is defined by the content handler's internal
// "app_vector" list.

class JSApp {
 public:
  JSApp(ApplicationDelegateImpl* content_handler_app,
        const std::string& url,
        URLResponsePtr content);
  ~JSApp();

  bool Start();
  void Quit();
  Handle ConnectToService(const std::string& application_url,
                          const std::string& interface_name);

 private:
  void Run();
  void Terminate();

  // Used to CHECK that we're running on the correct thread.
  bool on_content_handler_thread() const;
  bool on_js_app_thread() const;

  ApplicationDelegateImpl* content_handler_app_;
  std::string url_;
  URLResponsePtr content_;
  base::Thread thread_;
  scoped_refptr<base::SingleThreadTaskRunner> content_handler_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> js_app_task_runner_;
  MojoRunnerDelegate runner_delegate_;
  scoped_ptr<gin::IsolateHolder> isolate_holder_;
  scoped_ptr<gin::ShellRunner> shell_runner_;

  DISALLOW_COPY_AND_ASSIGN(JSApp);
};

}  // namespace apps
}  // namespace mojo

#endif  // MOJO_APPS_JS_JS_APP_H_
