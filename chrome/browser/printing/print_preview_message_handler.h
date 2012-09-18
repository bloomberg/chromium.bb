// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_

#include "base/compiler_specific.h"
#include "chrome/browser/tab_contents/web_contents_user_data.h"
#include "content/public/browser/web_contents_observer.h"

class PrintPreviewUI;
class TabContents;
struct PrintHostMsg_DidGetPreviewPageCount_Params;
struct PrintHostMsg_DidPreviewDocument_Params;
struct PrintHostMsg_DidPreviewPage_Params;

namespace gfx {
class Rect;
}

namespace printing {

struct PageSizeMargins;

// Manages the print preview handling for a WebContents.
class PrintPreviewMessageHandler
    : public content::WebContentsObserver,
      public WebContentsUserData<PrintPreviewMessageHandler> {
 public:
  virtual ~PrintPreviewMessageHandler();

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  explicit PrintPreviewMessageHandler(content::WebContents* web_contents);
  static int kUserDataKey;
  friend class WebContentsUserData<PrintPreviewMessageHandler>;

  // Gets the print preview tab associated with the WebContents being observed.
  TabContents* GetPrintPreviewTab();

  // Gets the PrintPreviewUI associated with the WebContents being observed.
  PrintPreviewUI* GetPrintPreviewUI();

  // Message handlers.
  void OnRequestPrintPreview(bool source_is_modifiable, bool webnode_only);
  void OnDidGetDefaultPageLayout(
      const printing::PageSizeMargins& page_layout_in_points,
      const gfx::Rect& printable_area_in_points,
      bool has_custom_page_size_style);
  void OnDidGetPreviewPageCount(
      const PrintHostMsg_DidGetPreviewPageCount_Params& params);
  void OnDidPreviewPage(const PrintHostMsg_DidPreviewPage_Params& params);
  void OnMetafileReadyForPrinting(
      const PrintHostMsg_DidPreviewDocument_Params& params);
  void OnPrintPreviewFailed(int document_cookie);
  void OnPrintPreviewCancelled(int document_cookie);
  void OnInvalidPrinterSettings(int document_cookie);
  void OnPrintPreviewScalingDisabled();

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewMessageHandler);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_
