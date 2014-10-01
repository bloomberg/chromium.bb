// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/apps/js/application_delegate_impl.h"
#include "mojo/apps/js/js_app.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"

namespace mojo {
namespace apps {

class StandaloneJSApp : public JSApp {
 public:
  StandaloneJSApp(ApplicationDelegateImpl* app_delegate_impl,
                  const base::FilePath& path)
      : JSApp(app_delegate_impl),
        path_(path) {
  }

  virtual bool Load(std::string* source, std::string* file_name) override {
    *file_name = path_.AsUTF8Unsafe();
    return ReadFileToString(path_, source);
  }

 private:
  base::FilePath path_;
};

class StandaloneApplicationDelegateImpl : public ApplicationDelegateImpl {
 private:
  virtual void Initialize(ApplicationImpl* app) override {
    ApplicationDelegateImpl::Initialize(app);

    // TODO(hansmuller): Find the JS source code path in app->args().
    base::FilePath path(base::FilePath::FromUTF8Unsafe(
        "/builds/dev/src/mojo/apps/js/main.js"));

    scoped_ptr<JSApp> js_app(new StandaloneJSApp(this, path));
    StartJSApp(js_app.Pass());
  }
};

}  // namespace apps
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  base::i18n::InitializeICU();
  mojo::ApplicationRunnerChromium runner(
      new mojo::apps::StandaloneApplicationDelegateImpl());
  return runner.Run(shell_handle);
}
