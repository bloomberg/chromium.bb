// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_
#pragma once

#include "chrome/browser/tab_contents/tab_contents_observer.h"

struct ViewHostMsg_DidPreviewDocument_Params;

namespace printing {

// TabContents offloads print preview message handling to
// PrintPreviewMessageHandler. This object has the same life time as the
// TabContents that owns it.
class PrintPreviewMessageHandler : public TabContentsObserver {
 public:
  explicit PrintPreviewMessageHandler(TabContents* tab_contents);
  virtual ~PrintPreviewMessageHandler();

  void OnPagesReadyForPreview(
      const ViewHostMsg_DidPreviewDocument_Params& params);

  // TabContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  // Gets the print preview tab associated with |owner_|.
  TabContents* GetPrintPreviewTab();

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewMessageHandler);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_MESSAGE_HANDLER_H_
