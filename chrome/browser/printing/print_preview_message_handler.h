// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_
#pragma once

#include "content/browser/tab_contents/tab_contents_observer.h"

class TabContentsWrapper;
struct PrintHostMsg_DidGetPreviewPageCount_Params;
struct PrintHostMsg_DidPreviewDocument_Params;
struct PrintHostMsg_DidPreviewPage_Params;

namespace printing {

struct PageSizeMargins;

// TabContents offloads print preview message handling to
// PrintPreviewMessageHandler. This object has the same life time as the
// TabContents that owns it.
class PrintPreviewMessageHandler : public TabContentsObserver {
 public:
  explicit PrintPreviewMessageHandler(TabContents* tab_contents);
  virtual ~PrintPreviewMessageHandler();

  // TabContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void DidStartLoading();

 private:
  // Gets the print preview tab associated with the TabContents being observed.
  TabContentsWrapper* GetPrintPreviewTab();
  // Helper function to return the TabContentsWrapper for tab_contents().
  TabContentsWrapper* tab_contents_wrapper();

  // Message handlers.
  void OnRequestPrintPreview();
  void OnDidGetDefaultPageLayout(
      const printing::PageSizeMargins& page_layout_in_points);
  void OnDidGetPreviewPageCount(
      const PrintHostMsg_DidGetPreviewPageCount_Params& params);
  void OnDidPreviewPage(const PrintHostMsg_DidPreviewPage_Params& params);
  void OnMetafileReadyForPrinting(
      const PrintHostMsg_DidPreviewDocument_Params& params);
  void OnPrintPreviewFailed(int document_cookie);
  void OnPrintPreviewCancelled(int document_cookie);

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewMessageHandler);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_
