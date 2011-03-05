// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_
#define CHROME_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_
#pragma once

#include "content/browser/browser_message_filter.h"

#if defined(OS_WIN)
#include "base/shared_memory.h"
#endif

class DictionaryValue;
struct ViewHostMsg_ScriptedPrint_Params;

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
  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread);
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

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

  // A javascript code requested to print the current page. This is done in two
  // steps and this is the first step. Get the print setting right here
  // synchronously. It will hang the I/O completely.
  void OnGetDefaultPrintSettings(IPC::Message* reply_msg);
  void OnGetDefaultPrintSettingsReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);

  // A javascript code requested to print the current page. The renderer host
  // have to show to the user the print dialog and returns the selected print
  // settings.
  void OnScriptedPrint(const ViewHostMsg_ScriptedPrint_Params& params,
                       IPC::Message* reply_msg);
  void OnScriptedPrintReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      int routing_id,
      IPC::Message* reply_msg);

  void OnUpdatePrintSettings(int document_cookie,
                             const DictionaryValue& job_settings,
                             IPC::Message* reply_msg);
  void OnUpdatePrintSettingsReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);

  printing::PrintJobManager* print_job_manager_;

  bool cloud_print_enabled_;

  DISALLOW_COPY_AND_ASSIGN(PrintingMessageFilter);
};

#endif  // CHROME_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_
