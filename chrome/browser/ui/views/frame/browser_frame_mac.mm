// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_mac.h"

#import "chrome/browser/ui/cocoa/browser_window_command_handler.h"
#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_shutdown.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#import "chrome/browser/ui/views/frame/native_widget_mac_frameless_nswindow.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#import "ui/base/cocoa/window_size_constants.h"

BrowserFrameMac::BrowserFrameMac(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetMac(browser_frame),
      browser_view_(browser_view),
      command_dispatcher_delegate_(
          [[ChromeCommandDispatcherDelegate alloc] init]) {}

BrowserFrameMac::~BrowserFrameMac() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameMac, views::NativeWidgetMac implementation:

void BrowserFrameMac::OnWindowWillClose() {
  // Destroy any remaining WebContents early on. This is consistent with Aura.
  // See comment in DesktopBrowserFrameAura::OnHostClosed().
  DestroyBrowserWebContents(browser_view_->browser());
  NativeWidgetMac::OnWindowWillClose();
}

int BrowserFrameMac::SheetPositionY() {
  web_modal::WebContentsModalDialogHost* dialog_host =
      browser_view_->GetWebContentsModalDialogHost();
  NSView* view = dialog_host->GetHostView();
  // Get the position of the host view relative to the window since
  // ModalDialogHost::GetDialogPosition() is relative to the host view.
  int host_view_y =
      [view convertPoint:NSMakePoint(0, NSHeight([view frame])) toView:nil].y;
  return host_view_y - dialog_host->GetDialogPosition(gfx::Size()).y();
}

void BrowserFrameMac::InitNativeWidget(
    const views::Widget::InitParams& params) {
  views::NativeWidgetMac::InitNativeWidget(params);

  // Our content view draws on top of the titlebar area, but we want the window
  // control buttons to draw on top of the content view.
  // We do this by setting the content view's z-order below the buttons, and
  // by giving the root view a layer so that the buttons get their own layers.
  NSView* content_view = [GetNativeWindow() contentView];
  NSView* root_view = [content_view superview];
  [content_view removeFromSuperview];
  [root_view setWantsLayer:YES];
  [root_view addSubview:content_view positioned:NSWindowBelow relativeTo:nil];
}

NativeWidgetMacNSWindow* BrowserFrameMac::CreateNSWindow(
    const views::Widget::InitParams& params) {
  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                          NSMiniaturizableWindowMask | NSResizableWindowMask |
                          NSTexturedBackgroundWindowMask;
  base::scoped_nsobject<NativeWidgetMacFramelessNSWindow> ns_window(
      [[NativeWidgetMacFramelessNSWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:style_mask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
  [ns_window setCommandDispatcherDelegate:command_dispatcher_delegate_];
  [ns_window setCommandHandler:[[[BrowserWindowCommandHandler alloc] init]
                                   autorelease]];
  return ns_window.autorelease();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameMac, NativeBrowserFrame implementation:

views::Widget::InitParams BrowserFrameMac::GetWidgetParams() {
  views::Widget::InitParams params;
  params.native_widget = this;
  return params;
}

bool BrowserFrameMac::UseCustomFrame() const {
  return false;
}

bool BrowserFrameMac::UsesNativeSystemMenu() const {
  return true;
}

bool BrowserFrameMac::ShouldSaveWindowPlacement() const {
  return true;
}

void BrowserFrameMac::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  return NativeWidgetMac::GetWindowPlacement(bounds, show_state);
}

int BrowserFrameMac::GetMinimizeButtonOffset() const {
  NOTIMPLEMENTED();
  return 0;
}
