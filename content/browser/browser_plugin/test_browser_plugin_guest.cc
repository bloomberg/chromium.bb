// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_browser_plugin_guest.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"

namespace content {

class BrowserPluginGuest;

TestBrowserPluginGuest::TestBrowserPluginGuest(
    int instance_id,
    WebContentsImpl* web_contents)
    : BrowserPluginGuest(instance_id, false, web_contents),
      update_rect_count_(0),
      input_observed_(false),
      load_stop_observed_(false),
      ime_cancel_observed_(false) {
}

TestBrowserPluginGuest::~TestBrowserPluginGuest() {
}

WebContentsImpl* TestBrowserPluginGuest::web_contents() const {
  return static_cast<WebContentsImpl*>(BrowserPluginGuest::web_contents());
}

void TestBrowserPluginGuest::SendMessageToEmbedder(IPC::Message* msg) {
  if (msg->type() == BrowserPluginMsg_UpdateRect::ID) {
    update_rect_count_++;
    BrowserPluginMsg_UpdateRect::Param param;
    BrowserPluginMsg_UpdateRect::Read(msg, &param);
    BrowserPluginMsg_UpdateRect_Params params = param.b;
    if (send_message_loop_runner_)
      send_message_loop_runner_->Quit();
  }
  BrowserPluginGuest::SendMessageToEmbedder(msg);
}

void TestBrowserPluginGuest::WaitForUpdateRectMsg() {
  // Check if we already got any UpdateRect message.
  if (update_rect_count_ > 0)
    return;
  send_message_loop_runner_ = new MessageLoopRunner();
  send_message_loop_runner_->Run();
}

void TestBrowserPluginGuest::OnHandleInputEvent(
    int instance_id,
    const gfx::Rect& guest_window_rect,
    const blink::WebInputEvent* event) {
  BrowserPluginGuest::OnHandleInputEvent(instance_id,
                                         guest_window_rect,
                                         event);
  input_observed_ = true;
  if (input_message_loop_runner_)
    input_message_loop_runner_->Quit();
}

void TestBrowserPluginGuest::WaitForInput() {
  if (input_observed_) {
    input_observed_ = false;
    return;
  }

  input_message_loop_runner_ = new MessageLoopRunner();
  input_message_loop_runner_->Run();
  input_observed_ = false;
}

void TestBrowserPluginGuest::WaitForLoadStop() {
  if (load_stop_observed_) {
    load_stop_observed_ = false;
    return;
  }

  load_stop_message_loop_runner_ = new MessageLoopRunner();
  load_stop_message_loop_runner_->Run();
  load_stop_observed_ = false;
}

void TestBrowserPluginGuest::WaitForImeCancel() {
  if (ime_cancel_observed_) {
    ime_cancel_observed_ = false;
    return;
  }

  ime_cancel_message_loop_runner_ = new MessageLoopRunner();
  ime_cancel_message_loop_runner_->Run();
  ime_cancel_observed_ = false;
}

void TestBrowserPluginGuest::DidStopLoading(
    RenderViewHost* render_view_host) {
  BrowserPluginGuest::DidStopLoading(render_view_host);
  load_stop_observed_ = true;
  if (load_stop_message_loop_runner_)
    load_stop_message_loop_runner_->Quit();
}

void TestBrowserPluginGuest::OnImeCancelComposition() {
  if (!ime_cancel_observed_) {
    ime_cancel_observed_ = true;
    if (ime_cancel_message_loop_runner_)
      ime_cancel_message_loop_runner_->Quit();
  }
  BrowserPluginGuest::OnImeCancelComposition();
}

}  // namespace content
