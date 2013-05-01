// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_ANDROID_SYNCHRONOUS_COMPOSTIOR_CLIENT_H_
#define CONTENT_PUBLIC_RENDERER_ANDROID_SYNCRHONOUS_COMPOSITOR_CLIENT_H_

namespace content {

class SynchronousCompositor;

class SynchronousCompositorClient {
 public:
  // Indication to the client that |compositor| is going out of scope, and
  // must not be accessed within or after this call.
  // NOTE if the client goes away before the compositor it must call
  // SynchronousCompositor::SetClient(NULL) to release the back pointer.
  virtual void DidDestroyCompositor(SynchronousCompositor* compositor) = 0;

  // TODO(joth): Add scroll getters and setters, and invalidations.

 protected:
  virtual ~SynchronousCompositorClient() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_ANDROID_SYNCHRONOUS_COMPOSTIOR_CLIENT_H_
