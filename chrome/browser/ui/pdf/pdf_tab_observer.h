// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_PDF_TAB_OBSERVER_H_
#define CHROME_BROWSER_UI_PDF_PDF_TAB_OBSERVER_H_
#pragma once

#include "content/public/browser/web_contents_observer.h"

class TabContents;

// Per-tab class to handle PDF messages.
class PDFTabObserver : public content::WebContentsObserver {
 public:
  explicit PDFTabObserver(TabContents* tab_contents);
  virtual ~PDFTabObserver();

 private:
  // content::WebContentsObserver overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Internal helpers ----------------------------------------------------------

  // Message handlers.
  void OnPDFHasUnsupportedFeature();

  // Our owning TabContents.
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(PDFTabObserver);
};

#endif  // CHROME_BROWSER_UI_PDF_PDF_TAB_OBSERVER_H_
