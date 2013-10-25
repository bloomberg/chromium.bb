// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_MOCK_RENDER_THREAD_H_
#define CHROME_RENDERER_CHROME_MOCK_RENDER_THREAD_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/public/test/mock_render_thread.h"

namespace base {
class DictionaryValue;
}

class MockPrinter;
struct ExtensionMsg_ExternalConnectionInfo;
struct PrintHostMsg_DidGetPreviewPageCount_Params;
struct PrintHostMsg_DidPreviewPage_Params;
struct PrintHostMsg_DidPrintPage_Params;
struct PrintHostMsg_ScriptedPrint_Params;
struct PrintMsg_PrintPages_Params;
struct PrintMsg_Print_Params;

// Extends content::MockRenderThread to know about printing and
// extension messages.
class ChromeMockRenderThread : public content::MockRenderThread {
 public:
  ChromeMockRenderThread();
  virtual ~ChromeMockRenderThread();

  // content::RenderThread overrides.
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy()
      OVERRIDE;

  //////////////////////////////////////////////////////////////////////////
  // The following functions are called by the test itself.

  // Set IO message loop proxy.
  void set_io_message_loop_proxy(
      const scoped_refptr<base::MessageLoopProxy>& proxy);

#if defined(ENABLE_PRINTING)
  // Returns the pseudo-printer instance.
  MockPrinter* printer();

  // Call with |response| set to true if the user wants to print.
  // False if the user decides to cancel.
  void set_print_dialog_user_response(bool response);

  // Cancel print preview when print preview has |page| remaining pages.
  void set_print_preview_cancel_page_number(int page);

  // Get the number of pages to generate for print preview.
  int print_preview_pages_remaining() const;
#endif

 private:
  // Overrides base class implementation to add custom handling for
  // print and extensions.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // The callee expects to be returned a valid channel_id.
  void OnOpenChannelToExtension(int routing_id,
                                const ExtensionMsg_ExternalConnectionInfo& info,
                                const std::string& channel_name,
                                bool include_tls_channel_id,
                                int* port_id);

#if defined(ENABLE_PRINTING)
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  void OnAllocateTempFileForPrinting(int render_view_id,
                                     base::FileDescriptor* renderer_fd,
                                     int* browser_fd);
  void OnTempFileForPrintingWritten(int render_view_id, int browser_fd);
#endif

  // PrintWebViewHelper expects default print settings.
  void OnGetDefaultPrintSettings(PrintMsg_Print_Params* setting);

  // PrintWebViewHelper expects final print settings from the user.
  void OnScriptedPrint(const PrintHostMsg_ScriptedPrint_Params& params,
                       PrintMsg_PrintPages_Params* settings);

  void OnDidGetPrintedPagesCount(int cookie, int number_pages);
  void OnDidPrintPage(const PrintHostMsg_DidPrintPage_Params& params);
  void OnDidGetPreviewPageCount(
      const PrintHostMsg_DidGetPreviewPageCount_Params& params);
  void OnDidPreviewPage(const PrintHostMsg_DidPreviewPage_Params& params);
  void OnCheckForCancel(int32 preview_ui_id,
                        int preview_request_id,
                        bool* cancel);


  // For print preview, PrintWebViewHelper will update settings.
  void OnUpdatePrintSettings(int document_cookie,
                             const base::DictionaryValue& job_settings,
                             PrintMsg_PrintPages_Params* params);

  // A mock printer device used for printing tests.
  scoped_ptr<MockPrinter> printer_;

  // True to simulate user clicking print. False to cancel.
  bool print_dialog_user_response_;

  // Simulates cancelling print preview if |print_preview_pages_remaining_|
  // equals this.
  int print_preview_cancel_page_number_;

  // Number of pages to generate for print preview.
  int print_preview_pages_remaining_;
#endif

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMockRenderThread);
};

#endif  // CHROME_RENDERER_CHROME_MOCK_RENDER_THREAD_H_
