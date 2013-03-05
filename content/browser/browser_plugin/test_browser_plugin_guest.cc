// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_browser_plugin_guest.h"

#include "base/test/test_timeouts.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/public/browser/notification_types.h"

namespace content {

class BrowserPluginGuest;

TestBrowserPluginGuest::TestBrowserPluginGuest(
    int instance_id,
    WebContentsImpl* embedder_web_contents,
    WebContentsImpl* web_contents,
    const BrowserPluginHostMsg_CreateGuest_Params& params)
    : BrowserPluginGuest(instance_id,
                         embedder_web_contents,
                         web_contents,
                         params),
      update_rect_count_(0),
      damage_buffer_call_count_(0),
      exit_observed_(false),
      focus_observed_(false),
      blur_observed_(false),
      advance_focus_observed_(false),
      was_hidden_observed_(false),
      stop_observed_(false),
      reload_observed_(false),
      set_damage_buffer_observed_(false),
      input_observed_(false),
      load_stop_observed_(false),
      waiting_for_damage_buffer_with_size_(false),
      last_damage_buffer_size_(gfx::Size()) {
  // Listen to visibility changes so that a test can wait for these changes.
  notification_registrar_.Add(this,
                              NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
                              Source<WebContents>(web_contents));
}

TestBrowserPluginGuest::~TestBrowserPluginGuest() {
}

WebContentsImpl* TestBrowserPluginGuest::web_contents() const {
  return static_cast<WebContentsImpl*>(BrowserPluginGuest::web_contents());
}

void TestBrowserPluginGuest::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED: {
      bool visible = *Details<bool>(details).ptr();
      if (!visible) {
        was_hidden_observed_ = true;
        if (was_hidden_message_loop_runner_)
          was_hidden_message_loop_runner_->Quit();
      }
      return;
    }
  }

  BrowserPluginGuest::Observe(type, source, details);
}

void TestBrowserPluginGuest::SendMessageToEmbedder(IPC::Message* msg) {
  if (msg->type() == BrowserPluginMsg_UpdateRect::ID) {
    update_rect_count_++;
    int instance_id = 0;
    BrowserPluginMsg_UpdateRect_Params params;
    BrowserPluginMsg_UpdateRect::Read(msg, &instance_id, &params);
    last_view_size_observed_ = params.view_size;
    if (!expected_auto_view_size_.IsEmpty() &&
        expected_auto_view_size_ == params.view_size) {
      if (auto_view_size_message_loop_runner_)
        auto_view_size_message_loop_runner_->Quit();
    }
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

void TestBrowserPluginGuest::ResetUpdateRectCount() {
  update_rect_count_ = 0;
}

void TestBrowserPluginGuest::WaitForDamageBufferWithSize(
    const gfx::Size& size) {
  if (damage_buffer_call_count_ > 0 && last_damage_buffer_size_ == size)
    return;

  expected_damage_buffer_size_ = size;
  waiting_for_damage_buffer_with_size_ = true;
  damage_buffer_message_loop_runner_ = new MessageLoopRunner();
  damage_buffer_message_loop_runner_->Run();
}

void TestBrowserPluginGuest::RenderViewGone(base::TerminationStatus status) {
  exit_observed_ = true;
  LOG(INFO) << "Guest crashed";
  if (crash_message_loop_runner_)
    crash_message_loop_runner_->Quit();
  BrowserPluginGuest::RenderViewGone(status);
}

void TestBrowserPluginGuest::OnHandleInputEvent(
    int instance_id,
    const gfx::Rect& guest_window_rect,
    const WebKit::WebInputEvent* event) {
  BrowserPluginGuest::OnHandleInputEvent(instance_id,
                                         guest_window_rect,
                                         event);
  input_observed_ = true;
  if (input_message_loop_runner_)
    input_message_loop_runner_->Quit();
}

void TestBrowserPluginGuest::WaitForExit() {
  // Check if we already observed a guest crash, return immediately if so.
  if (exit_observed_)
    return;

  crash_message_loop_runner_ = new MessageLoopRunner();
  crash_message_loop_runner_->Run();
}

void TestBrowserPluginGuest::WaitForFocus() {
  if (focus_observed_) {
    focus_observed_ = false;
    return;
  }
  focus_message_loop_runner_ = new MessageLoopRunner();
  focus_message_loop_runner_->Run();
  focus_observed_ = false;
}

void TestBrowserPluginGuest::WaitForBlur() {
  if (blur_observed_) {
    blur_observed_ = false;
    return;
  }
  blur_message_loop_runner_ = new MessageLoopRunner();
  blur_message_loop_runner_->Run();
  blur_observed_ = false;
}

void TestBrowserPluginGuest::WaitForAdvanceFocus() {
  if (advance_focus_observed_)
    return;
  advance_focus_message_loop_runner_ = new MessageLoopRunner();
  advance_focus_message_loop_runner_->Run();
}

void TestBrowserPluginGuest::WaitUntilHidden() {
  if (was_hidden_observed_) {
    was_hidden_observed_ = false;
    return;
  }
  was_hidden_message_loop_runner_ = new MessageLoopRunner();
  was_hidden_message_loop_runner_->Run();
  was_hidden_observed_ = false;
}

void TestBrowserPluginGuest::WaitForReload() {
  if (reload_observed_) {
    reload_observed_ = false;
    return;
  }

  reload_message_loop_runner_ = new MessageLoopRunner();
  reload_message_loop_runner_->Run();
  reload_observed_ = false;
}

void TestBrowserPluginGuest::WaitForStop() {
  if (stop_observed_) {
    stop_observed_ = false;
    return;
  }

  stop_message_loop_runner_ = new MessageLoopRunner();
  stop_message_loop_runner_->Run();
  stop_observed_ = false;
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

void TestBrowserPluginGuest::WaitForViewSize(const gfx::Size& view_size) {
  if (last_view_size_observed_ == view_size) {
    last_view_size_observed_ = gfx::Size();
    return;
  }

  expected_auto_view_size_ = view_size;
  auto_view_size_message_loop_runner_ = new MessageLoopRunner();
  auto_view_size_message_loop_runner_->Run();
  last_view_size_observed_ = gfx::Size();
}

void TestBrowserPluginGuest::OnSetFocus(int instance_id, bool focused) {
  if (focused) {
    focus_observed_ = true;
    if (focus_message_loop_runner_)
      focus_message_loop_runner_->Quit();
  } else {
    blur_observed_ = true;
    if (blur_message_loop_runner_)
      blur_message_loop_runner_->Quit();
  }
  BrowserPluginGuest::OnSetFocus(instance_id, focused);
}

void TestBrowserPluginGuest::OnTakeFocus(bool reverse) {
  advance_focus_observed_ = true;
  if (advance_focus_message_loop_runner_)
    advance_focus_message_loop_runner_->Quit();
  BrowserPluginGuest::OnTakeFocus(reverse);
}

void TestBrowserPluginGuest::OnReload(int instance_id) {
  reload_observed_ = true;
  if (reload_message_loop_runner_)
    reload_message_loop_runner_->Quit();
  BrowserPluginGuest::OnReload(instance_id);
}

void TestBrowserPluginGuest::OnStop(int instance_id) {
  stop_observed_ = true;
  if (stop_message_loop_runner_)
    stop_message_loop_runner_->Quit();
  BrowserPluginGuest::OnStop(instance_id);
}

void TestBrowserPluginGuest::SetDamageBuffer(
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  ++damage_buffer_call_count_;
  last_damage_buffer_size_ = params.view_size;

  if (waiting_for_damage_buffer_with_size_ &&
      expected_damage_buffer_size_ == params.view_size &&
      damage_buffer_message_loop_runner_) {
    damage_buffer_message_loop_runner_->Quit();
    waiting_for_damage_buffer_with_size_ = false;
  }

  BrowserPluginGuest::SetDamageBuffer(params);
}

void TestBrowserPluginGuest::DidStopLoading(
    RenderViewHost* render_view_host) {
  BrowserPluginGuest::DidStopLoading(render_view_host);
  load_stop_observed_ = true;
  if (load_stop_message_loop_runner_)
    load_stop_message_loop_runner_->Quit();
}

}  // namespace content
