// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "gin/public/isolate_holder.h"
#include "mojo/apps/js/bootstrap.h"
#include "mojo/apps/js/mojo_runner_delegate.h"
#include "mojo/common/bindings_support_impl.h"
#include "mojo/public/system/core_cpp.h"
#include "mojo/public/system/macros.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define MOJO_APPS_JS_EXPORT __declspec(dllexport)
#else
#define CDECL
#define MOJO_APPS_JS_EXPORT __attribute__((visibility("default")))
#endif

namespace mojo {
namespace apps {

void RunMojoJS(MojoHandle pipe) {
  gin::IsolateHolder instance;
  Bootstrap::SetInitialHandle(pipe);

  MojoRunnerDelegate delegate;
  gin::Runner runner(&delegate, instance.isolate());

  {
    gin::Runner::Scope scope(&runner);
    runner.Run("define(['mojo/apps/js/main'], function(main) {});",
               "mojo.js");
  }

  base::MessageLoop::current()->Run();
}

}  // namespace apps
}  // namespace mojo

extern "C" MOJO_APPS_JS_EXPORT MojoResult CDECL MojoMain(MojoHandle pipe) {
  base::MessageLoop loop;
  mojo::common::BindingsSupportImpl bindings_support;
  mojo::BindingsSupport::Set(&bindings_support);
  mojo::apps::RunMojoJS(pipe);
  mojo::BindingsSupport::Set(NULL);
  return MOJO_RESULT_OK;
}
