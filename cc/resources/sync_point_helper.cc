// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/sync_point_helper.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace cc {

class SignalSyncPointCallbackClass
    : public WebKit::WebGraphicsContext3D::WebGraphicsSyncPointCallback {
 public:
  explicit SignalSyncPointCallbackClass(const base::Closure& closure)
      : closure_(closure) {}

  virtual void onSyncPointReached() {
    if (!closure_.is_null())
      closure_.Run();
  }

 private:
  base::Closure closure_;
};

void SyncPointHelper::SignalSyncPoint(
    WebKit::WebGraphicsContext3D* context3d,
    unsigned sync_point,
    const base::Closure& closure) {
  SignalSyncPointCallbackClass* callback_class =
      new SignalSyncPointCallbackClass(closure);

  // Pass ownership of the CallbackClass to WebGraphicsContext3D.
  context3d->signalSyncPoint(sync_point, callback_class);
}

}  // namespace cc
