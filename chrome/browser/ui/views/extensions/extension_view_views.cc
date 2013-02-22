// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_view_views.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/common/view_type.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/events/event.h"
#include "ui/views/widget/widget.h"

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
  set_focusable(true);
}

ExtensionViewViews::~ExtensionViewViews() {
  if (parent())
    parent()->RemoveChildView(this);
  CleanUp();
}

const extensions::Extension* ExtensionViewViews::extension() const {
  return host_->extension();
}

content::RenderViewHost* ExtensionViewViews::render_view_host() const {
  return host_->render_view_host();
}

void ExtensionViewViews::DidStopLoading() {
  ShowIfCompletelyLoaded();
}

void ExtensionViewViews::SetIsClipped(bool is_clipped) {
  if (is_clipped_ != is_clipped) {
    is_clipped_ = is_clipped;
    if (visible())
      ShowIfCompletelyLoaded();
  }
}

gfx::NativeCursor ExtensionViewViews::GetCursor(const ui::MouseEvent& event) {
  return gfx::kNullCursor;
}

void ExtensionViewViews::SetVisible(bool is_visible) {
  if (is_visible != visible()) {
    NativeViewHost::SetVisible(is_visible);

    // Also tell RenderWidgetHostView the new visibility. Despite its name, it
    // is not part of the View hierarchy and does not know about the change
    // unless we tell it.
    if (render_view_host()->GetView()) {
      if (is_visible)
        render_view_host()->GetView()->Show();
      else
        render_view_host()->GetView()->Hide();
    }
  }
}

void ExtensionViewViews::CreateWidgetHostView() {
  DCHECK(!initialized_);
  initialized_ = true;
  Attach(host_->host_contents()->GetView()->GetNativeView());
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

void ExtensionViewViews::ResizeDueToAutoResize(const gfx::Size& new_size) {
  // Don't actually do anything with this information until we have been shown.
  // Size changes will not be honored by lower layers while we are hidden.
  if (!visible()) {
    pending_preferred_size_ = new_size;
    return;
  }

  gfx::Size preferred_size = GetPreferredSize();
  if (new_size != preferred_size)
    SetPreferredSize(new_size);
}

void ExtensionViewViews::ViewHierarchyChanged(bool is_add,
                                              views::View* parent,
                                              views::View* child) {
  NativeViewHost::ViewHierarchyChanged(is_add, parent, child);
  if (is_add && GetWidget() && !initialized_)
    CreateWidgetHostView();
}

void ExtensionViewViews::PreferredSizeChanged() {
  View::PreferredSizeChanged();
  if (container_)
    container_->OnExtensionSizeChanged(this);
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
  if (render_view_host()->GetView() && !bounds().IsEmpty()) {
    render_view_host()->GetView()->SetSize(size());

    if (container_)
      container_->OnViewWasResized();
  }
}

void ExtensionViewViews::RenderViewCreated() {
  chrome::ViewType host_type = host_->extension_host_type();
  if (host_type == chrome::VIEW_TYPE_EXTENSION_POPUP) {
    gfx::Size min_size(ExtensionPopup::kMinWidth,
                       ExtensionPopup::kMinHeight);
    gfx::Size max_size(ExtensionPopup::kMaxWidth,
                       ExtensionPopup::kMaxHeight);
    render_view_host()->EnableAutoResize(min_size, max_size);
  }

  if (container_)
    container_->OnViewWasResized();
}

void ExtensionViewViews::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                        GetFocusManager());
}
