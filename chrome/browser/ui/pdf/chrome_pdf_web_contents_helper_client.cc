// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"

#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/pdf/pdf_unsupported_feature.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"

ChromePDFWebContentsHelperClient::ChromePDFWebContentsHelperClient() {
}

ChromePDFWebContentsHelperClient::~ChromePDFWebContentsHelperClient() {
}

void ChromePDFWebContentsHelperClient::UpdateLocationBar(
    content::WebContents* contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser)
    return;

  BrowserWindow* window = browser->window();
  if (!window)
    return;

  LocationBar* location_bar = window->GetLocationBar();
  if (!location_bar)
    return;

  location_bar->UpdateOpenPDFInReaderPrompt();
}

void ChromePDFWebContentsHelperClient::UpdateContentRestrictions(
    content::WebContents* contents,
    int content_restrictions) {
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(contents);
  // |core_tab_helper| is NULL for WebViewGuest.
  if (core_tab_helper)
    core_tab_helper->UpdateContentRestrictions(content_restrictions);
}

void ChromePDFWebContentsHelperClient::OnPDFHasUnsupportedFeature(
    content::WebContents* contents) {
  PDFHasUnsupportedFeature(contents);
}

void ChromePDFWebContentsHelperClient::OnSaveURL(
    content::WebContents* contents) {
  RecordDownloadSource(DOWNLOAD_INITIATED_BY_PDF_SAVE);
}

void ChromePDFWebContentsHelperClient::OnShowPDFPasswordDialog(
    content::WebContents* contents,
    const base::string16& prompt,
    const pdf::PasswordDialogClosedCallback& callback) {
  ShowPDFPasswordDialog(contents, prompt, callback);
}
