// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_SURFACE_CALLBACK_H_
#define CONTENT_COMMON_ANDROID_SURFACE_CALLBACK_H_
#pragma once

#include <jni.h>

#include "base/callback.h"
#include "content/common/android/surface_texture_peer.h"

struct ANativeWindow;

namespace base {
class MessageLoopProxy;
class WaitableEvent;
}

namespace content {

// This file implements support for passing surface handles from Java
// to the correct thread on the native side. On Android, these surface
// handles can only be passed across processes through Java IPC (Binder),
// which means calls from Java come in on arbitrary threads. Hence the
// static nature and the need for the client to register a callback with
// the corresponding message loop.

// Asynchronously sets the Surface. This can be called from any thread.
// The Surface will be set to the proper thread based on the type. The
// nature of primary_id and secondary_id depend on the type of surface
// and are used to route the surface to the correct client.
// This method will call release() on the jsurface object to release
// all the resources. So after calling this method, the caller should
// not use the jsurface object again.
void SetSurfaceAsync(JNIEnv* env,
                     jobject jsurface,
                     SurfaceTexturePeer::SurfaceTextureTarget type,
                     int primary_id,
                     int secondary_id,
                     base::WaitableEvent* completion);

void ReleaseSurface(jobject surface);

typedef base::Callback<void(int, int, ANativeWindow*, base::WaitableEvent*)>
                            NativeWindowCallback;
void RegisterNativeWindowCallback(base::MessageLoopProxy* loop,
                                  NativeWindowCallback& callback);

typedef base::Callback<void(int, int, jobject surface)> VideoSurfaceCallback;
void RegisterVideoSurfaceCallback(base::MessageLoopProxy* loop,
                                  VideoSurfaceCallback& callback);

bool RegisterSurfaceCallback(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_SURFACE_CALLBACK_H_
