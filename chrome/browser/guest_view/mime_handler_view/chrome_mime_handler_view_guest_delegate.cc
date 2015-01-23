// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/mime_handler_view/chrome_mime_handler_view_guest_delegate.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "components/ui/zoom/page_zoom.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_PRINT_PREVIEW)
#endif  // defined(ENABLE_PRINTING)

namespace extensions {

ChromeMimeHandlerViewGuestDelegate::ChromeMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest)
    : MimeHandlerViewGuestDelegate(guest), guest_(guest) {
}

ChromeMimeHandlerViewGuestDelegate::~ChromeMimeHandlerViewGuestDelegate() {
}

// TODO(lazyboy): Investigate ways to move this out to /extensions.
void ChromeMimeHandlerViewGuestDelegate::AttachHelpers() {
  content::WebContents* web_contents = guest_->web_contents();
#if defined(ENABLE_PRINTING)
#if defined(ENABLE_PRINT_PREVIEW)
  printing::PrintViewManager::CreateForWebContents(web_contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(web_contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // defined(ENABLE_PRINT_PREVIEW)
#endif  // defined(ENABLE_PRINTING)
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents,
      scoped_ptr<pdf::PDFWebContentsHelperClient>(
          new ChromePDFWebContentsHelperClient()));
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
}

bool ChromeMimeHandlerViewGuestDelegate::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(web_contents);
  DCHECK(menu_delegate);

  scoped_ptr<RenderViewContextMenuBase> menu =
      menu_delegate->BuildMenu(web_contents, params);
  menu_delegate->ShowMenu(menu.Pass());
  return true;
}

}  // namespace extensions
