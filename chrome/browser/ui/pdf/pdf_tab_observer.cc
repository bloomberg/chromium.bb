// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/pdf_tab_observer.h"

#include "chrome/browser/ui/pdf/pdf_unsupported_feature.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/render_messages.h"

PDFTabObserver::PDFTabObserver(TabContents* tab_contents)
    : content::WebContentsObserver(tab_contents->web_contents()),
      tab_contents_(tab_contents) {
}

PDFTabObserver::~PDFTabObserver() {
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

bool PDFTabObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PDFTabObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PDFHasUnsupportedFeature,
                        OnPDFHasUnsupportedFeature)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void PDFTabObserver::OnPDFHasUnsupportedFeature() {
  PDFHasUnsupportedFeature(tab_contents_);
}
