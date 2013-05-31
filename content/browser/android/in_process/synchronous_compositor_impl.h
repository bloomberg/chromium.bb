// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/android/in_process/synchronous_compositor_output_surface.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

// The purpose of this class is to act as the intermediary between the various
// components that make up the 'synchronous compositor mode' implementation and
// expose their functionality via the SynchronousCompositor interface.
// This class is created on the main thread but most of the APIs are called
// from the Compositor thread.
class SynchronousCompositorImpl
    : public SynchronousCompositor,
      public SynchronousCompositorOutputSurfaceDelegate,
      public WebContentsUserData<SynchronousCompositorImpl> {
 public:
  static SynchronousCompositorImpl* FromRoutingID(int routing_id);

  // SynchronousCompositor
  virtual bool IsHwReady() OVERRIDE;
  virtual void SetClient(SynchronousCompositorClient* compositor_client)
      OVERRIDE;
  virtual bool DemandDrawSw(SkCanvas* canvas) OVERRIDE;
  virtual bool DemandDrawHw(
      gfx::Size view_size,
      const gfx::Transform& transform,
      gfx::Rect clip) OVERRIDE;

  // SynchronousCompositorOutputSurfaceDelegate
  virtual void DidBindOutputSurface(
      SynchronousCompositorOutputSurface* output_surface) OVERRIDE;
  virtual void DidDestroySynchronousOutputSurface(
      SynchronousCompositorOutputSurface* output_surface) OVERRIDE;
  virtual void SetContinuousInvalidate(bool enable) OVERRIDE;

 private:
  explicit SynchronousCompositorImpl(WebContents* contents);
  virtual ~SynchronousCompositorImpl();
  friend class WebContentsUserData<SynchronousCompositorImpl>;

  void DidCreateSynchronousOutputSurface(
      SynchronousCompositorOutputSurface* output_surface);
  bool CalledOnValidThread() const;

  int routing_id_;
  SynchronousCompositorClient* compositor_client_;
  SynchronousCompositorOutputSurface* output_surface_;
  WebContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_
