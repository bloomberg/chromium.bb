// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/external_application_listener_win.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"

namespace mojo {
namespace shell {

// static
base::FilePath ExternalApplicationListener::ConstructDefaultSocketPath() {
  return base::FilePath(FILE_PATH_LITERAL(""));
}

// static
scoped_ptr<ExternalApplicationListener> ExternalApplicationListener::Create(
    const scoped_refptr<base::SequencedTaskRunner>& shell_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_runner) {
  return make_scoped_ptr(new ExternalApplicationListenerStub);
}

ExternalApplicationListenerStub::ExternalApplicationListenerStub() {
}

ExternalApplicationListenerStub::~ExternalApplicationListenerStub() {
}

void ExternalApplicationListenerStub::ListenInBackground(
    const base::FilePath& listen_socket_path,
    const RegisterCallback& register_callback) {
}

void ExternalApplicationListenerStub::ListenInBackgroundWithErrorCallback(
    const base::FilePath& listen_socket_path,
    const RegisterCallback& register_callback,
    const ErrorCallback& error_callback) {
}

void ExternalApplicationListenerStub::WaitForListening() {
}

}  // namespace shell
}  // namespace mojo
