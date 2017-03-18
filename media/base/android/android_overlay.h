// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_ANDROID_OVERLAY_H_
#define MEDIA_BASE_ANDROID_ANDROID_OVERLAY_H_

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

// Client interface to an AndroidOverlay.  Once constructed, you can expect to
// receive either a call to ReadyCB or a call to FailedCB to indicate whether
// the overlay is ready, or isn't going to be ready.  If one does get ReadyCB,
// then one may GetJavaSurface() to retrieve the java Surface object.  One
// will get DestroyedCB eventually after ReadyCB, assuming that one doesn't
// delete the overlay before that.
// When DestroyedCB arrives, you should stop using the Android Surface and
// delete the AndroidOverlay instance.  Currently, the exact contract depends
// on the concrete implementation.  Once ContentVideoView is deprecated, it will
// be: it is not guaranteed that any AndroidOverlay instances will operate until
// the destroyed instance is deleted.  This must happen on the thread that was
// used to create it.  It does not have to happen immediately, or before the
// callback returns.
// With CVV, one must still delete the overlay on the main thread, and it
// doesn't have to happen before this returns.  However, one must signal the
// CVV onSurfaceDestroyed handler on some thread before returning from the
// callback.  AVDACodecAllocator::ReleaseMediaCodec handles signaling.  The
// fundamental difference is that CVV blocks the UI thread in the browser, which
// makes it unsafe to let the gpu main thread proceed without risk of deadlock
// AndroidOverlay isn't technically supposed to do that.
class MEDIA_EXPORT AndroidOverlay {
 public:
  // Called when the overlay is ready for use, via |GetJavaSurface()|.
  using ReadyCB = base::Callback<void()>;

  // Called when overlay has failed before |ReadyCB| is called.  Will not be
  // called after ReadyCB.  It will be the last callback for the overlay.
  using FailedCB = base::Callback<void()>;

  // Called when the overlay has been destroyed.  This will not be called unless
  // ReadyCB has been called.  It will be the last callback for the overlay.
  using DestroyedCB = base::Callback<void()>;

  // Configuration used to create an overlay.
  struct Config {
   public:
    Config();
    Config(const Config&);
    ~Config();

    // Implementation-specific token.
    base::UnguessableToken routing_token;

    gfx::Rect rect;

    ReadyCB ready_cb;
    FailedCB failed_cb;
    DestroyedCB destroyed_cb;
  };

  virtual ~AndroidOverlay() {}

  // Schedules a relayout of this overlay.  If called before the client is
  // notified that the surface is created, then the call will be ignored.
  virtual void ScheduleLayout(const gfx::Rect& rect) = 0;

  // May be called during / after ReadyCB and before DestroyedCB.
  virtual const base::android::JavaRef<jobject>& GetJavaSurface() const = 0;

 protected:
  AndroidOverlay() {}

  DISALLOW_COPY_AND_ASSIGN(AndroidOverlay);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_ANDROID_OVERLAY_H_
