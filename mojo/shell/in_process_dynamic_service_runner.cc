// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/in_process_dynamic_service_runner.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "mojo/public/platform/native/gles2_impl_chromium_sync_point_thunks.h"
#include "mojo/public/platform/native/gles2_impl_chromium_texture_mailbox_thunks.h"
#include "mojo/public/platform/native/gles2_impl_thunks.h"
#include "mojo/public/platform/native/gles2_thunks.h"
#include "mojo/public/platform/native/system_thunks.h"

namespace mojo {
namespace shell {

namespace {

template <typename Thunks>
bool SetThunks(Thunks (*make_thunks)(),
               const char* function_name,
               base::ScopedNativeLibrary* library) {
  typedef size_t (*SetThunksFn)(const Thunks* thunks);
  SetThunksFn set_thunks =
      reinterpret_cast<SetThunksFn>(library->GetFunctionPointer(function_name));
  if (!set_thunks)
    return false;
  Thunks thunks = make_thunks();
  size_t expected_size = set_thunks(&thunks);
  if (expected_size > sizeof(Thunks)) {
    LOG(ERROR) << "Invalid app library: expected " << function_name
               << " to return thunks of size: " << expected_size;
    return false;
  }
  return true;
}
}

InProcessDynamicServiceRunner::InProcessDynamicServiceRunner(
    Context* context) {
}

InProcessDynamicServiceRunner::~InProcessDynamicServiceRunner() {
  if (thread_) {
    DCHECK(thread_->HasBeenStarted());
    DCHECK(!thread_->HasBeenJoined());
    thread_->Join();
  }

  // It is important to let the thread exit before unloading the DSO because
  // the library may have registered thread-local data and destructors to run
  // on thread termination.
  app_library_.Reset(base::NativeLibrary());
}

void InProcessDynamicServiceRunner::Start(
    const base::FilePath& app_path,
    ScopedMessagePipeHandle service_handle,
    const base::Closure& app_completed_callback) {
  app_path_ = app_path;

  DCHECK(!service_handle_.is_valid());
  service_handle_ = service_handle.Pass();

  DCHECK(app_completed_callback_runner_.is_null());
  app_completed_callback_runner_ = base::Bind(&base::TaskRunner::PostTask,
                                              base::MessageLoopProxy::current(),
                                              FROM_HERE,
                                              app_completed_callback);

  DCHECK(!thread_);
  thread_.reset(new base::DelegateSimpleThread(this, "app_thread"));
  thread_->Start();
}

void InProcessDynamicServiceRunner::Run() {
  DVLOG(2) << "Loading/running Mojo app in process from library: "
           << app_path_.value();

  do {
    base::NativeLibraryLoadError error;
    app_library_.Reset(base::LoadNativeLibrary(app_path_, &error));
    if (!app_library_.is_valid()) {
      LOG(ERROR) << "Failed to load app library (error: " << error.ToString()
                 << ")";
      break;
    }

    if (!SetThunks(
            &MojoMakeSystemThunks, "MojoSetSystemThunks", &app_library_)) {
      // In the component build, Mojo Apps link against mojo_system_impl.
#if !defined(COMPONENT_BUILD)
      // Strictly speaking this is not required, but it's very unusual to have
      // an app that doesn't require the basic system library.
      LOG(WARNING) << "MojoSetSystemThunks not found in app library";
#endif
    }

    if (SetThunks(&MojoMakeGLES2ControlThunks,
                  "MojoSetGLES2ControlThunks",
                  &app_library_)) {
      // If we have the control thunks, we probably also have the
      // GLES2 implementation thunks.
      if (!SetThunks(&MojoMakeGLES2ImplThunks,
                     "MojoSetGLES2ImplThunks",
                     &app_library_)) {
        // In the component build, Mojo Apps link against mojo_gles2_impl.
#if !defined(COMPONENT_BUILD)
        // Warn on this really weird case: The library requires the GLES2
        // control functions, but doesn't require the GLES2 implementation.
        LOG(WARNING) << app_path_.value()
                     << " has MojoSetGLES2ControlThunks, "
                        "but doesn't have MojoSetGLES2ImplThunks.";
#endif
      }

      // If the application is using GLES2 extension points, register those
      // thunks. Applications may use or not use any of these, so don't warn if
      // they are missing.
      SetThunks(MojoMakeGLES2ImplChromiumTextureMailboxThunks,
                "MojoSetGLES2ImplChromiumTextureMailboxThunks",
                &app_library_);
      SetThunks(MojoMakeGLES2ImplChromiumSyncPointThunks,
                "MojoSetGLES2ImplChromiumSyncPointThunks",
                &app_library_);
    }
    // Unlike system thunks, we don't warn on a lack of GLES2 thunks because
    // not everything is a visual app.

    typedef MojoResult (*MojoMainFunction)(MojoHandle);
    MojoMainFunction main_function = reinterpret_cast<MojoMainFunction>(
        app_library_.GetFunctionPointer("MojoMain"));
    if (!main_function) {
      LOG(ERROR) << "Entrypoint MojoMain not found: " << app_path_.value();
      break;
    }

    // |MojoMain()| takes ownership of the service handle.
    MojoResult result = main_function(service_handle_.release().value());
    if (result < MOJO_RESULT_OK)
      LOG(ERROR) << "MojoMain returned an error: " << result;
  } while (false);

  app_completed_callback_runner_.Run();
  app_completed_callback_runner_.Reset();
}

}  // namespace shell
}  // namespace mojo
