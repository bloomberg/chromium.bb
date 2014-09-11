// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/extension_options/chrome_extension_options_guest_delegate.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "extensions/browser/guest_view/extension_options/extension_options_guest.h"

namespace extensions {

ChromeExtensionOptionsGuestDelegate::ChromeExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest)
    : ExtensionOptionsGuestDelegate(guest) {
}

ChromeExtensionOptionsGuestDelegate::~ChromeExtensionOptionsGuestDelegate() {
}

void ChromeExtensionOptionsGuestDelegate::DidInitialize() {
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      extension_options_guest()->web_contents());
}

bool ChromeExtensionOptionsGuestDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate = ContextMenuDelegate::FromWebContents(
      extension_options_guest()->web_contents());
  DCHECK(menu_delegate);

  scoped_ptr<RenderViewContextMenu> menu = menu_delegate->BuildMenu(
      extension_options_guest()->web_contents(), params);
  menu_delegate->ShowMenu(menu.Pass());
  return true;
}

content::WebContents* ChromeExtensionOptionsGuestDelegate::OpenURLInNewTab(
    const content::OpenURLParams& params) {
  Browser* browser = chrome::FindBrowserWithWebContents(
      extension_options_guest()->embedder_web_contents());
  return browser->OpenURL(params);
}

}  // namespace extensions
