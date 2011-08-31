// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/html_dialog_view.h"

#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/window.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/native_web_keyboard_event.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/events/event.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

#if defined(TOOLKIT_USES_GTK)
#include "views/widget/native_widget_gtk.h"
#endif

class RenderWidgetHost;

namespace browser {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
gfx::NativeWindow ShowHtmlDialog(gfx::NativeWindow parent, Profile* profile,
                                 HtmlDialogUIDelegate* delegate) {
  HtmlDialogView* html_view =
      new HtmlDialogView(profile, delegate);
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
      state_(NONE),
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

bool HtmlDialogView::AcceleratorPressed(const views::Accelerator& accelerator) {
  // Pressing ESC closes the dialog.
  DCHECK_EQ(ui::VKEY_ESCAPE, accelerator.key_code());
  OnDialogClosed(std::string());
  return true;
}

void HtmlDialogView::ViewHierarchyChanged(
    bool is_add, View* parent, View* child) {
  DOMView::ViewHierarchyChanged(is_add, parent, child);
  if (is_add && GetWidget() && state_ == NONE) {
    state_ = INITIALIZED;
#if defined(OS_CHROMEOS)
    CHECK(
        static_cast<views::NativeWidgetGtk*>(
            GetWidget()->native_widget())->SuppressFreezeUpdates());
#endif
    RegisterDialogAccelerators();
  }
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogView, views::WindowDelegate implementation:

bool HtmlDialogView::CanResize() const {
  return true;
}

bool HtmlDialogView::IsModal() const {
  if (delegate_)
    return delegate_->IsDialogModal();
  return false;
}

std::wstring HtmlDialogView::GetWindowTitle() const {
  if (delegate_)
    return UTF16ToWideHack(delegate_->GetDialogTitle());
  return std::wstring();
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
  return WideToUTF16Hack(GetWindowTitle());
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
#if defined(OS_WIN)
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

  tab_contents_->set_delegate(this);

  // Set the delegate. This must be done before loading the page. See
  // the comment above HtmlDialogUI in its header file for why.
  HtmlDialogUI::GetPropertyAccessor().SetProperty(tab_contents_->property_bag(),
                                                  this);
  notification_registrar_.Add(
      this,
      content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
      Source<TabContents>(tab_contents()));
  notification_registrar_.Add(
      this,
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      Source<TabContents>(tab_contents()));

  DOMView::LoadURL(GetDialogContentURL());
}

void HtmlDialogView::Observe(int type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      RenderWidgetHost* rwh = Details<RenderWidgetHost>(details).ptr();
      notification_registrar_.Add(
          this,
          content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT,
          Source<RenderWidgetHost>(rwh));
      break;
    }
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME:
      if (state_ == INITIALIZED)
        state_ = LOADED;
      break;
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT:
      if (state_ == LOADED) {
        state_ = PAINTED;
#if defined(OS_CHROMEOS)
        views::NativeWidgetGtk::UpdateFreezeUpdatesProperty(
            GTK_WINDOW(GetWidget()->GetNativeView()), false);
#endif
      }
      break;
    default:
      NOTREACHED() << "unknown type" << type;
  }
}

void HtmlDialogView::RegisterDialogAccelerators() {
  // Pressing the ESC key will close the dialog.
  AddAccelerator(views::Accelerator(ui::VKEY_ESCAPE, false, false, false));
}
