// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/html_dialog_view.h"

#include <vector>

#include "base/property_bag.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/html_dialog_controller.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/events/event.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/event.h"
#include "ui/views/widget/native_widget_aura.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace browser {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
gfx::NativeWindow ShowHtmlDialog(gfx::NativeWindow parent,
                                 Profile* profile,
                                 Browser* browser,
                                 HtmlDialogUIDelegate* delegate,
                                 DialogStyle style) {
  views::Widget* widget = views::Widget::CreateWindowWithParent(
      new HtmlDialogView(profile, browser, delegate),
      parent);
  widget->Show();
  return widget->GetNativeWindow();
}

void CloseHtmlDialog(gfx::NativeWindow window) {
  views::Widget::GetWidgetForNativeWindow(window)->Close();
}

}  // namespace browser

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, public:

HtmlDialogView::HtmlDialogView(Profile* profile,
                               Browser* browser,
                               HtmlDialogUIDelegate* delegate)
    : HtmlDialogTabContentsDelegate(profile),
      initialized_(false),
      delegate_(delegate),
      dialog_controller_(new HtmlDialogController(this, profile, browser)),
      web_view_(new views::WebView(profile)) {
  web_view_->set_allow_accelerators(true);
  AddChildView(web_view_);
  SetLayoutManager(new views::FillLayout);
  // Pressing the ESC key will close the dialog.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, false, false, false));
}

HtmlDialogView::~HtmlDialogView() {
}

content::WebContents* HtmlDialogView::web_contents() {
  return web_view_->web_contents();
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, views::View implementation:

gfx::Size HtmlDialogView::GetPreferredSize() {
  gfx::Size out;
  if (delegate_)
    delegate_->GetMinimumDialogSize(&out);
  return out;
}

bool HtmlDialogView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Pressing ESC closes the dialog.
  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
  OnDialogClosed(std::string());
  return true;
}

void HtmlDialogView::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && GetWidget())
    InitDialog();
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, views::WidgetDelegate implementation:

bool HtmlDialogView::CanResize() const {
  return true;
}

ui::ModalType HtmlDialogView::GetModalType() const {
  return GetDialogModalType();
}

string16 HtmlDialogView::GetWindowTitle() const {
  if (delegate_)
    return delegate_->GetDialogTitle();
  return string16();
}

std::string HtmlDialogView::GetWindowName() const {
  if (delegate_)
    return delegate_->GetDialogName();
  return std::string();
}

void HtmlDialogView::WindowClosing() {
  // If we still have a delegate that means we haven't notified it of the
  // dialog closing. This happens if the user clicks the Close button on the
  // dialog.
  if (delegate_)
    OnDialogClosed("");
}

views::View* HtmlDialogView::GetContentsView() {
  return this;
}

views::View* HtmlDialogView::GetInitiallyFocusedView() {
  return web_view_;
}

bool HtmlDialogView::ShouldShowWindowTitle() const {
  return ShouldShowDialogTitle();
}

views::Widget* HtmlDialogView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* HtmlDialogView::GetWidget() const {
  return View::GetWidget();
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogUIDelegate implementation:

ui::ModalType HtmlDialogView::GetDialogModalType() const {
  if (delegate_)
    return delegate_->GetDialogModalType();
  return ui::MODAL_TYPE_NONE;
}

string16 HtmlDialogView::GetDialogTitle() const {
  return GetWindowTitle();
}

GURL HtmlDialogView::GetDialogContentURL() const {
  if (delegate_)
    return delegate_->GetDialogContentURL();
  return GURL();
}

void HtmlDialogView::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  if (delegate_)
    delegate_->GetWebUIMessageHandlers(handlers);
}

void HtmlDialogView::GetDialogSize(gfx::Size* size) const {
  if (delegate_)
    delegate_->GetDialogSize(size);
}

void HtmlDialogView::GetMinimumDialogSize(gfx::Size* size) const {
  if (delegate_)
    delegate_->GetMinimumDialogSize(size);
}

std::string HtmlDialogView::GetDialogArgs() const {
  if (delegate_)
    return delegate_->GetDialogArgs();
  return std::string();
}

void HtmlDialogView::OnDialogClosed(const std::string& json_retval) {
  HtmlDialogTabContentsDelegate::Detach();
  if (delegate_) {
    // Store the dialog content area size.
    delegate_->StoreDialogSize(GetContentsBounds().size());
  }

  if (GetWidget())
    GetWidget()->Close();

  if (delegate_) {
    delegate_->OnDialogClosed(json_retval);
    delegate_ = NULL;  // We will not communicate further with the delegate.
  }
}

void HtmlDialogView::OnCloseContents(WebContents* source,
                                     bool* out_close_dialog) {
  if (delegate_)
    delegate_->OnCloseContents(source, out_close_dialog);
}

bool HtmlDialogView::ShouldShowDialogTitle() const {
  if (delegate_)
    return delegate_->ShouldShowDialogTitle();
  return true;
}

bool HtmlDialogView::HandleContextMenu(
    const content::ContextMenuParams& params) {
  if (delegate_)
    return delegate_->HandleContextMenu(params);
  return HtmlDialogTabContentsDelegate::HandleContextMenu(params);
}

////////////////////////////////////////////////////////////////////////////////
// content::WebContentsDelegate implementation:

void HtmlDialogView::MoveContents(WebContents* source, const gfx::Rect& pos) {
  // The contained web page wishes to resize itself. We let it do this because
  // if it's a dialog we know about, we trust it not to be mean to the user.
  GetWidget()->SetBounds(pos);
}

// A simplified version of BrowserView::HandleKeyboardEvent().
// We don't handle global keyboard shortcuts here, but that's fine since
// they're all browser-specific. (This may change in the future.)
void HtmlDialogView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
#if defined(USE_AURA)
  aura::KeyEvent aura_event(event.os_event->native_event(), false);
  views::NativeWidgetAura* aura_widget =
      static_cast<views::NativeWidgetAura*>(GetWidget()->native_widget());
  aura_widget->OnKeyEvent(&aura_event);
#elif defined(OS_WIN)
  // Any unhandled keyboard/character messages should be defproced.
  // This allows stuff like F10, etc to work correctly.
  DefWindowProc(event.os_event.hwnd, event.os_event.message,
                  event.os_event.wParam, event.os_event.lParam);
#endif
}

void HtmlDialogView::CloseContents(WebContents* source) {
  bool close_dialog = false;
  OnCloseContents(source, &close_dialog);
  if (close_dialog)
    OnDialogClosed(std::string());
}

content::WebContents* HtmlDialogView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  content::WebContents* new_contents = NULL;
  if (delegate_ &&
      delegate_->HandleOpenURLFromTab(source, params, &new_contents)) {
    return new_contents;
  }
  return HtmlDialogTabContentsDelegate::OpenURLFromTab(source, params);
}

void HtmlDialogView::AddNewContents(content::WebContents* source,
                                    content::WebContents* new_contents,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_pos,
                                    bool user_gesture) {
  if (delegate_ && delegate_->HandleAddNewContents(
          source, new_contents, disposition, initial_pos, user_gesture)) {
    return;
  }
  HtmlDialogTabContentsDelegate::AddNewContents(
      source, new_contents, disposition, initial_pos, user_gesture);
}

void HtmlDialogView::LoadingStateChanged(content::WebContents* source) {
  if (delegate_)
    delegate_->OnLoadingStateChanged(source);
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, TabRenderWatcher::Delegate implementation:

void HtmlDialogView::OnRenderHostCreated(content::RenderViewHost* host) {
}

void HtmlDialogView::OnTabMainFrameLoaded() {
}

void HtmlDialogView::OnTabMainFrameRender() {
  tab_watcher_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, private:

void HtmlDialogView::InitDialog() {
  content::WebContents* web_contents = web_view_->GetWebContents();
  if (web_contents->GetDelegate() == this)
    return;

  web_contents->SetDelegate(this);

  // Set the delegate. This must be done before loading the page. See
  // the comment above HtmlDialogUI in its header file for why.
  HtmlDialogUI::GetPropertyAccessor().SetProperty(
      web_contents->GetPropertyBag(), this);
  tab_watcher_.reset(new TabRenderWatcher(web_contents, this));

  if (delegate_) {
    gfx::Size out;
    delegate_->GetDialogSize(&out);
    if (!out.IsEmpty() && GetWidget())
      GetWidget()->CenterWindow(out);
  }

  web_view_->LoadInitialURL(GetDialogContentURL());
}
