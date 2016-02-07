// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_web_dialog_sheet.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

using content::WebContents;
using ui::WebDialogDelegate;
using ui::WebDialogWebContentsDelegate;

namespace {

class ConstrainedWebDialogDelegateMac
    : public ConstrainedWebDialogDelegateBase {
 public:
  ConstrainedWebDialogDelegateMac(
      content::BrowserContext* browser_context,
      WebDialogDelegate* delegate)
      : ConstrainedWebDialogDelegateBase(browser_context, delegate, NULL) {}

  // WebDialogWebContentsDelegate interface.
  void CloseContents(WebContents* source) override {
    window_->CloseWebContentsModalDialog();
  }

  void set_window(ConstrainedWindowMac* window) { window_ = window; }
  ConstrainedWindowMac* window() const { return window_; }

 private:
  // Weak, owned by ConstrainedWebDialogDelegateViewMac.
  ConstrainedWindowMac* window_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateMac);
};

}  // namespace

class ConstrainedWebDialogDelegateViewMac :
    public ConstrainedWindowMacDelegate,
    public ConstrainedWebDialogDelegate {

 public:
  ConstrainedWebDialogDelegateViewMac(
      content::BrowserContext* browser_context,
      WebDialogDelegate* delegate,
      content::WebContents* web_contents);
  ~ConstrainedWebDialogDelegateViewMac() override {}

  // ConstrainedWebDialogDelegate interface
  const WebDialogDelegate* GetWebDialogDelegate() const override {
    return impl_->GetWebDialogDelegate();
  }
  WebDialogDelegate* GetWebDialogDelegate() override {
    return impl_->GetWebDialogDelegate();
  }
  void OnDialogCloseFromWebUI() override {
    return impl_->OnDialogCloseFromWebUI();
  }
  void ReleaseWebContentsOnDialogClose() override {
    return impl_->ReleaseWebContentsOnDialogClose();
  }
  gfx::NativeWindow GetNativeDialog() override { return window_; }
  WebContents* GetWebContents() override { return impl_->GetWebContents(); }
  gfx::Size GetMinimumSize() const override {
    NOTIMPLEMENTED();
    return gfx::Size();
  }
  gfx::Size GetMaximumSize() const override {
    NOTIMPLEMENTED();
    return gfx::Size();
  }
  gfx::Size GetPreferredSize() const override {
    NOTIMPLEMENTED();
    return gfx::Size();
  }

  // ConstrainedWindowMacDelegate interface
  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override {
    if (!impl_->closed_via_webui())
      GetWebDialogDelegate()->OnDialogClosed("");
    delete this;
  }

 private:
  scoped_ptr<ConstrainedWebDialogDelegateMac> impl_;
  scoped_ptr<ConstrainedWindowMac> constrained_window_;
  base::scoped_nsobject<NSWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateViewMac);
};

ConstrainedWebDialogDelegateViewMac::ConstrainedWebDialogDelegateViewMac(
    content::BrowserContext* browser_context,
    WebDialogDelegate* delegate,
    content::WebContents* web_contents)
    : impl_(new ConstrainedWebDialogDelegateMac(browser_context, delegate)) {
  // Create a window to hold web_contents in the constrained sheet:
  gfx::Size size;
  delegate->GetDialogSize(&size);
  NSRect frame = NSMakeRect(0, 0, size.width(), size.height());

  window_.reset(
      [[ConstrainedWindowCustomWindow alloc] initWithContentRect:frame]);
  [GetWebContents()->GetNativeView() setFrame:frame];
  [GetWebContents()->GetNativeView() setAutoresizingMask:
      NSViewWidthSizable|NSViewHeightSizable];
  [[window_ contentView] addSubview:GetWebContents()->GetNativeView()];

  base::scoped_nsobject<WebDialogConstrainedWindowSheet> sheet(
      [[WebDialogConstrainedWindowSheet alloc] initWithCustomWindow:window_
                                                  webDialogDelegate:delegate]);
  constrained_window_.reset(new ConstrainedWindowMac(
      this, web_contents, sheet));

  impl_->set_window(constrained_window_.get());
}

ConstrainedWebDialogDelegate* ShowConstrainedWebDialog(
        content::BrowserContext* browser_context,
        WebDialogDelegate* delegate,
        content::WebContents* web_contents) {
  // Deleted when the dialog closes.
  ConstrainedWebDialogDelegateViewMac* constrained_delegate =
      new ConstrainedWebDialogDelegateViewMac(
          browser_context, delegate, web_contents);
  return constrained_delegate;
}
