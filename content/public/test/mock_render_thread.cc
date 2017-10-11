// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_render_thread.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/common/frame_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/render_thread_observer.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/message_filter.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebScriptController.h"

namespace content {

namespace {

class MockRenderMessageFilterImpl : public mojom::RenderMessageFilter {
 public:
  explicit MockRenderMessageFilterImpl(MockRenderThread* thread)
      : thread_(thread) {}
  ~MockRenderMessageFilterImpl() override {}

  // mojom::RenderMessageFilter:
  void GenerateRoutingID(GenerateRoutingIDCallback callback) override {
    NOTREACHED();
    std::move(callback).Run(MSG_ROUTING_NONE);
  }

  void CreateNewWidget(int32_t opener_id,
                       blink::WebPopupType popup_type,
                       mojom::WidgetPtr widget,
                       CreateNewWidgetCallback callback) override {
    // See comment in CreateNewWindow().
    NOTREACHED();
  }

  bool CreateNewWidget(int32_t opener_id,
                       blink::WebPopupType popup_type,
                       mojom::WidgetPtr widget,
                       int32_t* route_id) override {
    thread_->OnCreateWidget(opener_id, popup_type, route_id);
    return true;
  }

  void CreateFullscreenWidget(
      int opener_id,
      mojom::WidgetPtr widget,
      CreateFullscreenWidgetCallback callback) override {
    NOTREACHED();
  }

 private:
  MockRenderThread* const thread_;
};

}  // namespace

MockRenderThread::MockRenderThread()
    : routing_id_(0),
      opener_id_(0),
      new_window_routing_id_(0),
      new_window_main_frame_routing_id_(0),
      new_window_main_frame_widget_routing_id_(0),
      new_frame_routing_id_(0),
      mock_render_message_filter_(new MockRenderMessageFilterImpl(this)) {
  RenderThreadImpl::SetRenderMessageFilterForTesting(
      mock_render_message_filter_.get());
}

MockRenderThread::~MockRenderThread() {
  while (!filters_.empty()) {
    scoped_refptr<IPC::MessageFilter> filter = filters_.back();
    filters_.pop_back();
    filter->OnFilterRemoved();
  }
}

// Called by the Widget. Used to send messages to the browser.
// We short-circuit the mechanism and handle the messages right here on this
// class.
bool MockRenderThread::Send(IPC::Message* msg) {
  // We need to simulate a synchronous channel, thus we are going to receive
  // through this function messages, messages with reply and reply messages.
  // We can only handle one synchronous message at a time.
  if (msg->is_reply()) {
    if (reply_deserializer_) {
      reply_deserializer_->SerializeOutputParameters(*msg);
      reply_deserializer_.reset();
    }
  } else {
    if (msg->is_sync()) {
      // We actually need to handle deleting the reply deserializer for sync
      // messages.
      reply_deserializer_.reset(
          static_cast<IPC::SyncMessage*>(msg)->GetReplyDeserializer());
    }
    if (msg->routing_id() == MSG_ROUTING_CONTROL)
      OnControlMessageReceived(*msg);
    else
      OnMessageReceived(*msg);
  }
  delete msg;
  return true;
}

IPC::SyncChannel* MockRenderThread::GetChannel() {
  return NULL;
}

std::string MockRenderThread::GetLocale() {
  return "en-US";
}

IPC::SyncMessageFilter* MockRenderThread::GetSyncMessageFilter() {
  return NULL;
}

scoped_refptr<base::SingleThreadTaskRunner>
MockRenderThread::GetIOTaskRunner() {
  return scoped_refptr<base::SingleThreadTaskRunner>();
}

void MockRenderThread::AddRoute(int32_t routing_id, IPC::Listener* listener) {}

void MockRenderThread::RemoveRoute(int32_t routing_id) {}

int MockRenderThread::GenerateRoutingID() {
  NOTREACHED();
  return MSG_ROUTING_NONE;
}

void MockRenderThread::AddFilter(IPC::MessageFilter* filter) {
  filter->OnFilterAdded(&sink());
  // Add this filter to a vector so the MockRenderThread::RemoveFilter function
  // can check if this filter is added.
  filters_.push_back(base::WrapRefCounted(filter));
}

void MockRenderThread::RemoveFilter(IPC::MessageFilter* filter) {
  // Emulate the IPC::ChannelProxy::OnRemoveFilter function.
  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i].get() == filter) {
      filter->OnFilterRemoved();
      filters_.erase(filters_.begin() + i);
      return;
    }
  }
  NOTREACHED() << "filter to be removed not found";
}

void MockRenderThread::AddObserver(RenderThreadObserver* observer) {
  observers_.AddObserver(observer);
}

void MockRenderThread::RemoveObserver(RenderThreadObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MockRenderThread::SetResourceDispatcherDelegate(
    ResourceDispatcherDelegate* delegate) {
}

void MockRenderThread::RecordAction(const base::UserMetricsAction& action) {
}

void MockRenderThread::RecordComputedAction(const std::string& action) {
}

std::unique_ptr<base::SharedMemory>
MockRenderThread::HostAllocateSharedMemoryBuffer(size_t buffer_size) {
  std::unique_ptr<base::SharedMemory> shared_buf(new base::SharedMemory);
  if (!shared_buf->CreateAnonymous(buffer_size)) {
    NOTREACHED() << "Cannot map shared memory buffer";
    return std::unique_ptr<base::SharedMemory>();
  }

  return std::unique_ptr<base::SharedMemory>(shared_buf.release());
}

viz::SharedBitmapManager* MockRenderThread::GetSharedBitmapManager() {
  return &shared_bitmap_manager_;
}

void MockRenderThread::RegisterExtension(v8::Extension* extension) {
  blink::WebScriptController::RegisterExtension(extension);
}

void MockRenderThread::ScheduleIdleHandler(int64_t initial_delay_ms) {}

void MockRenderThread::IdleHandler() {
}

int64_t MockRenderThread::GetIdleNotificationDelayInMs() const {
  return 0;
}

void MockRenderThread::SetIdleNotificationDelayInMs(
    int64_t idle_notification_delay_in_ms) {}

int MockRenderThread::PostTaskToAllWebWorkers(const base::Closure& closure) {
  return 0;
}

bool MockRenderThread::ResolveProxy(const GURL& url, std::string* proxy_list) {
  return false;
}

base::WaitableEvent* MockRenderThread::GetShutdownEvent() {
  return NULL;
}

int32_t MockRenderThread::GetClientId() {
  return 1;
}

void MockRenderThread::SetRendererProcessType(
    blink::scheduler::RendererProcessType type) {}

#if defined(OS_WIN)
void MockRenderThread::PreCacheFont(const LOGFONT& log_font) {
}

void MockRenderThread::ReleaseCachedFonts() {
}

#endif  // OS_WIN

ServiceManagerConnection* MockRenderThread::GetServiceManagerConnection() {
  return nullptr;
}

service_manager::Connector* MockRenderThread::GetConnector() {
  if (!connector_) {
    connector_ =
        service_manager::Connector::Create(&pending_connector_request_);
  }
  return connector_.get();
}

void MockRenderThread::SetFieldTrialGroup(const std::string& trial_name,
                                          const std::string& group_name) {}
void MockRenderThread::SendCloseMessage() {
  ViewMsg_Close msg(routing_id_);
  RenderViewImpl::FromRoutingID(routing_id_)->OnMessageReceived(msg);
}

// The Widget expects to be returned a valid route_id.
void MockRenderThread::OnCreateWidget(int opener_id,
                                      blink::WebPopupType popup_type,
                                      int* route_id) {
  opener_id_ = opener_id;
  *route_id = routing_id_;
}

// The Frame expects to be returned a valid route_id different from its own.
void MockRenderThread::OnCreateChildFrame(
    const FrameHostMsg_CreateChildFrame_Params& params,
    int* new_render_frame_id,
    base::UnguessableToken* devtools_frame_token) {
  *new_render_frame_id = new_frame_routing_id_++;
  *devtools_frame_token = base::UnguessableToken::Create();
}

bool MockRenderThread::OnControlMessageReceived(const IPC::Message& msg) {
  for (auto& observer : observers_) {
    if (observer.OnControlMessageReceived(msg))
      return true;
  }
  return OnMessageReceived(msg);
}

bool MockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(msg);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MockRenderThread, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CreateChildFrame, OnCreateChildFrame)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if defined(OS_WIN)
void MockRenderThread::OnDuplicateSection(
    base::SharedMemoryHandle renderer_handle,
    base::SharedMemoryHandle* browser_handle) {
  // We don't have to duplicate the input handles since RenderViewTest does not
  // separate a browser process from a renderer process.
  *browser_handle = renderer_handle;
}
#endif  // defined(OS_WIN)

// The View expects to be returned a valid route_id different from its own.
void MockRenderThread::OnCreateWindow(
    const mojom::CreateNewWindowParams& params,
    mojom::CreateNewWindowReply* reply) {
  reply->route_id = new_window_routing_id_;
  reply->main_frame_route_id = new_window_main_frame_routing_id_;
  reply->main_frame_widget_route_id = new_window_main_frame_widget_routing_id_;
  reply->cloned_session_storage_namespace_id = 0;
}

}  // namespace content
