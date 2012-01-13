// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/web_intent_picker_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/web_intent_bubble_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "skia/ext/skia_utils_mac.h"

using content::WebContents;

class InlineHtmlContentDelegate: public content::WebContentsDelegate {
 public:
  InlineHtmlContentDelegate() {}
  virtual ~InlineHtmlContentDelegate() {}

  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE {
    return true;
  }
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      content::NavigationType navigation_type) OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InlineHtmlContentDelegate);
};

// static
WebIntentPicker* WebIntentPicker::Create(Browser* browser,
                                         TabContentsWrapper* wrapper,
                                         WebIntentPickerDelegate* delegate) {
  return new WebIntentPickerCocoa(browser, wrapper, delegate);
}

WebIntentPickerCocoa::WebIntentPickerCocoa()
    : delegate_(NULL), browser_(NULL), controller_(NULL) {
}


WebIntentPickerCocoa::WebIntentPickerCocoa(Browser* browser,
                                           TabContentsWrapper* wrapper,
                                           WebIntentPickerDelegate* delegate)
    : delegate_(delegate),
      browser_(browser),
      controller_(NULL) {
  DCHECK(browser);
  DCHECK(delegate);
  NSWindow* parentWindow = browser->window()->GetNativeHandle();

  // Compute the anchor point, relative to location bar.
  BrowserWindowController* controller = [parentWindow windowController];
  LocationBarViewMac* locationBar = [controller locationBarBridge];
  NSPoint anchor = locationBar->GetPageInfoBubblePoint();
  anchor = [browser->window()->GetNativeHandle() convertBaseToScreen:anchor];

  // The controller is deallocated when the window is closed, so no need to
  // worry about it here.
  [[WebIntentBubbleController alloc] initWithPicker:this
                                       parentWindow:parentWindow
                                         anchoredAt:anchor];
}

void WebIntentPickerCocoa::SetServiceURLs(const std::vector<GURL>& urls) {
  DCHECK(controller_);
  scoped_nsobject<NSMutableArray> urlArray(
      [[NSMutableArray alloc] initWithCapacity:urls.size()]);

  for (std::vector<GURL>::const_iterator iter(urls.begin());
       iter != urls.end(); ++iter) {
    [urlArray addObject:
        [NSString stringWithUTF8String:iter->spec().c_str()]];
  }

  [controller_ setServiceURLs:urlArray];
}

void WebIntentPickerCocoa::SetServiceIcon(size_t index, const SkBitmap& icon) {
  DCHECK(controller_);
  if (icon.empty())
    return;

  NSImage* image = gfx::SkBitmapToNSImage(icon);
  [controller_ replaceImageAtIndex:index withImage:image];
}

void WebIntentPickerCocoa::SetDefaultServiceIcon(size_t index) {
}

void WebIntentPickerCocoa::Close() {
}

WebContents* WebIntentPickerCocoa::SetInlineDisposition(const GURL& url) {
  TabContents* tab_contents = new TabContents(
      browser_->profile(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  inline_disposition_tab_contents_.reset(new TabContentsWrapper(tab_contents));
  inline_disposition_delegate_.reset(new InlineHtmlContentDelegate);
  tab_contents->SetDelegate(inline_disposition_delegate_.get());

  inline_disposition_tab_contents_->web_contents()->GetController().LoadURL(
      url,
      content::Referrer(),
      content::PAGE_TRANSITION_START_PAGE,
      std::string());

  [controller_ setInlineDispositionTabContents:
      inline_disposition_tab_contents_.get()];
  return inline_disposition_tab_contents_->web_contents();
}

WebIntentPickerCocoa::~WebIntentPickerCocoa() {
}

void WebIntentPickerCocoa::OnCancelled() {
  DCHECK(delegate_);
  delegate_->OnCancelled();
  controller_ = NULL;  // Controller will be unusable soon, abandon.
}

void WebIntentPickerCocoa::OnServiceChosen(size_t index) {
  DCHECK(delegate_);
  delegate_->OnServiceChosen(index);
}

void WebIntentPickerCocoa::set_controller(
    WebIntentBubbleController* controller) {
  controller_ = controller;
}
