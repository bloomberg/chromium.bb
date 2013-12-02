// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_RENDER_HOST_PRINT_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_RENDER_HOST_PRINT_MANAGER_H_

#include "base/callback_forward.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace base {
struct FileDescriptor;
}

namespace printing {
class PrintSettings;
}

namespace android_webview {

class PrintManagerDelegate {
 public:
  virtual ~PrintManagerDelegate() { }
  virtual void DidExportPdf(bool success) = 0;
  virtual bool IsCancelled() = 0;

 private:
  //  DISALLOW_COPY_AND_ASSIGN(PrintManagerDelegate);
};

// Provides RenderViewHost wrapper functionality for sending WebView-specific
// IPC messages to the renderer and from there to WebKit.
//
// Note for android_webview:
// This class manages the print (an export to PDF file essentially) task for
// a webview. There can be at most one active print task per webview.
class PrintManager : public content::WebContentsObserver,
                     public base::NonThreadSafe {
 public:
  // To send receive messages to a RenderView we take the WebContents instance,
  // as it internally handles RenderViewHost instances changing underneath us.
  PrintManager(content::WebContents* contents,
               printing::PrintSettings* settings,
               int fd,
               PrintManagerDelegate* delegate);
  virtual ~PrintManager();

  // Prints the current document immediately. Since the rendering is
  // asynchronous, the actual printing will not be completed on the return of
  // this function. Returns false if printing is impossible at the moment.
  //
  // Note for webview: Returns false immediately if this print manager is
  // already busy printing.
  bool PrintNow();

 private:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  void OnDidGetPrintedPagesCount(int cookie, int number_pages);
  void OnDidGetDocumentCookie(int cookie);
  void OnPrintingFailed(int cookie);
  void OnGetDefaultPrintSettingsReply(IPC::Message* reply_msg);
  void OnGetDefaultPrintSettings(IPC::Message* reply_msg);
  void OnAllocateTempFileForPrinting(base::FileDescriptor* temp_file_fd,
                                     int* sequence_number);
  void OnTempFileForPrintingWritten(int sequence_number);

  // Print Settings.
  printing::PrintSettings* settings_;

  // File descriptor to export to.
  int fd_;
  // Print manager delegate
  PrintManagerDelegate* delegate_;
  // Number of pages to print in the print job.
  int number_pages_;
  // The document cookie of the current PrinterQuery.
  int cookie_;
  // Whether a print task is pending.
  int printing_;

  DISALLOW_COPY_AND_ASSIGN(PrintManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_RENDER_HOST_PRINT_MANAGER_H_
