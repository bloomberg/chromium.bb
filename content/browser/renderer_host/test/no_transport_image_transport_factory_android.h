// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEST_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_TEST_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_ANDROID_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"

namespace cc {
class ContextProvider;
}

namespace ui {
class ContextFactory;
}

namespace content {
class GLHelper;

// An ImageTransportFactoryAndroid that disables transport. This class is for
// unittests.
class NoTransportImageTransportFactoryAndroid
    : public ImageTransportFactoryAndroid {
 public:
  NoTransportImageTransportFactoryAndroid();
  virtual ~NoTransportImageTransportFactoryAndroid();
  virtual GLHelper* GetGLHelper() OVERRIDE;
  virtual uint32 GetChannelID() OVERRIDE;

 private:
  scoped_ptr<ui::ContextFactory> context_factory_;
  scoped_refptr<cc::ContextProvider> context_provider_;
  scoped_ptr<GLHelper> gl_helper_;

  DISALLOW_COPY_AND_ASSIGN(NoTransportImageTransportFactoryAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEST_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_ANDROID_H_
