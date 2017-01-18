// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_
#define CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/shared_memory.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_test_sink.h"
#include "ipc/message_filter.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "third_party/WebKit/public/web/WebPopupType.h"

struct FrameHostMsg_CreateChildFrame_Params;

namespace IPC {
class MessageFilter;
class MessageReplyDeserializer;
}

namespace blink {
enum class WebSandboxFlags;
enum class WebTreeScopeType;
}

namespace content {

namespace mojom {
class CreateNewWindowParams;
class CreateNewWindowReply;
class RenderMessageFilter;
}

// This class is a very simple mock of RenderThread. It simulates an IPC channel
// which supports only three messages:
// ViewHostMsg_CreateWidget : sync message sent by the Widget.
// ViewMsg_Close : async, send to the Widget.
class MockRenderThread : public RenderThread {
 public:
  MockRenderThread();
  ~MockRenderThread() override;

  // Provides access to the messages that have been received by this thread.
  IPC::TestSink& sink() { return sink_; }

  // RenderThread implementation:
  bool Send(IPC::Message* msg) override;
  IPC::SyncChannel* GetChannel() override;
  std::string GetLocale() override;
  IPC::SyncMessageFilter* GetSyncMessageFilter() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  int GenerateRoutingID() override;
  void AddFilter(IPC::MessageFilter* filter) override;
  void RemoveFilter(IPC::MessageFilter* filter) override;
  void AddObserver(RenderThreadObserver* observer) override;
  void RemoveObserver(RenderThreadObserver* observer) override;
  void SetResourceDispatcherDelegate(
      ResourceDispatcherDelegate* delegate) override;
  void RecordAction(const base::UserMetricsAction& action) override;
  void RecordComputedAction(const std::string& action) override;
  std::unique_ptr<base::SharedMemory> HostAllocateSharedMemoryBuffer(
      size_t buffer_size) override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  void RegisterExtension(v8::Extension* extension) override;
  void ScheduleIdleHandler(int64_t initial_delay_ms) override;
  void IdleHandler() override;
  int64_t GetIdleNotificationDelayInMs() const override;
  void SetIdleNotificationDelayInMs(
      int64_t idle_notification_delay_in_ms) override;
  int PostTaskToAllWebWorkers(const base::Closure& closure) override;
  bool ResolveProxy(const GURL& url, std::string* proxy_list) override;
  base::WaitableEvent* GetShutdownEvent() override;
  int32_t GetClientId() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetLoadingTaskRunner() override;
#if defined(OS_WIN)
  void PreCacheFont(const LOGFONT& log_font) override;
  void ReleaseCachedFonts() override;
#endif
  ServiceManagerConnection* GetServiceManagerConnection() override;
  service_manager::InterfaceRegistry* GetInterfaceRegistry() override;
  service_manager::InterfaceProvider* GetRemoteInterfaces() override;

  //////////////////////////////////////////////////////////////////////////
  // The following functions are called by the test itself.

  void set_routing_id(int32_t id) { routing_id_ = id; }

  int32_t opener_id() const { return opener_id_; }

  void set_new_window_routing_id(int32_t id) { new_window_routing_id_ = id; }

  void set_new_window_main_frame_widget_routing_id(int32_t id) {
    new_window_main_frame_widget_routing_id_ = id;
  }

  void set_new_frame_routing_id(int32_t id) { new_frame_routing_id_ = id; }

  // Simulates the Widget receiving a close message. This should result
  // on releasing the internal reference counts and destroying the internal
  // state.
  void SendCloseMessage();

  // Dispatches control messages to observers.
  bool OnControlMessageReceived(const IPC::Message& msg);

  base::ObserverList<RenderThreadObserver>& observers() { return observers_; }

  // The View expects to be returned a valid |reply.route_id| different from its
  // own. We do not keep track of the newly created widget in MockRenderThread,
  // so it must be cleaned up on its own.
  void OnCreateWindow(const mojom::CreateNewWindowParams& params,
                      mojom::CreateNewWindowReply* reply);

  // The Widget expects to be returned a valid route_id.
  void OnCreateWidget(int opener_id,
                      blink::WebPopupType popup_type,
                      int* route_id);
 protected:
  // This function operates as a regular IPC listener. Subclasses
  // overriding this should first delegate to this implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // The Frame expects to be returned a valid route_id different from its own.
  void OnCreateChildFrame(const FrameHostMsg_CreateChildFrame_Params& params,
                          int* new_render_frame_id);

#if defined(OS_WIN)
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
#endif

  IPC::TestSink sink_;

  // Routing id what will be assigned to the Widget.
  int32_t routing_id_;

  // Opener id reported by the Widget.
  int32_t opener_id_;

  // Routing id that will be assigned to a CreateWindow Widget.
  int32_t new_window_routing_id_;
  int32_t new_window_main_frame_routing_id_;
  int32_t new_window_main_frame_widget_routing_id_;
  int32_t new_frame_routing_id_;

  // The last known good deserializer for sync messages.
  std::unique_ptr<IPC::MessageReplyDeserializer> reply_deserializer_;

  // A list of message filters added to this thread.
  std::vector<scoped_refptr<IPC::MessageFilter> > filters_;

  // Observers to notify.
  base::ObserverList<RenderThreadObserver> observers_;

  cc::TestSharedBitmapManager shared_bitmap_manager_;
  std::unique_ptr<service_manager::InterfaceRegistry> interface_registry_;
  std::unique_ptr<service_manager::InterfaceProvider> remote_interfaces_;
  service_manager::mojom::InterfaceProviderRequest
      pending_remote_interface_provider_request_;

  std::unique_ptr<mojom::RenderMessageFilter> mock_render_message_filter_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_RENDER_THREAD_H_
