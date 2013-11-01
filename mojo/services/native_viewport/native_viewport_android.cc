// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_android.h"

#include "mojo/services/native_viewport/android/mojo_viewport.h"
#include "mojo/shell/context.h"

namespace mojo {
namespace services {

NativeViewportAndroid::NativeViewportAndroid(NativeViewportDelegate* delegate)
    : delegate_(delegate),
      weak_factory_(this) {
}

NativeViewportAndroid::~NativeViewportAndroid() {
}

void NativeViewportAndroid::Close() {
  // TODO(beng): close activity containing MojoView?

  // TODO(beng): perform this in response to view destruction.
  delegate_->OnDestroyed();
}

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    shell::Context* context,
    NativeViewportDelegate* delegate) {
  scoped_ptr<NativeViewportAndroid> native_viewport(
      new NativeViewportAndroid(delegate));

  MojoViewportInit* init = new MojoViewportInit();
  init->ui_runner = context->task_runners()->ui_runner();
  init->native_viewport = native_viewport->GetWeakPtr();

  context->task_runners()->java_runner()->PostTask(FROM_HERE,
      base::Bind(MojoViewport::CreateForActivity,
                 context->activity(),
                 init));

   return scoped_ptr<NativeViewport>(native_viewport.Pass());
}

}  // namespace services
}  // namespace mojo
