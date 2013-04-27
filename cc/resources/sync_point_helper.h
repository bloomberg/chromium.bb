// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SYNC_POINT_HELPER_H_
#define CC_RESOURCES_SYNC_POINT_HELPER_H_

#include "base/callback.h"
#include "cc/base/cc_export.h"

namespace WebKit { class WebGraphicsContext3D; }

namespace cc {

class CC_EXPORT SyncPointHelper {
 public:
  // Requests a callback to |closure| when the |sync_point| is reached by the
  // |context3d|.
  //
  // If the |context3d| is destroyed or lost before the callback fires, then
  // AbortBecauseDidLoseOrDestroyContext() must be called to clean up the
  // callback's resources.
  static void SignalSyncPoint(
      WebKit::WebGraphicsContext3D* context3d,
      unsigned sync_point,
      const base::Closure& closure);

 private:
  SyncPointHelper();

  DISALLOW_COPY_AND_ASSIGN(SyncPointHelper);
};

}  // namespace cc

#endif  // CC_RESOURCES_SYNC_POINT_HELPER_H_
