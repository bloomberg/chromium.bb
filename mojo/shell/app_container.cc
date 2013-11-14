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
#include "mojo/public/utility/scoped_handle.h"
#include "mojo/services/native_viewport/native_viewport_controller.h"
#include "mojo/shell/context.h"

typedef MojoResult (*MojoMainFunction)(mojo::Handle pipe);

namespace mojo {
namespace shell {

void LaunchAppOnThread(
    const base::FilePath& app_path,
    Handle app_handle_raw) {
  base::ScopedClosureRunner app_deleter(
      base::Bind(base::IgnoreResult(&base::DeleteFile), app_path, false));
  ScopedHandle app_handle(app_handle_raw);

  base::ScopedNativeLibrary app_library(
      base::LoadNativeLibrary(app_path, NULL));
  if (!app_library.is_valid()) {
    LOG(ERROR) << "Failed to load library: " << app_path.value().c_str();
    return;
  }

  MojoMainFunction main_function = reinterpret_cast<MojoMainFunction>(
      app_library.GetFunctionPointer("MojoMain"));
  if (!main_function) {
    LOG(ERROR) << "Entrypoint MojoMain not found.";
    return;
  }

  MojoResult result = main_function(app_handle.get());
  if (result < MOJO_RESULT_OK) {
    LOG(ERROR) << "MojoMain returned an error: " << result;
    return;
  }

  LOG(INFO) << "MojoMain succeeded: " << result;
}

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
  Handle app_handle;
  MojoResult result = CreateMessagePipe(&shell_handle_, &app_handle);
  if (result < MOJO_RESULT_OK) {
    // Failure..
  }

  // Launch the app on its own thread.
  // TODO(beng): Create a unique thread name.
  thread_.reset(new base::Thread("app_thread"));
  thread_->Start();
  thread_->message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&LaunchAppOnThread, app_path, app_handle),
      base::Bind(&AppContainer::AppCompleted, weak_factory_.GetWeakPtr()));

  const char* hello_msg = "Hello";
  result = WriteMessage(shell_handle_, hello_msg,
                        static_cast<uint32_t>(strlen(hello_msg)+1),
                        NULL, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (result < MOJO_RESULT_OK) {
    // Failure..
  }

  // TODO(beng): This should be created on demand by the NativeViewportService
  //             when it is retrieved by the app.
  native_viewport_controller_.reset(
      new services::NativeViewportController(context_, shell_handle_));
}

void AppContainer::AppCompleted() {
  native_viewport_controller_->Close();

  thread_.reset();
  Close(shell_handle_);
}

}  // namespace shell
}  // namespace mojo
