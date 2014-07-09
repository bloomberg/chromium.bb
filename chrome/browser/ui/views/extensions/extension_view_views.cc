// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_view_views.h"

#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/view_type.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

ExtensionViewViews::ExtensionViewViews(extensions::ExtensionHost* host,
                                       Browser* browser)
    : host_(host),
      browser_(browser),
      initialized_(false),
      container_(NULL),
      is_clipped_(false) {
  // This view needs to be focusable so it can act as the focused view for the
  // focus manager. This is required to have SkipDefaultKeyEventProcessing
  // called so the tab key events are forwarded to the renderer.
  SetFocusable(true);
}

ExtensionViewViews::~ExtensionViewViews() {
  if (parent())
    parent()->RemoveChildView(this);
  CleanUp();
}

gfx::Size ExtensionViewViews::GetMinimumSize() const {
  // If the minimum size has never been set, returns the preferred size (same
  // behavior as views::View).
  return (minimum_size_ == gfx::Size()) ? GetPreferredSize() : minimum_size_;
}

void ExtensionViewViews::SetVisible(bool is_visible) {
  if (is_visible != visible()) {
    NativeViewHost::SetVisible(is_visible);

    // Also tell RenderWidgetHostView the new visibility. Despite its name, it
    // is not part of the View hierarchy and does not know about the change
    // unless we tell it.
    content::RenderWidgetHostView* host_view = render_view_host()->GetView();
    if (host_view) {
      if (is_visible)
        host_view->Show();
      else
        host_view->Hide();
    }
  }
}

gfx::NativeCursor ExtensionViewViews::GetCursor(const ui::MouseEvent& event) {
  return gfx::kNullCursor;
}

void ExtensionViewViews::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  NativeViewHost::ViewHierarchyChanged(details);
  if (details.is_add && GetWidget() && !initialized_)
    CreateWidgetHostView();
}

void ExtensionViewViews::SetIsClipped(bool is_clipped) {
  if (is_clipped_ != is_clipped) {
    is_clipped_ = is_clipped;
    if (visible())
      ShowIfCompletelyLoaded();
  }
}

void ExtensionViewViews::Init() {
  // Initialization continues in ViewHierarchyChanged().
}

Browser* ExtensionViewViews::GetBrowser() {
  return browser_;
}

gfx::NativeView ExtensionViewViews::GetNativeView() {
  return native_view();
}

void ExtensionViewViews::ResizeDueToAutoResize(const gfx::Size& new_size) {
  // Don't actually do anything with this information until we have been shown.
  // Size changes will not be honored by lower layers while we are hidden.
  if (!visible()) {
    pending_preferred_size_ = new_size;
    return;
  }

  if (new_size != GetPreferredSize())
    SetPreferredSize(new_size);
}

void ExtensionViewViews::RenderViewCreated() {
  extensions::ViewType host_type = host_->extension_host_type();
  if (host_type == extensions::VIEW_TYPE_EXTENSION_POPUP) {
    render_view_host()->EnableAutoResize(
        gfx::Size(ExtensionPopup::kMinWidth, ExtensionPopup::kMinHeight),
        gfx::Size(ExtensionPopup::kMaxWidth, ExtensionPopup::kMaxHeight));
  }
}

void ExtensionViewViews::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (browser_) {
    // Handle lower priority browser shortcuts such as Ctrl-f.
    browser_->HandleKeyboardEvent(source, event);
    return;
  }

  unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                        GetFocusManager());
}

void ExtensionViewViews::DidStopLoading() {
  ShowIfCompletelyLoaded();
}

bool ExtensionViewViews::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  // Let the tab key event be processed by the renderer (instead of moving the
  // focus to the next focusable view). Also handle Backspace, since otherwise
  // (on Windows at least), pressing Backspace, when focus is on a text field
  // within the ExtensionViewViews, will navigate the page back instead of
  // erasing a character.
  return (e.key_code() == ui::VKEY_TAB || e.key_code() == ui::VKEY_BACK);
}

void ExtensionViewViews::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Propagate the new size to RenderWidgetHostView.
  // We can't send size zero because RenderWidget DCHECKs that.
  if (render_view_host()->GetView() && !bounds().IsEmpty())
    render_view_host()->GetView()->SetSize(size());
}

void ExtensionViewViews::PreferredSizeChanged() {
  View::PreferredSizeChanged();
  if (container_)
    container_->OnExtensionSizeChanged(this);
}

void ExtensionViewViews::OnFocus() {
  host()->host_contents()->Focus();
}

void ExtensionViewViews::CreateWidgetHostView() {
  DCHECK(!initialized_);
  initialized_ = true;
  Attach(host_->host_contents()->GetNativeView());
  host_->CreateRenderViewSoon();
  SetVisible(false);
}

void ExtensionViewViews::ShowIfCompletelyLoaded() {
  if (visible() || is_clipped_)
    return;

  // We wait to show the ExtensionViewViews until it has loaded, and the view
  // has actually been created. These can happen in different orders.
  if (host_->did_stop_loading()) {
    SetVisible(true);
    ResizeDueToAutoResize(pending_preferred_size_);
  }
}

void ExtensionViewViews::CleanUp() {
  if (!initialized_)
    return;
  if (native_view())
    Detach();
  initialized_ = false;
}

namespace extensions {

// static
scoped_ptr<ExtensionView> ExtensionViewHost::CreateExtensionView(
    ExtensionViewHost* host,
    Browser* browser) {
  scoped_ptr<ExtensionViewViews> view(new ExtensionViewViews(host, browser));
  // We own |view_|, so don't auto delete when it's removed from the view
  // hierarchy.
  view->set_owned_by_client();
  return view.PassAs<ExtensionView>();
}

}  // namespace extensions
