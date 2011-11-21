// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_
#define CHROME_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_
#pragma once

#include <string>

#include "content/browser/browser_message_filter.h"

#if defined(OS_WIN)
#include "base/shared_memory.h"
#endif

struct PrintHostMsg_ScriptedPrint_Params;

namespace base {
class DictionaryValue;
}

namespace printing {
class PrinterQuery;
class PrintJobManager;
}

// This class filters out incoming printing related IPC messages for the
// renderer process on the IPC thread.
class PrintingMessageFilter : public BrowserMessageFilter {
 public:
  PrintingMessageFilter();

  // BrowserMessageFilter methods.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  virtual ~PrintingMessageFilter();

#if defined(OS_WIN)
  // Used to pass resulting EMF from renderer to browser in printing.
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
#endif

#if defined(OS_CHROMEOS)
  // Used to ask the browser allocate a temporary file for the renderer
  // to fill in resulting PDF in renderer.
  void OnAllocateTempFileForPrinting(base::FileDescriptor* temp_file_fd,
                                     int* sequence_number);
  void OnTempFileForPrintingWritten(int sequence_number);
#endif

  // Get the default print setting. The task is handled by the print
  // worker thread and the UI thread. The reply occurs on the IO thread.
  void OnGetDefaultPrintSettings(IPC::Message* reply_msg);
  void OnGetDefaultPrintSettingsReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);

  // The renderer host have to show to the user the print dialog and returns
  // the selected print settings. The task is handled by the print worker
  // thread and the UI thread. The reply occurs on the IO thread.
  void OnScriptedPrint(const PrintHostMsg_ScriptedPrint_Params& params,
                       IPC::Message* reply_msg);
  void OnScriptedPrintReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);

  // Modify the current print settings based on |job_settings|. The task is
  // handled by the print worker thread and the UI thread. The reply occurs on
  // the IO thread.
  void OnUpdatePrintSettings(int document_cookie,
                             const base::DictionaryValue& job_settings,
                             IPC::Message* reply_msg);
  void OnUpdatePrintSettingsReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);

  // Check to see if print preview has been cancelled.
  void OnCheckForCancel(const std::string& preview_ui_addr,
                        int preview_request_id,
                        bool* cancel);

  printing::PrintJobManager* print_job_manager_;

  DISALLOW_COPY_AND_ASSIGN(PrintingMessageFilter);
};

#endif  // CHROME_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_
