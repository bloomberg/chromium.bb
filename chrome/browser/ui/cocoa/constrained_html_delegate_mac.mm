// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_html_ui_delegate_impl.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/constrained_window_mac.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/size.h"

using content::WebContents;

class ConstrainedHtmlDelegateMac :
    public ConstrainedWindowMacDelegateCustomSheet,
    public ConstrainedHtmlUIDelegate {

 public:
  ConstrainedHtmlDelegateMac(Profile* profile,
                             HtmlDialogUIDelegate* delegate,
                             HtmlDialogTabContentsDelegate* tab_delegate);
  virtual ~ConstrainedHtmlDelegateMac() {}

  void set_window(ConstrainedWindow* window) {
    return impl_->set_window(window);
  }

  // ConstrainedHtmlUIDelegate interface
  virtual const HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() const OVERRIDE {
    return impl_->GetHtmlDialogUIDelegate();
  }
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() OVERRIDE {
    return impl_->GetHtmlDialogUIDelegate();
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
  virtual TabContentsWrapper* tab() OVERRIDE {
    return impl_->tab();
  }

  // ConstrainedWindowMacDelegateCustomSheet interface
  virtual void DeleteDelegate() OVERRIDE {
    // From ConstrainedWindowMacDelegate: "you MUST close the sheet belonging to
    // your delegate in this method."
    if (is_sheet_open())
      [NSApp endSheet:sheet()];
    if (!impl_->closed_via_webui())
      GetHtmlDialogUIDelegate()->OnDialogClosed("");
    delete this;
  }

 private:
  scoped_ptr<ConstrainedHtmlUIDelegateImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedHtmlDelegateMac);
};

// The delegate used to forward events from the sheet to the constrained
// window delegate. This bridge needs to be passed into the customsheet
// to allow the HtmlDialog to know when the sheet closes.
@interface ConstrainedHtmlDialogSheetCocoa : NSObject {
  ConstrainedHtmlDelegateMac* constrainedHtmlDelegate_;  // weak
}
- (id)initWithConstrainedHtmlDelegateMac:
    (ConstrainedHtmlDelegateMac*)ConstrainedHtmlDelegateMac;
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
@end

ConstrainedHtmlDelegateMac::ConstrainedHtmlDelegateMac(
  Profile* profile,
  HtmlDialogUIDelegate* delegate,
  HtmlDialogTabContentsDelegate* tab_delegate)
  : impl_(new ConstrainedHtmlUIDelegateImpl(profile, delegate, tab_delegate)) {
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
      [[[ConstrainedHtmlDialogSheetCocoa alloc]
          initWithConstrainedHtmlDelegateMac:this] autorelease],
      @selector(sheetDidEnd:returnCode:contextInfo:));
}

// static
ConstrainedHtmlUIDelegate* ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    HtmlDialogTabContentsDelegate* tab_delegate,
    TabContentsWrapper* wrapper) {
  // Deleted when ConstrainedHtmlDelegateMac::DeleteDelegate() runs.
  ConstrainedHtmlDelegateMac* constrained_delegate =
      new ConstrainedHtmlDelegateMac(profile, delegate, tab_delegate);
  // Deleted when ConstrainedHtmlDelegateMac::OnDialogCloseFromWebUI() runs.
  ConstrainedWindow* constrained_window =
      new ConstrainedWindowMac(wrapper, constrained_delegate);
  constrained_delegate->set_window(constrained_window);
  return constrained_delegate;
}

@implementation ConstrainedHtmlDialogSheetCocoa

- (id)initWithConstrainedHtmlDelegateMac:
    (ConstrainedHtmlDelegateMac*)ConstrainedHtmlDelegateMac {
  if ((self = [super init]))
    constrainedHtmlDelegate_ = ConstrainedHtmlDelegateMac;
  return self;
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void *)contextInfo {
  [sheet orderOut:self];
}

@end
