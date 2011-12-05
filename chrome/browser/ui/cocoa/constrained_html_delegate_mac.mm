// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_html_ui.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/constrained_window_mac.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/gfx/size.h"

class ConstrainedHtmlDelegateMac :
    public ConstrainedWindowMacDelegateCustomSheet,
    public HtmlDialogTabContentsDelegate,
    public ConstrainedHtmlUIDelegate {

 public:
  ConstrainedHtmlDelegateMac(Profile* profile,
                             HtmlDialogUIDelegate* delegate);
  ~ConstrainedHtmlDelegateMac() {
    if (release_tab_on_close_)
      ignore_result(tab_.release());
  }

  // ConstrainedWindowMacDelegateCustomSheet -----------------------------------
  virtual void DeleteDelegate() OVERRIDE {
    // From ConstrainedWindowMacDelegate: "you MUST close the sheet belonging to
    // your delegate in this method."
    if (is_sheet_open())
      [NSApp endSheet:sheet()];
    if (!closed_via_webui_)
      html_delegate_->OnDialogClosed("");
    delete this;
  }

  // ConstrainedHtmlDelegate ---------------------------------------------------
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() OVERRIDE;
  virtual void OnDialogCloseFromWebUI() OVERRIDE;
  virtual void ReleaseTabContentsOnDialogClose() OVERRIDE;
  virtual ConstrainedWindow* window() OVERRIDE { return constrained_window_; }
  virtual TabContentsWrapper* tab() OVERRIDE { return tab_.get(); }


  // HtmlDialogTabContentsDelegate ---------------------------------------------
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE {}

  void set_window(ConstrainedWindow* window) {
    constrained_window_ = window;
  }

 private:
  // Holds the HTML to be displayed in the sheet.
  scoped_ptr<TabContentsWrapper> tab_;
  HtmlDialogUIDelegate* html_delegate_;  // weak.

  // The constrained window that owns |this|. Saved here because it needs to be
  // closed in response to the WebUI OnDialogClose callback.
  ConstrainedWindow* constrained_window_;

  // Was the dialog closed from WebUI (in which case |html_delegate_|'s
  // OnDialogClosed() method has already been called)?
  bool closed_via_webui_;

  // If true, release |tab_| on close instead of destroying it.
  bool release_tab_on_close_;

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
  HtmlDialogUIDelegate* delegate)
  : HtmlDialogTabContentsDelegate(profile),
    html_delegate_(delegate),
    constrained_window_(NULL),
    closed_via_webui_(false),
    release_tab_on_close_(false) {
  TabContents* tab_contents =
    new TabContents(profile, NULL, MSG_ROUTING_NONE, NULL, NULL);
  tab_.reset(new TabContentsWrapper(tab_contents));
  tab_contents->set_delegate(this);

  // Set |this| as a property on the tab contents so that the ConstrainedHtmlUI
  // can get a reference to |this|.
  ConstrainedHtmlUI::GetPropertyAccessor().SetProperty(
      tab_contents->property_bag(), this);

  tab_contents->controller().LoadURL(delegate->GetDialogContentURL(),
                                     content::Referrer(),
                                     content::PAGE_TRANSITION_START_PAGE,
                                     std::string());

  // Create NSWindow to hold tab_contents in the constrained sheet:
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

  [window.get() setContentView:tab_contents->GetNativeView()];

  // Set the custom sheet to point to the new window.
  ConstrainedWindowMacDelegateCustomSheet::init(
      window.get(),
      [[[ConstrainedHtmlDialogSheetCocoa alloc]
          initWithConstrainedHtmlDelegateMac:this] autorelease],
      @selector(sheetDidEnd:returnCode:contextInfo:));
}

HtmlDialogUIDelegate* ConstrainedHtmlDelegateMac::GetHtmlDialogUIDelegate() {
  return html_delegate_;
}

void ConstrainedHtmlDelegateMac::OnDialogCloseFromWebUI() {
  closed_via_webui_ = true;
  DCHECK(constrained_window_);
  if (constrained_window_)
    constrained_window_->CloseConstrainedWindow();
}

void ConstrainedHtmlDelegateMac::ReleaseTabContentsOnDialogClose() {
  release_tab_on_close_ = true;
}

// static
ConstrainedHtmlUIDelegate* ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    TabContentsWrapper* wrapper) {
  // Deleted when ConstrainedHtmlDelegateMac::DeleteDelegate() runs.
  ConstrainedHtmlDelegateMac* constrained_delegate =
      new ConstrainedHtmlDelegateMac(profile, delegate);
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
