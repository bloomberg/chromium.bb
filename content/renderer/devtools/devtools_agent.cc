// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/devtools_agent.h"

#include <stddef.h>

#include <map>
#include <utility>

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/common/devtools_messages.h"
#include "content/common/frame_messages.h"
#include "content/public/common/manifest.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebDevToolsAgent;
using blink::WebDevToolsAgentClient;
using blink::WebLocalFrame;
using blink::WebPoint;
using blink::WebString;

using base::trace_event::TraceLog;

namespace content {

namespace {

// TODO(dgozman): somehow get this from a mojo config.
// See kMaximumMojoMessageSize in services/service_manager/embedder/main.cc.
const size_t kMaxDevToolsMessageChunkSize = 128 * 1024 * 1024 / 4;

} //  namespace

class DevToolsAgent::MessageImpl : public WebDevToolsAgent::MessageDescriptor {
 public:
  MessageImpl(base::WeakPtr<DevToolsAgent> agent,
              const std::string& method,
              const std::string& message)
      : agent_(std::move(agent)), method_(method), msg_(message) {}
  ~MessageImpl() override {}

  WebDevToolsAgent* Agent() override {
    if (!agent_)
      return nullptr;
    return agent_->GetWebAgent();
  }
  WebString Message() override { return WebString::FromUTF8(msg_); }
  WebString Method() override { return WebString::FromUTF8(method_); }

 private:
  base::WeakPtr<DevToolsAgent> agent_;
  std::string method_;
  std::string msg_;

  DISALLOW_COPY_AND_ASSIGN(MessageImpl);
};

// Created and stored in unique_ptr on UI.
// Binds request, receives messages and destroys on IO.
class DevToolsAgent::IOSession : public mojom::DevToolsSession {
 public:
  IOSession(int session_id,
            base::SequencedTaskRunner* session_task_runner,
            scoped_refptr<base::SingleThreadTaskRunner> agent_task_runner,
            base::WeakPtr<DevToolsAgent> agent,
            mojom::DevToolsSessionRequest request)
      : session_id_(session_id),
        agent_task_runner_(agent_task_runner),
        agent_(std::move(agent)),
        binding_(this) {
    session_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&IOSession::BindInterface,
                                  base::Unretained(this), std::move(request)));
  }

  ~IOSession() override {}

  void BindInterface(mojom::DevToolsSessionRequest request) {
    binding_.Bind(std::move(request));
  }

  // mojom::DevToolsSession implementation.
  void DispatchProtocolMessage(int call_id,
                               const std::string& method,
                               const std::string& message) override {
    DCHECK(WebDevToolsAgent::ShouldInterruptForMethod(
        WebString::FromUTF8(method)));
    WebDevToolsAgent::InterruptAndDispatch(
        session_id_, new DevToolsAgent::MessageImpl(agent_, method, message));
    agent_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DevToolsAgent::DispatchOnInspectorBackend,
                              agent_, session_id_, call_id, method, message));
  }

  void InspectElement(const gfx::Point& point) override { NOTREACHED(); }

 private:
  int session_id_;
  scoped_refptr<base::SingleThreadTaskRunner> agent_task_runner_;
  base::WeakPtr<DevToolsAgent> agent_;
  mojo::Binding<mojom::DevToolsSession> binding_;

  DISALLOW_COPY_AND_ASSIGN(IOSession);
};

class DevToolsAgent::Session : public mojom::DevToolsSession {
 public:
  Session(int session_id,
          DevToolsAgent* agent,
          mojom::DevToolsSessionAssociatedRequest request)
      : session_id_(session_id),
        agent_(agent),
        binding_(this, std::move(request)) {}

  ~Session() override {}

  // mojom::DevToolsSession implementation.
  void DispatchProtocolMessage(int call_id,
                               const std::string& method,
                               const std::string& message) override {
    DCHECK(!WebDevToolsAgent::ShouldInterruptForMethod(
        WebString::FromUTF8(method)));
    agent_->DispatchOnInspectorBackend(session_id_, call_id, method, message);
  }

  void InspectElement(const gfx::Point& point) override {
    agent_->InspectElement(session_id_, point);
  }

 private:
  int session_id_;
  DevToolsAgent* agent_;
  mojo::AssociatedBinding<mojom::DevToolsSession> binding_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

DevToolsAgent::DevToolsAgent(RenderFrameImpl* frame)
    : RenderFrameObserver(frame),
      binding_(this),
      frame_(frame),
      weak_factory_(this) {
  frame_->GetWebFrame()->SetDevToolsAgentClient(this);
}

DevToolsAgent::~DevToolsAgent() {
}

void DevToolsAgent::OnDestruct() {
  delete this;
}

void DevToolsAgent::AttachDevToolsSession(
    mojom::DevToolsSessionHostAssociatedPtrInfo host,
    mojom::DevToolsSessionAssociatedRequest session,
    mojom::DevToolsSessionRequest io_session,
    const base::Optional<std::string>& reattach_state) {
  int session_id = ++last_session_id_;

  if (reattach_state.has_value()) {
    GetWebAgent()->Reattach(session_id,
                            WebString::FromUTF8(reattach_state.value()));
  } else {
    GetWebAgent()->Attach(session_id);
  }

  sessions_[session_id].reset(
      new Session(session_id, this, std::move(session)));

  base::SequencedTaskRunner* io_task_runner =
      ChildProcess::current()->io_task_runner();
  io_sessions_.emplace(
      session_id,
      std::unique_ptr<IOSession, base::OnTaskRunnerDeleter>(
          new IOSession(session_id, io_task_runner,
                        frame_->GetTaskRunner(blink::TaskType::kUnthrottled),
                        weak_factory_.GetWeakPtr(), std::move(io_session)),
          base::OnTaskRunnerDeleter(io_task_runner)));

  hosts_[session_id].Bind(std::move(host));
  hosts_[session_id].set_connection_error_handler(base::BindOnce(
      &DevToolsAgent::DetachSession, base::Unretained(this), session_id));
}

void DevToolsAgent::DetachSession(int session_id) {
  GetWebAgent()->Detach(session_id);
  io_sessions_.erase(session_id);
  sessions_.erase(session_id);
  hosts_.erase(session_id);
}

void DevToolsAgent::SendProtocolMessage(int session_id,
                                        int call_id,
                                        const blink::WebString& message,
                                        const blink::WebString& state_cookie) {
  SendChunkedProtocolMessage(session_id, call_id, message.Utf8(),
                             state_cookie.Utf8());
}

bool DevToolsAgent::RequestDevToolsForFrame(int session_id,
                                            blink::WebLocalFrame* webFrame) {
  RenderFrameImpl* frame = RenderFrameImpl::FromWebFrame(webFrame);
  if (!frame)
    return false;
  auto it = hosts_.find(session_id);
  if (it == hosts_.end())
    return false;
  it->second->RequestNewWindow(
      frame->GetRoutingID(),
      base::Bind(&DevToolsAgent::OnRequestNewWindowCompleted,
                 weak_factory_.GetWeakPtr(), session_id));
  return true;
}

void DevToolsAgent::SendChunkedProtocolMessage(int session_id,
                                               int call_id,
                                               std::string message,
                                               std::string post_state) {
  auto it = hosts_.find(session_id);
  if (it == hosts_.end())
    return;

  bool single_chunk = message.length() < kMaxDevToolsMessageChunkSize;
  for (size_t pos = 0; pos < message.length();
       pos += kMaxDevToolsMessageChunkSize) {
    mojom::DevToolsMessageChunkPtr chunk = mojom::DevToolsMessageChunk::New();
    chunk->is_first = pos == 0;
    chunk->message_size = chunk->is_first ? message.size() : 0;
    chunk->is_last = pos + kMaxDevToolsMessageChunkSize >= message.length();
    chunk->call_id = chunk->is_last ? call_id : 0;
    chunk->post_state = chunk->is_last ? std::move(post_state) : std::string();
    chunk->data = single_chunk
                      ? std::move(message)
                      : message.substr(pos, kMaxDevToolsMessageChunkSize);
    it->second->DispatchProtocolMessage(std::move(chunk));
  }
}

void DevToolsAgent::DispatchOnInspectorBackend(int session_id,
                                               int call_id,
                                               const std::string& method,
                                               const std::string& message) {
  TRACE_EVENT0("devtools", "DevToolsAgent::DispatchOnInspectorBackend");
  GetWebAgent()->DispatchOnInspectorBackend(session_id, call_id,
                                            WebString::FromUTF8(method),
                                            WebString::FromUTF8(message));
}

void DevToolsAgent::InspectElement(int session_id, const gfx::Point& point) {
  blink::WebFloatRect point_rect(point.x(), point.y(), 0, 0);
  frame_->GetRenderWidget()->ConvertWindowToViewport(&point_rect);
  GetWebAgent()->InspectElementAt(session_id,
                                  WebPoint(point_rect.x, point_rect.y));
}

void DevToolsAgent::OnRequestNewWindowCompleted(int session_id, bool success) {
  if (!success)
    GetWebAgent()->FailedToRequestDevTools(session_id);
}

void DevToolsAgent::ContinueProgram() {
  GetWebAgent()->ContinueProgram();
}

WebDevToolsAgent* DevToolsAgent::GetWebAgent() {
  return frame_->GetWebFrame()->DevToolsAgent();
}

void DevToolsAgent::BindRequest(mojom::DevToolsAgentAssociatedRequest request) {
  binding_.Bind(std::move(request));
}

base::WeakPtr<DevToolsAgent> DevToolsAgent::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool DevToolsAgent::IsAttached() {
  return !hosts_.empty();
}

void DevToolsAgent::DetachAllSessions() {
  for (auto& it : hosts_)
    GetWebAgent()->Detach(it.first);
  hosts_.clear();
  io_sessions_.clear();
  sessions_.clear();
}

}  // namespace content
