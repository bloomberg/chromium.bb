// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/extension_options/chrome_extension_options_guest_delegate.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/renderer_context_menu/context_menu_delegate.h"

namespace extensions {

ChromeExtensionOptionsGuestDelegate::ChromeExtensionOptionsGuestDelegate() {
}

ChromeExtensionOptionsGuestDelegate::~ChromeExtensionOptionsGuestDelegate() {
}

void
ChromeExtensionOptionsGuestDelegate::CreateChromeExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
}

bool ChromeExtensionOptionsGuestDelegate::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(web_contents);
  DCHECK(menu_delegate);

  scoped_ptr<RenderViewContextMenu> menu =
      menu_delegate->BuildMenu(web_contents, params);
  menu_delegate->ShowMenu(menu.Pass());
  return true;
}

content::WebContents* ChromeExtensionOptionsGuestDelegate::OpenURLInNewTab(
    content::WebContents* embedder_web_contents,
    const content::OpenURLParams& params) {
  Browser* browser = chrome::FindBrowserWithWebContents(embedder_web_contents);
  return browser->OpenURL(params);
}

}  // namespace extensions
