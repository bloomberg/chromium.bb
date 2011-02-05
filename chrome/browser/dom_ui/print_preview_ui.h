// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_PRINT_PREVIEW_UI_H_
#define CHROME_BROWSER_DOM_UI_PRINT_PREVIEW_UI_H_
#pragma once

#include "base/ref_counted.h"
#include "chrome/browser/dom_ui/dom_ui.h"

class PrintPreviewUIHTMLSource;

class PrintPreviewUI : public DOMUI {
 public:
  explicit PrintPreviewUI(TabContents* contents);
  virtual ~PrintPreviewUI();

  PrintPreviewUIHTMLSource* html_source();

  // Notify the Web UI renderer that preview data is available.
  void PreviewDataIsAvailable();

 private:
  scoped_refptr<PrintPreviewUIHTMLSource> html_source_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewUI);
};

#endif  // CHROME_BROWSER_DOM_UI_PRINT_PREVIEW_UI_H_
