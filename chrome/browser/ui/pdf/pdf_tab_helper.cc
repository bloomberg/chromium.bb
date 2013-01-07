// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/pdf_tab_helper.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/pdf/open_pdf_in_reader_prompt_delegate.h"
#include "chrome/browser/ui/pdf/pdf_unsupported_feature.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_details.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PDFTabHelper);

PDFTabHelper::PDFTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

PDFTabHelper::~PDFTabHelper() {
}

void PDFTabHelper::ShowOpenInReaderPrompt(
    scoped_ptr<OpenPDFInReaderPromptDelegate> prompt) {
  open_in_reader_prompt_ = prompt.Pass();
  UpdateLocationBar();
}

bool PDFTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PDFTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PDFHasUnsupportedFeature,
                        OnPDFHasUnsupportedFeature)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PDFTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (open_in_reader_prompt_.get() &&
      open_in_reader_prompt_->ShouldExpire(details)) {
    open_in_reader_prompt_.reset();
    UpdateLocationBar();
  }
}

void PDFTabHelper::UpdateLocationBar() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
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

void PDFTabHelper::OnPDFHasUnsupportedFeature() {
  PDFHasUnsupportedFeature(web_contents());
}
