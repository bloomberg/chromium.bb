// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MOCK_RENDER_THREAD_H_
#define CHROME_RENDERER_MOCK_RENDER_THREAD_H_
#pragma once

#include <string>

#include "chrome/common/extensions/extension_set.h"
#include "chrome/renderer/mock_printer.h"
#include "content/renderer/render_thread.h"
#include "ipc/ipc_test_sink.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"

namespace IPC {
class MessageReplyDeserializer;
}

struct PrintHostMsg_DidGetPreviewPageCount_Params;
struct PrintHostMsg_DidPreviewPage_Params;
struct PrintHostMsg_ScriptedPrint_Params;
struct PrintMsg_PrintPages_Params;
struct PrintMsg_Print_Params;

// This class is very simple mock of RenderThread. It simulates an IPC channel
// which supports only two messages:
// ViewHostMsg_CreateWidget : sync message sent by the Widget.
// ViewMsg_Close : async, send to the Widget.
class MockRenderThread : public RenderThreadBase {
 public:
  MockRenderThread();
  virtual ~MockRenderThread();

  // Provides access to the messages that have been received by this thread.
  IPC::TestSink& sink() { return sink_; }

  // Called by the Widget. The routing_id must match the routing id assigned
  // to the Widget in reply to ViewHostMsg_CreateWidget message.
  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);

  // Called by the Widget. The routing id must match the routing id of AddRoute.
  virtual void RemoveRoute(int32 routing_id);

  // Called by the Widget. Used to send messages to the browser.
  // We short-circuit the mechanim and handle the messages right here on this
  // class.
  virtual bool Send(IPC::Message* msg);

  // Filtering support.
  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter);
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter);

  // Our mock thread doesn't deal with hidden and restored tabs.
  virtual void WidgetHidden() { }
  virtual void WidgetRestored() { }

  virtual bool IsIncognitoProcess() const;

  //////////////////////////////////////////////////////////////////////////
  // The following functions are called by the test itself.

  void set_routing_id(int32 id) {
    routing_id_ = id;
  }

  int32 opener_id() const {
    return opener_id_;
  }

  bool has_widget() const {
    return widget_ ? true : false;
  }

  // Simulates the Widget receiving a close message. This should result
  // on releasing the internal reference counts and destroying the internal
  // state.
  void SendCloseMessage();

  // Returns the pseudo-printer instance.
  MockPrinter* printer() const { return printer_.get(); }

  // Call with |response| set to true if the user wants to print.
  // False if the user decides to cancel.
  void set_print_dialog_user_response(bool response);

  // Cancel print preview when print preview has |page| remaining pages.
  void set_print_preview_cancel_page_number(int page);

  // Get the number of pages to generate for print preview.
  int print_preview_pages_remaining();

 private:
  // This function operates as a regular IPC listener.
  bool OnMessageReceived(const IPC::Message& msg);

  // The Widget expects to be returned valid route_id.
  void OnMsgCreateWidget(int opener_id,
                         WebKit::WebPopupType popup_type,
                         int* route_id);

  // The callee expects to be returned a valid channel_id.
  void OnMsgOpenChannelToExtension(
      int routing_id, const std::string& extension_id,
      const std::string& source_extension_id,
      const std::string& target_extension_id, int* port_id);

#if defined(OS_WIN)
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
#endif

  void OnAllocateSharedMemoryBuffer(uint32 buffer_size,
                                    base::SharedMemoryHandle* handle);

#if defined(OS_CHROMEOS)
  void OnAllocateTempFileForPrinting(base::FileDescriptor* renderer_fd,
                                     int* browser_fd);
  void OnTempFileForPrintingWritten(int browser_fd);
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
  void OnCheckForCancel(const std::string& preview_ui_addr,
                        int preview_request_id,
                        bool* cancel);


  // For print preview, PrintWebViewHelper will update settings.
  void OnUpdatePrintSettings(int document_cookie,
                             const DictionaryValue& job_settings,
                             PrintMsg_PrintPages_Params* params);

  IPC::TestSink sink_;

  // Routing id what will be assigned to the Widget.
  int32 routing_id_;

  // Opener id reported by the Widget.
  int32 opener_id_;

  // We only keep track of one Widget, we learn its pointer when it
  // adds a new route.
  IPC::Channel::Listener* widget_;

  // The last known good deserializer for sync messages.
  scoped_ptr<IPC::MessageReplyDeserializer> reply_deserializer_;

  // A mock printer device used for printing tests.
  scoped_ptr<MockPrinter> printer_;

  // True to simulate user clicking print. False to cancel.
  bool print_dialog_user_response_;

  // Simulates cancelling print preview if |print_preview_pages_remaining_|
  // equals this.
  int print_preview_cancel_page_number_;

  // Number of pages to generate for print preview.
  int print_preview_pages_remaining_;
};

#endif  // CHROME_RENDERER_MOCK_RENDER_THREAD_H_
