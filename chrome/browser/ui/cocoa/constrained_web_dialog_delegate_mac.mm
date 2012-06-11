// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/constrained_window_mac.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/web_dialog_web_contents_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/size.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

using content::WebContents;
using ui::WebDialogDelegate;
using ui::ConstrainedWebDialogDelegate;

class ConstrainedWebDialogDelegateMac :
    public ConstrainedWindowMacDelegateCustomSheet,
    public ConstrainedWebDialogDelegate {

 public:
  ConstrainedWebDialogDelegateMac(
      Profile* profile,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate);
  virtual ~ConstrainedWebDialogDelegateMac() {}

  void set_window(ConstrainedWindow* window) {
    return impl_->set_window(window);
  }

  // ConstrainedWebDialogDelegate interface
  virtual const WebDialogDelegate*
      GetWebDialogDelegate() const OVERRIDE {
    return impl_->GetWebDialogDelegate();
  }
  virtual WebDialogDelegate* GetWebDialogDelegate() OVERRIDE {
    return impl_->GetWebDialogDelegate();
  }
  virtual void OnDialogCloseFromWebUI() OVERRIDE {
    return impl_->OnDialogCloseFromWebUI();
  }
  virtual void ReleaseTabContentsOnDialogClose() OVERRIDE {
    return impl_->ReleaseTabContentsOnDialogClose();
  }
  virtual ConstrainedWindow* window() OVERRIDE {
    return impl_->window();
  }
  virtual TabContents* tab() OVERRIDE {
    return impl_->tab();
  }

  // ConstrainedWindowMacDelegateCustomSheet interface
  virtual void DeleteDelegate() OVERRIDE {
    // From ConstrainedWindowMacDelegate: "you MUST close the sheet belonging to
    // your delegate in this method."
    if (is_sheet_open())
      [NSApp endSheet:sheet()];
    if (!impl_->closed_via_webui())
      GetWebDialogDelegate()->OnDialogClosed("");
    delete this;
  }

 private:
  scoped_ptr<ConstrainedWebDialogDelegateBase> impl_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateMac);
};

// The delegate used to forward events from the sheet to the constrained
// window delegate. This bridge needs to be passed into the customsheet
// to allow the WebDialog to know when the sheet closes.
@interface ConstrainedWebDialogSheetCocoa : NSObject {
  ConstrainedWebDialogDelegateMac* constrainedWebDelegate_;  // weak
}
- (id)initWithConstrainedWebDialogDelegateMac:
    (ConstrainedWebDialogDelegateMac*)ConstrainedWebDialogDelegateMac;
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
@end

ConstrainedWebDialogDelegateMac::ConstrainedWebDialogDelegateMac(
    Profile* profile,
    WebDialogDelegate* delegate,
    WebDialogWebContentsDelegate* tab_delegate)
    : impl_(new ConstrainedWebDialogDelegateBase(profile,
                                                 delegate,
                                                 tab_delegate)) {
  // Create NSWindow to hold web_contents in the constrained sheet:
  gfx::Size size;
  delegate->GetDialogSize(&size);
  NSRect frame = NSMakeRect(0, 0, size.width(), size.height());

  // |window| is retained by the ConstrainedWindowMacDelegateCustomSheet when
  // the sheet is initialized.
  scoped_nsobject<NSWindow> window;
  window.reset(
      [[NSWindow alloc] initWithContentRect:frame
                                  styleMask:NSTitledWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:YES]);

  [window.get() setContentView:tab()->web_contents()->GetNativeView()];

  // Set the custom sheet to point to the new window.
  ConstrainedWindowMacDelegateCustomSheet::init(
      window.get(),
      [[[ConstrainedWebDialogSheetCocoa alloc]
          initWithConstrainedWebDialogDelegateMac:this] autorelease],
      @selector(sheetDidEnd:returnCode:contextInfo:));
}

ConstrainedWebDialogDelegate* ui::CreateConstrainedWebDialog(
        Profile* profile,
        WebDialogDelegate* delegate,
        WebDialogWebContentsDelegate* tab_delegate,
        TabContents* tab_contents) {
  // Deleted when ConstrainedWebDialogDelegateMac::DeleteDelegate() runs.
  ConstrainedWebDialogDelegateMac* constrained_delegate =
      new ConstrainedWebDialogDelegateMac(profile, delegate, tab_delegate);
  // Deleted when ConstrainedWebDialogDelegateMac::OnDialogCloseFromWebUI()
  // runs.
  ConstrainedWindow* constrained_window =
      new ConstrainedWindowMac(tab_contents, constrained_delegate);
  constrained_delegate->set_window(constrained_window);
  return constrained_delegate;
}

@implementation ConstrainedWebDialogSheetCocoa

- (id)initWithConstrainedWebDialogDelegateMac:
    (ConstrainedWebDialogDelegateMac*)ConstrainedWebDialogDelegateMac {
  if ((self = [super init]))
    constrainedWebDelegate_ = ConstrainedWebDialogDelegateMac;
  return self;
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void *)contextInfo {
  [sheet orderOut:self];
}

@end
