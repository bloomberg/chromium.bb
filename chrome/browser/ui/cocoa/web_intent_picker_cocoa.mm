// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/web_intent_picker_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/constrained_window_mac.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/web_intent_sheet_controller.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/gfx/image/image.h"

using content::WebContents;

namespace {

// Since any delegates for constrained windows are tasked with deleting
// themselves, and the WebIntentPicker needs to live longer than the
// constrained window, we need this forwarding class.
class ConstrainedPickerSheetDelegate :
    public ConstrainedWindowMacDelegateCustomSheet {
 public:
  ConstrainedPickerSheetDelegate(
      WebIntentPickerCocoa* picker,
      WebIntentPickerSheetController* sheet_controller);
  virtual ~ConstrainedPickerSheetDelegate() {}

  // ConstrainedWindowMacDelegateCustomSheet interface
  virtual void DeleteDelegate() OVERRIDE;

 private:
  WebIntentPickerCocoa* picker_; // Weak reference to picker

  DISALLOW_COPY_AND_ASSIGN(ConstrainedPickerSheetDelegate);
};

ConstrainedPickerSheetDelegate::ConstrainedPickerSheetDelegate(
    WebIntentPickerCocoa* picker,
    WebIntentPickerSheetController* sheet_controller)
    : picker_(picker) {
  init([sheet_controller window], sheet_controller,
      @selector(sheetDidEnd:returnCode:contextInfo:));
}

void ConstrainedPickerSheetDelegate::DeleteDelegate() {
  if (is_sheet_open())
    [NSApp endSheet:sheet()];

  delete this;
}

}  // namespace

// static
WebIntentPicker* WebIntentPicker::Create(TabContentsWrapper* wrapper,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model) {
  return new WebIntentPickerCocoa(wrapper, delegate, model);
}

WebIntentPickerCocoa::WebIntentPickerCocoa()
    : delegate_(NULL),
      model_(NULL),
      wrapper_(NULL),
      sheet_controller_(nil),
      service_invoked(false) {
}

WebIntentPickerCocoa::WebIntentPickerCocoa(TabContentsWrapper* wrapper,
                                           WebIntentPickerDelegate* delegate,
                                           WebIntentPickerModel* model)
    : delegate_(delegate),
      model_(model),
      wrapper_(wrapper),
      sheet_controller_(nil),
      service_invoked(false) {
  model_->set_observer(this);

  DCHECK(delegate);
  DCHECK(wrapper);

  sheet_controller_ = [
      [WebIntentPickerSheetController alloc] initWithPicker:this];

  // Deleted when ConstrainedPickerSheetDelegate::DeleteDelegate() runs.
  ConstrainedPickerSheetDelegate* constrained_delegate =
      new ConstrainedPickerSheetDelegate(this, sheet_controller_);

  window_ = new ConstrainedWindowMac(wrapper, constrained_delegate);
}

WebIntentPickerCocoa::~WebIntentPickerCocoa() {
  if (model_ != NULL)
    model_->set_observer(NULL);
}

void WebIntentPickerCocoa::OnSheetDidEnd(NSWindow* sheet) {
  [sheet orderOut:sheet_controller_];
  if (window_)
    window_->CloseConstrainedWindow();
  delegate_->OnClosing();
}

void WebIntentPickerCocoa::Close() {
  DCHECK(sheet_controller_);
  [sheet_controller_ closeSheet];

  if (inline_disposition_tab_contents_.get())
    inline_disposition_tab_contents_->web_contents()->OnCloseStarted();
}

void WebIntentPickerCocoa::SetActionString(const string16& action_string) {
  [sheet_controller_ setActionString:base::SysUTF16ToNSString(action_string)];
}

void WebIntentPickerCocoa::PerformLayout() {
  DCHECK(sheet_controller_);
  // If the window is animating closed when this is called, the
  // animation could be holding the last reference to |controller_|
  // (and thus |this|).  Pin it until the task is completed.
  scoped_nsobject<WebIntentPickerSheetController>
      keep_alive([sheet_controller_ retain]);
  [sheet_controller_ performLayoutWithModel:model_];
}

void WebIntentPickerCocoa::OnModelChanged(WebIntentPickerModel* model) {
  PerformLayout();
}

void WebIntentPickerCocoa::OnFaviconChanged(WebIntentPickerModel* model,
                                            size_t index) {
  // We don't handle individual icon changes - just redo the whole model.
  PerformLayout();
}

void WebIntentPickerCocoa::OnExtensionIconChanged(
    WebIntentPickerModel* model,
    const string16& extension_id) {
  // We don't handle individual icon changes - just redo the whole model.
  PerformLayout();
}

void WebIntentPickerCocoa::OnInlineDisposition(WebIntentPickerModel* model,
                                               const GURL& url) {
  content::WebContents* web_contents = content::WebContents::Create(
      wrapper_->profile(),
      tab_util::GetSiteInstanceForNewTab(wrapper_->profile(), url),
      MSG_ROUTING_NONE, NULL, NULL);
  inline_disposition_tab_contents_.reset(new TabContentsWrapper(web_contents));
  inline_disposition_delegate_.reset(
      new WebIntentInlineDispositionDelegate(this, web_contents,
                                             wrapper_->profile()));

  // Must call this immediately after WebContents creation to avoid race
  // with load.
  delegate_->OnInlineDispositionWebContentsCreated(web_contents);

  inline_disposition_tab_contents_->web_contents()->GetController().LoadURL(
      url,
      content::Referrer(),
      content::PAGE_TRANSITION_START_PAGE,
      std::string());

  [sheet_controller_ setInlineDispositionTabContents:
      inline_disposition_tab_contents_.get()];
  PerformLayout();
}

void WebIntentPickerCocoa::OnCancelled() {
  DCHECK(delegate_);
  if (!service_invoked)
    delegate_->OnPickerClosed();
  delegate_->OnClosing();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WebIntentPickerCocoa::OnServiceChosen(size_t index) {
  DCHECK(delegate_);
  const WebIntentPickerModel::InstalledService& installed_service =
      model_->GetInstalledServiceAt(index);
  service_invoked = true;
  delegate_->OnServiceChosen(installed_service.url,
                             installed_service.disposition);
}

void WebIntentPickerCocoa::OnExtensionInstallRequested(
    const std::string& extension_id) {
  delegate_->OnExtensionInstallRequested(extension_id);
}

void WebIntentPickerCocoa::OnExtensionInstallSuccess(const std::string& id) {
  DCHECK(sheet_controller_);
  [sheet_controller_ stopThrobber];
}

void WebIntentPickerCocoa::OnExtensionInstallFailure(const std::string& id) {
  // TODO(groby): What to do on failure? (See also binji for views/gtk)
  DCHECK(sheet_controller_);
  [sheet_controller_ stopThrobber];
}

void WebIntentPickerCocoa::OnExtensionLinkClicked(const std::string& id) {
  DCHECK(delegate_);
  delegate_->OnExtensionLinkClicked(id);
}

void WebIntentPickerCocoa::OnSuggestionsLinkClicked() {
  DCHECK(delegate_);
  delegate_->OnSuggestionsLinkClicked();
}
