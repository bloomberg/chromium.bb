// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/mime_handler_view/chrome_mime_handler_view_guest_delegate.h"

#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)

ChromeMimeHandlerViewGuestDelegate::ChromeMimeHandlerViewGuestDelegate(
    extensions::MimeHandlerViewGuest* guest)
    : extensions::MimeHandlerViewGuestDelegate(guest), guest_(guest) {
}

ChromeMimeHandlerViewGuestDelegate::~ChromeMimeHandlerViewGuestDelegate() {
}

// TODO(lazyboy): Investigate ways to move this out to /extensions.
void ChromeMimeHandlerViewGuestDelegate::AttachHelpers() {
  content::WebContents* web_contents = guest_->web_contents();
#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
  printing::PrintViewManager::CreateForWebContents(web_contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(web_contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents,
      scoped_ptr<pdf::PDFWebContentsHelperClient>(
          new ChromePDFWebContentsHelperClient()));
}

void ChromeMimeHandlerViewGuestDelegate::ChangeZoom(bool zoom_in) {
  // TODO(lazyboy): Move this to //extensions once ZoomController and friends
  // move to //extensions.
  chrome_page_zoom::Zoom(
      guest_->embedder_web_contents(),
      zoom_in ? content::PAGE_ZOOM_IN : content::PAGE_ZOOM_OUT);
}
