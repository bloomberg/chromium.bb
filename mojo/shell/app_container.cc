// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/app_container.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "mojo/public/system/core.h"
#include "mojo/services/native_viewport/native_viewport_controller.h"
#include "mojo/shell/context.h"

typedef MojoResult (*MojoMainFunction)(MojoHandle pipe);

namespace mojo {
namespace shell {

AppContainer::AppContainer(Context* context)
    : context_(context),
      weak_factory_(this) {
}

AppContainer::~AppContainer() {
}

void AppContainer::Load(const GURL& app_url) {
  request_ = context_->loader()->Load(app_url, this);
}

void AppContainer::DidCompleteLoad(const GURL& app_url,
                                   const base::FilePath& app_path) {
  CreateMessagePipe(&shell_handle_, &app_handle_);

  hello_world_service_.reset(
    new examples::HelloWorldServiceImpl(shell_handle_.Pass()));

  // Launch the app on its own thread.
  // TODO(beng): Create a unique thread name.
  app_path_ = app_path;
  ack_closure_ =
      base::Bind(&AppContainer::AppCompleted, weak_factory_.GetWeakPtr());
  thread_.reset(new base::DelegateSimpleThread(this, "app_thread"));
  thread_->Start();

  // TODO(beng): This should be created on demand by the NativeViewportService
  //             when it is retrieved by the app.
  // native_viewport_controller_.reset(
  //     new services::NativeViewportController(context_, shell_handle_));
}

void AppContainer::Run() {
  base::ScopedClosureRunner app_deleter(
      base::Bind(base::IgnoreResult(&base::DeleteFile), app_path_, false));
  base::ScopedNativeLibrary app_library(
      base::LoadNativeLibrary(app_path_, NULL));
  if (!app_library.is_valid()) {
    LOG(ERROR) << "Failed to load library: " << app_path_.value().c_str();
    return;
  }

  MojoMainFunction main_function = reinterpret_cast<MojoMainFunction>(
      app_library.GetFunctionPointer("MojoMain"));
  if (!main_function) {
    LOG(ERROR) << "Entrypoint MojoMain not found.";
    return;
  }

  // |MojoMain()| takes ownership of the app handle.
  MojoResult result = main_function(app_handle_.release().value());
  if (result < MOJO_RESULT_OK) {
    LOG(ERROR) << "MojoMain returned an error: " << result;
    return;
  }
  LOG(INFO) << "MojoMain succeeded: " << result;
  context_->task_runners()->ui_runner()->PostTask(FROM_HERE, ack_closure_);
}

void AppContainer::AppCompleted() {
  hello_world_service_.reset();
  // TODO(aa): This code gets replaced once we have a service manager.
  // native_viewport_controller_->Close();

  thread_->Join();
  thread_.reset();
}

}  // namespace shell
}  // namespace mojo
