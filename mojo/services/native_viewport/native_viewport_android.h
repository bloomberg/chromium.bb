// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_ANDROID_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "mojo/services/native_viewport/native_viewport.h"

namespace mojo {
namespace services {

class NativeViewportAndroid : public NativeViewport {
 public:
  explicit NativeViewportAndroid(NativeViewportDelegate* delegate);
  virtual ~NativeViewportAndroid();

  base::WeakPtr<NativeViewportAndroid> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Overridden from NativeViewport:
  virtual void Close() OVERRIDE;

  NativeViewportDelegate* delegate_;

  base::WeakPtrFactory<NativeViewportAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportAndroid);
};


}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_ANDROID_H_
