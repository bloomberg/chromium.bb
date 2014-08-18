// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_popup.h"

#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/gfx/insets.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace {

ExtensionViewViews* GetExtensionView(extensions::ExtensionViewHost* host) {
  return static_cast<ExtensionViewViews*>(host->view());
}

}  // namespace

// The minimum/maximum dimensions of the popup.
// The minimum is just a little larger than the size of the button itself.
// The maximum is an arbitrary number that should be smaller than most screens.
const int ExtensionPopup::kMinWidth = 25;
const int ExtensionPopup::kMinHeight = 25;
const int ExtensionPopup::kMaxWidth = 800;
const int ExtensionPopup::kMaxHeight = 600;

ExtensionPopup::ExtensionPopup(extensions::ExtensionViewHost* host,
                               views::View* anchor_view,
                               views::BubbleBorder::Arrow arrow,
                               ShowAction show_action)
    : BubbleDelegateView(anchor_view, arrow),
      host_(host),
      devtools_callback_(base::Bind(
          &ExtensionPopup::OnDevToolsStateChanged, base::Unretained(this))),
      widget_initialized_(false) {
  inspect_with_devtools_ = show_action == SHOW_AND_INSPECT;
  // Adjust the margin so that contents fit better.
  const int margin = views::BubbleBorder::GetCornerRadius() / 2;
  set_margins(gfx::Insets(margin, margin, margin, margin));
  SetLayoutManager(new views::FillLayout());
  AddChildView(GetExtensionView(host));
  GetExtensionView(host)->set_container(this);
  // ExtensionPopup closes itself on very specific de-activation conditions.
  set_close_on_deactivate(false);

  // Wait to show the popup until the contained host finishes loading.
  registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                 content::Source<content::WebContents>(host->host_contents()));

  // Listen for the containing view calling window.close();
  registrar_.Add(
      this,
      extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      content::Source<content::BrowserContext>(host->browser_context()));
  content::DevToolsAgentHost::AddAgentStateCallback(devtools_callback_);

  GetExtensionView(host)->GetBrowser()->tab_strip_model()->AddObserver(this);
}

ExtensionPopup::~ExtensionPopup() {
  content::DevToolsAgentHost::RemoveAgentStateCallback(devtools_callback_);

  GetExtensionView(
      host_.get())->GetBrowser()->tab_strip_model()->RemoveObserver(this);
}

void ExtensionPopup::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME:
      DCHECK_EQ(host()->host_contents(),
                content::Source<content::WebContents>(source).ptr());
      // Show when the content finishes loading and its width is computed.
      ShowBubble();
      break;
    case extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (content::Details<extensions::ExtensionHost>(host()) == details)
        GetWidget()->Close();
      break;
    default:
      NOTREACHED() << L"Received unexpected notification";
  }
}

void ExtensionPopup::OnDevToolsStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  // First check that the devtools are being opened on this popup.
  if (host()->host_contents() != agent_host->GetWebContents())
    return;

  if (attached) {
    // Set inspect_with_devtools_ so the popup will be kept open while
    // the devtools are open.
    inspect_with_devtools_ = true;
  } else {
    // Widget::Close posts a task, which should give the devtools window a
    // chance to finish detaching from the inspected RenderViewHost.
    GetWidget()->Close();
  }
}

void ExtensionPopup::OnExtensionSizeChanged(ExtensionViewViews* view) {
  SizeToContents();
}

gfx::Size ExtensionPopup::GetPreferredSize() const {
  // Constrain the size to popup min/max.
  gfx::Size sz = views::View::GetPreferredSize();
  sz.set_width(std::max(kMinWidth, std::min(kMaxWidth, sz.width())));
  sz.set_height(std::max(kMinHeight, std::min(kMaxHeight, sz.height())));
  return sz;
}

void ExtensionPopup::ViewHierarchyChanged(
  const ViewHierarchyChangedDetails& details) {
  // TODO(msw): Find any remaining crashes related to http://crbug.com/327776
  // No view hierarchy changes are expected if the widget no longer exists.
  widget_initialized_ |= details.child == this && details.is_add && GetWidget();
  CHECK(GetWidget() || !widget_initialized_);
}

void ExtensionPopup::OnWidgetDestroying(views::Widget* widget) {
  BubbleDelegateView::OnWidgetDestroying(widget);
  aura::Window* bubble_window = GetWidget()->GetNativeWindow();
  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(bubble_window->GetRootWindow());
  // If the popup was being inspected with devtools and the browser window was
  // closed, then the root window and activation client are already destroyed.
  if (activation_client)
    activation_client->RemoveObserver(this);
}

void ExtensionPopup::OnWidgetActivationChanged(views::Widget* widget,
                                               bool active) {
  // TODO(msw): Find any remaining crashes related to http://crbug.com/327776
  // No calls are expected if the widget isn't initialized or no longer exists.
  CHECK(widget_initialized_);
  CHECK(GetWidget());

  // Close on anchor window activation (ie. user clicked the browser window).
  if (!inspect_with_devtools_ && widget && active &&
      widget->GetNativeWindow() == anchor_widget()->GetNativeWindow())
    GetWidget()->Close();
}

void ExtensionPopup::OnWindowActivated(aura::Window* gained_active,
                                       aura::Window* lost_active) {
  // TODO(msw): Find any remaining crashes related to http://crbug.com/327776
  // No calls are expected if the widget isn't initialized or no longer exists.
  CHECK(widget_initialized_);
  CHECK(GetWidget());

  // Close on anchor window activation (ie. user clicked the browser window).
  // DesktopNativeWidgetAura does not trigger the expected browser widget
  // [de]activation events when activating widgets in its own root window.
  // This additional check handles those cases. See: http://crbug.com/320889
  if (!inspect_with_devtools_ &&
      gained_active == anchor_widget()->GetNativeWindow())
    GetWidget()->Close();
}

void ExtensionPopup::ActiveTabChanged(content::WebContents* old_contents,
                                      content::WebContents* new_contents,
                                      int index,
                                      int reason) {
  GetWidget()->Close();
}

// static
ExtensionPopup* ExtensionPopup::ShowPopup(const GURL& url,
                                          Browser* browser,
                                          views::View* anchor_view,
                                          views::BubbleBorder::Arrow arrow,
                                          ShowAction show_action) {
  extensions::ExtensionViewHost* host =
      extensions::ExtensionViewHostFactory::CreatePopupHost(url, browser);
  ExtensionPopup* popup = new ExtensionPopup(host, anchor_view, arrow,
      show_action);
  views::BubbleDelegateView::CreateBubble(popup);

  gfx::NativeView native_view = popup->GetWidget()->GetNativeView();
  wm::SetWindowVisibilityAnimationType(
      native_view, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  wm::SetWindowVisibilityAnimationVerticalPosition(native_view, -3.0f);

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading())
    popup->ShowBubble();

  aura::Window* bubble_window = popup->GetWidget()->GetNativeWindow();
  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(bubble_window->GetRootWindow());
  activation_client->AddObserver(popup);

  return popup;
}

void ExtensionPopup::ShowBubble() {
  GetWidget()->Show();

  // Focus on the host contents when the bubble is first shown.
  host()->host_contents()->Focus();

  if (inspect_with_devtools_) {
    DevToolsWindow::OpenDevToolsWindow(host()->host_contents(),
                                       DevToolsToggleAction::ShowConsole());
  }
}
