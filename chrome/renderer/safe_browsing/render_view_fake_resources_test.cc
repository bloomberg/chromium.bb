// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/render_view_fake_resources_test.h"

#include <string.h>

#include "base/command_line.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "chrome/common/dom_storage_common.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/resource_response.h"
#include "chrome/common/sandbox_init_wrapper.h"
#include "chrome/renderer/mock_render_process.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/renderer_main_platform_delegate.h"
#include "googleurl/src/gurl.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_status.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"

namespace safe_browsing {

const int32 RenderViewFakeResourcesTest::kViewId = 5;

RenderViewFakeResourcesTest::RenderViewFakeResourcesTest() {}
RenderViewFakeResourcesTest::~RenderViewFakeResourcesTest() {}

bool RenderViewFakeResourcesTest::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(RenderViewFakeResourcesTest, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestResource, OnRequestResource)
  IPC_END_MESSAGE_MAP()
  return true;
}

bool RenderViewFakeResourcesTest::Visit(RenderView* render_view) {
  view_ = render_view;
  return false;
}

void RenderViewFakeResourcesTest::SetUp() {
  // Set up the renderer.  This code is largely adapted from
  // render_view_test.cc and renderer_main.cc.  Note that we use a
  // MockRenderProcess (because we don't need to use IPC for painting),
  // but we use a real RenderThread so that we can use the ResourceDispatcher
  // to fetch network resources.  These are then served canned content
  // in OnRequestResource().
  sandbox_init_wrapper_.reset(new SandboxInitWrapper);
  command_line_.reset(new CommandLine(CommandLine::NO_PROGRAM));
  params_.reset(new MainFunctionParams(*command_line_,
                                       *sandbox_init_wrapper_, NULL));
  platform_.reset(new RendererMainPlatformDelegate(*params_));
  platform_->PlatformInitialize();

  static const char kThreadName[] = "RenderViewFakeResourcesTest";
  channel_.reset(new IPC::Channel(kThreadName,
                                  IPC::Channel::MODE_SERVER, this));
  ASSERT_TRUE(channel_->Connect());

  webkit_glue::SetJavaScriptFlags("--expose-gc");
  mock_process_.reset(new MockRenderProcess);
  render_thread_ = new RenderThread(kThreadName);
  mock_process_->set_main_thread(render_thread_);

  // Tell the renderer to create a view, then wait until it's ready.
  // We can't call View::Create() directly here or else we won't get
  // RenderProcess's lazy initialization of WebKit.
  view_ = NULL;
  ViewMsg_New_Params params;
  params.parent_window = 0;
  params.view_id = kViewId;
  params.session_storage_namespace_id = kInvalidSessionStorageNamespaceId;
  ASSERT_TRUE(channel_->Send(new ViewMsg_New(params)));
  message_loop_.Run();
}

void RenderViewFakeResourcesTest::TearDown() {
  // Try very hard to collect garbage before shutting down.
  GetMainFrame()->collectGarbage();
  GetMainFrame()->collectGarbage();

  ASSERT_TRUE(channel_->Send(new ViewMsg_Close(kViewId)));
  do {
    message_loop_.RunAllPending();
    view_ = NULL;
    RenderView::ForEach(this);
  } while (view_);

  mock_process_.reset();
  message_loop_.RunAllPending();
  platform_->PlatformUninitialize();
  platform_.reset();
  command_line_.reset();
  sandbox_init_wrapper_.reset();
}

WebKit::WebFrame* RenderViewFakeResourcesTest::GetMainFrame() {
  return view_->webview()->mainFrame();
}

void RenderViewFakeResourcesTest::LoadURL(const std::string& url) {
  GURL g_url(url);
  GetMainFrame()->loadRequest(WebKit::WebURLRequest(g_url));
  message_loop_.Run();
}

void RenderViewFakeResourcesTest::LoadURLWithPost(const std::string& url) {
  GURL g_url(url);
  WebKit::WebURLRequest request(g_url);
  request.setHTTPMethod(WebKit::WebString::fromUTF8("POST"));
  GetMainFrame()->loadRequest(request);
  message_loop_.Run();
}

void RenderViewFakeResourcesTest::GoBack() {
  WebKit::WebFrame* frame = GetMainFrame();
  frame->loadHistoryItem(frame->previousHistoryItem());
  message_loop_.Run();
}

void RenderViewFakeResourcesTest::OnDidStopLoading() {
  message_loop_.Quit();
}

void RenderViewFakeResourcesTest::OnRequestResource(
    const IPC::Message& message,
    int request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  std::string headers, body;
  std::map<std::string, std::string>::const_iterator it =
      responses_.find(request_data.url.spec());
  if (it == responses_.end()) {
    headers = "HTTP/1.1 404 Not Found\0Content-Type:text/html\0\0";
    body = "content not found";
  } else {
    headers = "HTTP/1.1 200 OK\0Content-Type:text/html\0\0";
    body = it->second;
  }

  ResourceResponseHead response_head;
  response_head.headers = new net::HttpResponseHeaders(headers);
  response_head.mime_type = "text/html";
  ASSERT_TRUE(channel_->Send(new ViewMsg_Resource_ReceivedResponse(
      message.routing_id(), request_id, response_head)));

  base::SharedMemory shared_memory;
  ASSERT_TRUE(shared_memory.CreateAndMapAnonymous(body.size()));
  memcpy(shared_memory.memory(), body.data(), body.size());

  base::SharedMemoryHandle handle;
  ASSERT_TRUE(shared_memory.GiveToProcess(base::Process::Current().handle(),
                                          &handle));
  ASSERT_TRUE(channel_->Send(new ViewMsg_Resource_DataReceived(
      message.routing_id(), request_id, handle, body.size())));

  ASSERT_TRUE(channel_->Send(new ViewMsg_Resource_RequestComplete(
      message.routing_id(),
      request_id,
      net::URLRequestStatus(),
      std::string(),
      base::Time())));
}

void RenderViewFakeResourcesTest::OnRenderViewReady() {
  // Grab a pointer to the new view using RenderViewVisitor.
  ASSERT_TRUE(!view_);
  RenderView::ForEach(this);
  ASSERT_TRUE(view_);
  message_loop_.Quit();
}

}  // namespace safe_browsing
