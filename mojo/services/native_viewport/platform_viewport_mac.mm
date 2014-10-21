// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/platform_viewport.h"

#import <AppKit/NSApplication.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>

#include "base/bind.h"
#include "ui/gfx/rect.h"

namespace mojo {

class PlatformViewportMac : public PlatformViewport {
 public:
  PlatformViewportMac(Delegate* delegate)
      : delegate_(delegate),
        window_(nil) {
  }

  ~PlatformViewportMac() override {
    [window_ orderOut:nil];
    [window_ close];
  }

 private:
  // Overridden from PlatformViewport:
  void Init(const gfx::Rect& bounds) override {
    [NSApplication sharedApplication];

    rect_ = bounds;
    window_ = [[NSWindow alloc]
                  initWithContentRect:NSRectFromCGRect(rect_.ToCGRect())
                            styleMask:NSTitledWindowMask
                              backing:NSBackingStoreBuffered
                                defer:NO];
    delegate_->OnAcceleratedWidgetAvailable([window_ contentView]);
    delegate_->OnBoundsChanged(rect_);
  }

  void Show() override { [window_ orderFront:nil]; }

  void Hide() override { [window_ orderOut:nil]; }

  void Close() override {
    // TODO(beng): perform this in response to NSWindow destruction.
    delegate_->OnDestroyed();
  }

  gfx::Size GetSize() override { return rect_.size(); }

  void SetBounds(const gfx::Rect& bounds) override { NOTIMPLEMENTED(); }

  void SetCapture() override { NOTIMPLEMENTED(); }

  void ReleaseCapture() override { NOTIMPLEMENTED(); }

  Delegate* delegate_;
  NSWindow* window_;
  gfx::Rect rect_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportMac);
};

// static
scoped_ptr<PlatformViewport> PlatformViewport::Create(Delegate* delegate) {
  return scoped_ptr<PlatformViewport>(new PlatformViewportMac(delegate)).Pass();
}

}  // namespace mojo
