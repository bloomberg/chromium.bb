// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/test/no_transport_image_transport_factory_android.h"

#include "cc/output/context_provider.h"
#include "content/common/gpu/client/gl_helper.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/in_process_context_factory.h"

namespace content {

NoTransportImageTransportFactoryAndroid::
    NoTransportImageTransportFactoryAndroid()
    : context_factory_(new ui::InProcessContextFactory) {
}

NoTransportImageTransportFactoryAndroid::
    ~NoTransportImageTransportFactoryAndroid() {
}

GLHelper* NoTransportImageTransportFactoryAndroid::GetGLHelper() {
  if (!gl_helper_) {
    context_provider_ = context_factory_->SharedMainThreadContextProvider();
    gl_helper_.reset(new GLHelper(context_provider_->ContextGL(),
                                  context_provider_->ContextSupport()));
  }
  return gl_helper_.get();
}

uint32 NoTransportImageTransportFactoryAndroid::GetChannelID() {
  NOTREACHED();
  return 0;
}

}  // namespace content
