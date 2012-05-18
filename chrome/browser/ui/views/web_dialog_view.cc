// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_dialog_view.h"

#include <vector>

#include "base/property_bag.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/web_dialog_controller.h"
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

using content::NativeWebKeyboardEvent;
using content::WebContents;
using content::WebUIMessageHandler;

namespace browser {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
gfx::NativeWindow ShowWebDialog(gfx::NativeWindow parent,
                                Profile* profile,
                                Browser* browser,
                                WebDialogDelegate* delegate) {
  views::Widget* widget = views::Widget::CreateWindowWithParent(
      new WebDialogView(profile, browser, delegate), parent);
  widget->Show();
  return widget->GetNativeWindow();
}

}  // namespace browser

////////////////////////////////////////////////////////////////////////////////
// WebDialogView, public:

WebDialogView::WebDialogView(Profile* profile,
                             Browser* browser,
                             WebDialogDelegate* delegate)
    : WebDialogWebContentsDelegate(profile),
      initialized_(false),
      delegate_(delegate),
      dialog_controller_(new WebDialogController(this, profile, browser)),
      web_view_(new views::WebView(profile)) {
  web_view_->set_allow_accelerators(true);
  AddChildView(web_view_);
  SetLayoutManager(new views::FillLayout);
  // Pressing the ESC key will close the dialog.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

WebDialogView::~WebDialogView() {
}

content::WebContents* WebDialogView::web_contents() {
  return web_view_->web_contents();
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogView, views::View implementation:

gfx::Size WebDialogView::GetPreferredSize() {
  gfx::Size out;
  if (delegate_)
    delegate_->GetMinimumDialogSize(&out);
  return out;
}

bool WebDialogView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Pressing ESC closes the dialog.
  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
  OnDialogClosed(std::string());
  return true;
}

void WebDialogView::ViewHierarchyChanged(bool is_add,
                                         views::View* parent,
                                         views::View* child) {
  if (is_add && GetWidget())
    InitDialog();
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogView, views::WidgetDelegate implementation:

bool WebDialogView::CanResize() const {
  return true;
}

ui::ModalType WebDialogView::GetModalType() const {
  return GetDialogModalType();
}

string16 WebDialogView::GetWindowTitle() const {
  if (delegate_)
    return delegate_->GetDialogTitle();
  return string16();
}

std::string WebDialogView::GetWindowName() const {
  if (delegate_)
    return delegate_->GetDialogName();
  return std::string();
}

void WebDialogView::WindowClosing() {
  // If we still have a delegate that means we haven't notified it of the
  // dialog closing. This happens if the user clicks the Close button on the
  // dialog.
  if (delegate_)
    OnDialogClosed("");
}

views::View* WebDialogView::GetContentsView() {
  return this;
}

views::View* WebDialogView::GetInitiallyFocusedView() {
  return web_view_;
}

bool WebDialogView::ShouldShowWindowTitle() const {
  return ShouldShowDialogTitle();
}

views::Widget* WebDialogView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* WebDialogView::GetWidget() const {
  return View::GetWidget();
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogDelegate implementation:

ui::ModalType WebDialogView::GetDialogModalType() const {
  if (delegate_)
    return delegate_->GetDialogModalType();
  return ui::MODAL_TYPE_NONE;
}

string16 WebDialogView::GetDialogTitle() const {
  return GetWindowTitle();
}

GURL WebDialogView::GetDialogContentURL() const {
  if (delegate_)
    return delegate_->GetDialogContentURL();
  return GURL();
}

void WebDialogView::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  if (delegate_)
    delegate_->GetWebUIMessageHandlers(handlers);
}

void WebDialogView::GetDialogSize(gfx::Size* size) const {
  if (delegate_)
    delegate_->GetDialogSize(size);
}

void WebDialogView::GetMinimumDialogSize(gfx::Size* size) const {
  if (delegate_)
    delegate_->GetMinimumDialogSize(size);
}

std::string WebDialogView::GetDialogArgs() const {
  if (delegate_)
    return delegate_->GetDialogArgs();
  return std::string();
}

void WebDialogView::OnDialogClosed(const std::string& json_retval) {
  Detach();
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

void WebDialogView::OnCloseContents(WebContents* source,
                                    bool* out_close_dialog) {
  if (delegate_)
    delegate_->OnCloseContents(source, out_close_dialog);
}

bool WebDialogView::ShouldShowDialogTitle() const {
  if (delegate_)
    return delegate_->ShouldShowDialogTitle();
  return true;
}

bool WebDialogView::HandleContextMenu(
    const content::ContextMenuParams& params) {
  if (delegate_)
    return delegate_->HandleContextMenu(params);
  return WebDialogWebContentsDelegate::HandleContextMenu(params);
}

////////////////////////////////////////////////////////////////////////////////
// content::WebContentsDelegate implementation:

void WebDialogView::MoveContents(WebContents* source, const gfx::Rect& pos) {
  // The contained web page wishes to resize itself. We let it do this because
  // if it's a dialog we know about, we trust it not to be mean to the user.
  GetWidget()->SetBounds(pos);
}

// A simplified version of BrowserView::HandleKeyboardEvent().
// We don't handle global keyboard shortcuts here, but that's fine since
// they're all browser-specific. (This may change in the future.)
void WebDialogView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
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

void WebDialogView::CloseContents(WebContents* source) {
  bool close_dialog = false;
  OnCloseContents(source, &close_dialog);
  if (close_dialog)
    OnDialogClosed(std::string());
}

content::WebContents* WebDialogView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  content::WebContents* new_contents = NULL;
  if (delegate_ &&
      delegate_->HandleOpenURLFromTab(source, params, &new_contents)) {
    return new_contents;
  }
  return WebDialogWebContentsDelegate::OpenURLFromTab(source, params);
}

void WebDialogView::AddNewContents(content::WebContents* source,
                                   content::WebContents* new_contents,
                                   WindowOpenDisposition disposition,
                                   const gfx::Rect& initial_pos,
                                   bool user_gesture) {
  if (delegate_ && delegate_->HandleAddNewContents(
          source, new_contents, disposition, initial_pos, user_gesture)) {
    return;
  }
  WebDialogWebContentsDelegate::AddNewContents(
      source, new_contents, disposition, initial_pos, user_gesture);
}

void WebDialogView::LoadingStateChanged(content::WebContents* source) {
  if (delegate_)
    delegate_->OnLoadingStateChanged(source);
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogView, TabRenderWatcher::Delegate implementation:

void WebDialogView::OnRenderHostCreated(content::RenderViewHost* host) {
}

void WebDialogView::OnTabMainFrameLoaded() {
}

void WebDialogView::OnTabMainFrameRender() {
  tab_watcher_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogView, private:

void WebDialogView::InitDialog() {
  content::WebContents* web_contents = web_view_->GetWebContents();
  if (web_contents->GetDelegate() == this)
    return;

  web_contents->SetDelegate(this);

  // Set the delegate. This must be done before loading the page. See
  // the comment above WebDialogUI in its header file for why.
  WebDialogUI::GetPropertyAccessor().SetProperty(
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
