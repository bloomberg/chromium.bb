// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/pdf_tab_observer.h"

#include "chrome/browser/ui/pdf/pdf_unsupported_feature.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/render_messages.h"

PDFTabObserver::PDFTabObserver(TabContentsWrapper* wrapper)
    : content::WebContentsObserver(wrapper->web_contents()),
      wrapper_(wrapper) {
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
  PDFHasUnsupportedFeature(wrapper_);
}
