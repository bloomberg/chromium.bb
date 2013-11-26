// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host.h"

#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/render_view_host.h"

using content::NativeWebKeyboardEvent;
using content::OpenURLParams;
using content::RenderViewHost;
using content::WebContents;

namespace extensions {

ExtensionViewHost::ExtensionViewHost(
    const Extension* extension,
    content::SiteInstance* site_instance,
    const GURL& url,
    ViewType host_type)
    : ExtensionHost(extension, site_instance, url, host_type) {
  // Not used for panels, see PanelHost.
  DCHECK(host_type == VIEW_TYPE_EXTENSION_DIALOG ||
         host_type == VIEW_TYPE_EXTENSION_INFOBAR ||
         host_type == VIEW_TYPE_EXTENSION_POPUP);
}

ExtensionViewHost::~ExtensionViewHost() {}

void ExtensionViewHost::CreateView(Browser* browser) {
#if defined(TOOLKIT_VIEWS)
  view_.reset(new ExtensionViewViews(this, browser));
  // We own |view_|, so don't auto delete when it's removed from the view
  // hierarchy.
  view_->set_owned_by_client();
#elif defined(OS_MACOSX)
  view_.reset(new ExtensionViewMac(this, browser));
  view_->Init();
#elif defined(TOOLKIT_GTK)
  view_.reset(new ExtensionViewGtk(this, browser));
  view_->Init();
#else
  // TODO(port)
  NOTREACHED();
#endif
}

// ExtensionHost overrides:

void ExtensionViewHost::UnhandledKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  Browser* browser = view_->browser();
  if (browser) {
    // Handle lower priority browser shortcuts such as Ctrl-f.
    return browser->HandleKeyboardEvent(source, event);
  } else {
#if defined(TOOLKIT_VIEWS)
    // In case there's no Browser (e.g. for dialogs), pass it to
    // ExtensionViewViews to handle accelerators. The view's FocusManager does
    // not know anything about Browser accelerators, but might know others such
    // as Ash's.
    view_->HandleKeyboardEvent(event);
#endif
  }
}

void ExtensionViewHost::OnDidStopLoading() {
  DCHECK(did_stop_loading());
#if defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)
  view_->DidStopLoading();
#endif
}

bool ExtensionViewHost::IsBackgroundPage() const {
  DCHECK(view_);
  return false;
}

WebContents* ExtensionViewHost::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params) {
  // Whitelist the dispositions we will allow to be opened.
  switch (params.disposition) {
    case SINGLETON_TAB:
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB:
    case NEW_POPUP:
    case NEW_WINDOW:
    case SAVE_TO_DISK:
    case OFF_THE_RECORD: {
      // Only allow these from hosts that are bound to a browser (e.g. popups).
      // Otherwise they are not driven by a user gesture.
      Browser* browser = view_->browser();
      return browser ? browser->OpenURL(params) : NULL;
    }
    default:
      return NULL;
  }
}

bool ExtensionViewHost::PreHandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP &&
      event.type == NativeWebKeyboardEvent::RawKeyDown &&
      event.windowsKeyCode == ui::VKEY_ESCAPE) {
    DCHECK(is_keyboard_shortcut != NULL);
    *is_keyboard_shortcut = true;
    return false;
  }

  // Handle higher priority browser shortcuts such as Ctrl-w.
  Browser* browser = view_->browser();
  if (browser)
    return browser->PreHandleKeyboardEvent(source, event, is_keyboard_shortcut);

  *is_keyboard_shortcut = false;
  return false;
}

void ExtensionViewHost::HandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP) {
    if (event.type == NativeWebKeyboardEvent::RawKeyDown &&
        event.windowsKeyCode == ui::VKEY_ESCAPE) {
      Close();
      return;
    }
  }
  UnhandledKeyboardEvent(source, event);
}

void ExtensionViewHost::ResizeDueToAutoResize(WebContents* source,
                                          const gfx::Size& new_size) {
  view_->ResizeDueToAutoResize(new_size);
}

// content::WebContentsObserver

void ExtensionViewHost::RenderViewCreated(RenderViewHost* render_view_host) {
  ExtensionHost::RenderViewCreated(render_view_host);

  view_->RenderViewCreated();

  // If the host is bound to a window, then extract its id. Extensions hosted
  // in ExternalTabContainer objects may not have an associated window.
  WindowController* window = GetExtensionWindowController();
  if (window) {
    render_view_host->Send(new ExtensionMsg_UpdateBrowserWindowId(
        render_view_host->GetRoutingID(), window->GetWindowId()));
  }
}

#if !defined(OS_ANDROID)
gfx::NativeView ExtensionViewHost::GetHostView() const {
  return view_->native_view();
}
#endif  // !defined(OS_ANDROID)

WindowController* ExtensionViewHost::GetExtensionWindowController() const {
  return view_->browser() ? view_->browser()->extension_window_controller()
                          : NULL;
}

}  // namespace extensions
