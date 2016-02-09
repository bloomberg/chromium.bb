// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_RUNNER_HOST_NATIVE_APPLICATION_SUPPORT_H_
#define MOJO_SHELL_RUNNER_HOST_NATIVE_APPLICATION_SUPPORT_H_

#include "base/native_library.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"

namespace base {
class FilePath;
}

namespace mojo {
class Application;
namespace shell {

// Loads the native Mojo application from the DSO specified by |app_path|.
// Returns the |base::NativeLibrary| for the application on success (or null on
// failure).
//
// Note: The caller may choose to eventually unload the returned DSO. If so,
// this should be done only after the thread on which |LoadNativeApplication()|
// and |RunNativeApplication()| were called has terminated, so that any
// thread-local destructors have been executed.
base::NativeLibrary LoadNativeApplication(const base::FilePath& app_path);

// Runs the native Mojo application from the DSO that was loaded using
// |LoadNativeApplication()|; this tolerates |app_library| being null. This
// should be called on the same thread as |LoadNativeApplication()|. Returns
// true if |MojoMain()| was called (even if it returns an error), and false
// otherwise.
// TODO(vtl): Maybe this should also have a |MojoResult| as an out parameter?
bool RunNativeApplication(base::NativeLibrary app_library,
                          InterfaceRequest<mojom::ShellClient> request);

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_RUNNER_HOST_NATIVE_APPLICATION_SUPPORT_H_
