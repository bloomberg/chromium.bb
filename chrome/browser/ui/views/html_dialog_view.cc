// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/html_dialog_view.h"

#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/window.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/views/widget/native_widget_gtk.h"
#endif

class RenderWidgetHost;

namespace browser {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
gfx::NativeWindow ShowHtmlDialog(gfx::NativeWindow parent,
                                 Profile* profile,
                                 HtmlDialogUIDelegate* delegate) {
  // It's not always safe to display an html dialog with an off the record
  // profile.  If the last browser with that profile is closed it will go
  // away.
  DCHECK(!profile->IsOffTheRecord() || delegate->IsDialogModal());
  HtmlDialogView* html_view = new HtmlDialogView(profile, delegate);
  browser::CreateViewsWindow(parent, html_view);
  html_view->InitDialog();
  html_view->GetWidget()->Show();
  return html_view->GetWidget()->GetNativeWindow();
}

}  // namespace browser

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, public:

HtmlDialogView::HtmlDialogView(Profile* profile,
                               HtmlDialogUIDelegate* delegate)
    : DOMView(),
      HtmlDialogTabContentsDelegate(profile),
      initialized_(false),
      delegate_(delegate) {
}

HtmlDialogView::~HtmlDialogView() {
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, views::View implementation:

gfx::Size HtmlDialogView::GetPreferredSize() {
  gfx::Size out;
  if (delegate_)
    delegate_->GetDialogSize(&out);
  return out;
}

bool HtmlDialogView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Pressing ESC closes the dialog.
  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
  OnDialogClosed(std::string());
  return true;
}

void HtmlDialogView::ViewHierarchyChanged(
    bool is_add, View* parent, View* child) {
  DOMView::ViewHierarchyChanged(is_add, parent, child);
  if (is_add && GetWidget() && !initialized_) {
    initialized_ = true;
#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
    CHECK(
        static_cast<views::NativeWidgetGtk*>(
            GetWidget()->native_widget())->SuppressFreezeUpdates());
#endif
    RegisterDialogAccelerators();
  }
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, views::WidgetDelegate implementation:

bool HtmlDialogView::CanResize() const {
  return true;
}

bool HtmlDialogView::IsModal() const {
  if (delegate_)
    return delegate_->IsDialogModal();
  return false;
}

string16 HtmlDialogView::GetWindowTitle() const {
  if (delegate_)
    return delegate_->GetDialogTitle();
  return string16();
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
  return this;
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

bool HtmlDialogView::IsDialogModal() const {
  return IsModal();
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

std::string HtmlDialogView::GetDialogArgs() const {
  if (delegate_)
    return delegate_->GetDialogArgs();
  return std::string();
}

void HtmlDialogView::OnDialogClosed(const std::string& json_retval) {
  HtmlDialogTabContentsDelegate::Detach();
  if (delegate_) {
    HtmlDialogUIDelegate* dialog_delegate = delegate_;
    delegate_ = NULL;  // We will not communicate further with the delegate.
    dialog_delegate->OnDialogClosed(json_retval);
  }
  GetWidget()->Close();
}

void HtmlDialogView::OnCloseContents(TabContents* source,
                                     bool* out_close_dialog) {
  if (delegate_)
    delegate_->OnCloseContents(source, out_close_dialog);
}

bool HtmlDialogView::ShouldShowDialogTitle() const {
  if (delegate_)
    return delegate_->ShouldShowDialogTitle();
  return true;
}

bool HtmlDialogView::HandleContextMenu(const ContextMenuParams& params) {
  if (delegate_)
    return delegate_->HandleContextMenu(params);
  return HtmlDialogTabContentsDelegate::HandleContextMenu(params);
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsDelegate implementation:

void HtmlDialogView::MoveContents(TabContents* source, const gfx::Rect& pos) {
  // The contained web page wishes to resize itself. We let it do this because
  // if it's a dialog we know about, we trust it not to be mean to the user.
  GetWidget()->SetBounds(pos);
}

// A simplified version of BrowserView::HandleKeyboardEvent().
// We don't handle global keyboard shortcuts here, but that's fine since
// they're all browser-specific. (This may change in the future.)
void HtmlDialogView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
#if defined(USE_AURA)
  // TODO(saintlou): Need to provide some Aura handling.
#elif defined(OS_WIN)
  // Any unhandled keyboard/character messages should be defproced.
  // This allows stuff like F10, etc to work correctly.
  DefWindowProc(event.os_event.hwnd, event.os_event.message,
                  event.os_event.wParam, event.os_event.lParam);
#elif defined(TOOLKIT_USES_GTK)
  views::NativeWidgetGtk* window_gtk =
      static_cast<views::NativeWidgetGtk*>(GetWidget()->native_widget());
  if (event.os_event && !event.skip_in_browser) {
    views::KeyEvent views_event(reinterpret_cast<GdkEvent*>(event.os_event));
    window_gtk->HandleKeyboardEvent(views_event);
  }
#endif
}

void HtmlDialogView::CloseContents(TabContents* source) {
  bool close_dialog = false;
  OnCloseContents(source, &close_dialog);
  if (close_dialog)
    OnDialogClosed(std::string());
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView:

void HtmlDialogView::InitDialog() {
  // Now Init the DOMView. This view runs in its own process to render the html.
  DOMView::Init(profile(), NULL);

  TabContents* tab_contents = dom_contents_->tab_contents();
  tab_contents->set_delegate(this);

  // Set the delegate. This must be done before loading the page. See
  // the comment above HtmlDialogUI in its header file for why.
  HtmlDialogUI::GetPropertyAccessor().SetProperty(
      tab_contents->property_bag(), this);
  tab_watcher_.reset(new TabFirstRenderWatcher(tab_contents, this));

  DOMView::LoadURL(GetDialogContentURL());
}

void HtmlDialogView::RegisterDialogAccelerators() {
  // Pressing the ESC key will close the dialog.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, false, false, false));
}

void HtmlDialogView::OnRenderHostCreated(RenderViewHost* host) {
}

void HtmlDialogView::OnTabMainFrameLoaded() {
}

void HtmlDialogView::OnTabMainFrameFirstRender() {
#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
  if (initialized_) {
    views::NativeWidgetGtk::UpdateFreezeUpdatesProperty(
        GTK_WINDOW(GetWidget()->GetNativeView()), false);
  }
#endif
}
