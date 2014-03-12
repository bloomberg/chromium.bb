// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_popup.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/gfx/insets.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/activation_client.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"
#endif

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

using content::BrowserContext;
using content::RenderViewHost;
using content::WebContents;

namespace {

// Returns true if |possible_owner| is the owner of |child|.
bool IsOwnerOf(gfx::NativeView child, gfx::NativeView possible_owner) {
  if (!child)
    return false;
#if defined(OS_WIN)
  if (::GetWindow(views::HWNDForNativeView(child), GW_OWNER) ==
      views::HWNDForNativeView(possible_owner))
    return true;
#endif
  return false;
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
          &ExtensionPopup::OnDevToolsStateChanged, base::Unretained(this))) {
  inspect_with_devtools_ = show_action == SHOW_AND_INSPECT;
  // Adjust the margin so that contents fit better.
  const int margin = views::BubbleBorder::GetCornerRadius() / 2;
  set_margins(gfx::Insets(margin, margin, margin, margin));
  SetLayoutManager(new views::FillLayout());
  AddChildView(host->view());
  host->view()->set_container(this);
  // Use OnNativeFocusChange to check for child window activation on deactivate.
  set_close_on_deactivate(false);
  // Make the bubble move with its anchor (during inspection, etc.).
  set_move_with_anchor(true);

  // Wait to show the popup until the contained host finishes loading.
  registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                 content::Source<WebContents>(host->host_contents()));

  // Listen for the containing view calling window.close();
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<BrowserContext>(host->browser_context()));
  content::DevToolsManager::GetInstance()->AddAgentStateCallback(
      devtools_callback_);

  host_->view()->browser()->tab_strip_model()->AddObserver(this);
}

ExtensionPopup::~ExtensionPopup() {
  content::DevToolsManager::GetInstance()->RemoveAgentStateCallback(
      devtools_callback_);

  host_->view()->browser()->tab_strip_model()->RemoveObserver(this);
}

void ExtensionPopup::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME:
      DCHECK(content::Source<WebContents>(host()->host_contents()) == source);
      // Show when the content finishes loading and its width is computed.
      ShowBubble();
      break;
    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (content::Details<extensions::ExtensionHost>(host()) == details)
        GetWidget()->Close();
      break;
    default:
      NOTREACHED() << L"Received unexpected notification";
  }
}

void ExtensionPopup::OnDevToolsStateChanged(
    content::DevToolsAgentHost* agent_host, bool attached) {
  // First check that the devtools are being opened on this popup.
  if (host()->render_view_host() != agent_host->GetRenderViewHost())
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

gfx::Size ExtensionPopup::GetPreferredSize() {
  // Constrain the size to popup min/max.
  gfx::Size sz = views::View::GetPreferredSize();
  sz.set_width(std::max(kMinWidth, std::min(kMaxWidth, sz.width())));
  sz.set_height(std::max(kMinHeight, std::min(kMaxHeight, sz.height())));
  return sz;
}

void ExtensionPopup::OnWidgetDestroying(views::Widget* widget) {
  BubbleDelegateView::OnWidgetDestroying(widget);
#if defined(USE_AURA)
  aura::Window* bubble_window = GetWidget()->GetNativeWindow();
  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(bubble_window->GetRootWindow());
  activation_client->RemoveObserver(this);
#endif
}

void ExtensionPopup::OnWidgetActivationChanged(views::Widget* widget,
                                               bool active) {
  // Dismiss only if the window being activated is not owned by this popup's
  // window. In particular, don't dismiss when we lose activation to a child
  // dialog box. Possibly relevant: http://crbug.com/106723 and
  // http://crbug.com/179786
  views::Widget* this_widget = GetWidget();

  // TODO(msw): Resolve crashes and remove checks. See: http://crbug.com/327776
  CHECK(!close_on_deactivate());
  CHECK(this_widget);
  CHECK(widget);

  gfx::NativeView activated_view = widget->GetNativeView();
  gfx::NativeView this_view = this_widget->GetNativeView();
  if (active && !inspect_with_devtools_ && activated_view != this_view &&
      !IsOwnerOf(activated_view, this_view))
    this_widget->Close();
}

#if defined(USE_AURA)
void ExtensionPopup::OnWindowActivated(aura::Window* gained_active,
                                       aura::Window* lost_active) {
  // DesktopNativeWidgetAura does not trigger the expected browser widget
  // [de]activation events when activating widgets in its own root window.
  // This additional check handles those cases. See: http://crbug.com/320889
  aura::Window* this_window = GetWidget()->GetNativeWindow();
  aura::Window* anchor_window = anchor_widget()->GetNativeWindow();
  chrome::HostDesktopType host_desktop_type =
      chrome::GetHostDesktopTypeForNativeWindow(this_window);
  if (!inspect_with_devtools_ && anchor_window == gained_active &&
      host_desktop_type != chrome::HOST_DESKTOP_TYPE_ASH &&
      this_window->GetRootWindow() == anchor_window->GetRootWindow() &&
      wm::GetTransientParent(gained_active) != this_window)
    GetWidget()->Close();
}
#endif

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

#if defined(USE_AURA)
  gfx::NativeView native_view = popup->GetWidget()->GetNativeView();
  wm::SetWindowVisibilityAnimationType(
      native_view,
      wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  wm::SetWindowVisibilityAnimationVerticalPosition(
      native_view,
      -3.0f);
#endif

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading())
    popup->ShowBubble();

#if defined(USE_AURA)
  aura::Window* bubble_window = popup->GetWidget()->GetNativeWindow();
  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(bubble_window->GetRootWindow());
  activation_client->AddObserver(popup);
#endif

  return popup;
}

void ExtensionPopup::ShowBubble() {
  GetWidget()->Show();

  // Focus on the host contents when the bubble is first shown.
  host()->host_contents()->GetView()->Focus();

  if (inspect_with_devtools_) {
    DevToolsWindow::OpenDevToolsWindow(host()->render_view_host(),
        DevToolsToggleAction::ShowConsole());
  }
}
