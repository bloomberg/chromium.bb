// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_THREAD_IMPL_H_
#define CONTENT_RENDERER_RENDER_THREAD_IMPL_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/common/child_thread.h"
#include "content/common/content_export.h"
#include "content/common/css_colors.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/gfx/native_widget_types.h"

class AppCacheDispatcher;
class AudioInputMessageFilter;
class AudioMessageFilter;
class CompositorThread;
class DBMessageFilter;
class DevToolsAgentFilter;
class DomStorageDispatcher;
class GpuChannelHost;
class IndexedDBDispatcher;
class RendererWebKitPlatformSupportImpl;
class SkBitmap;
class VideoCaptureImplManager;
struct ViewMsg_New_Params;
class WebDatabaseObserverImpl;

namespace WebKit {
class WebMediaStreamCenter;
class WebMediaStreamCenterClient;
}

namespace base {
class MessageLoopProxy;
class Thread;
namespace win {
class ScopedCOMInitializer;
}
}

namespace content {
class MediaStreamCenter;
class RenderProcessObserver;
}

namespace v8 {
class Extension;
}

// The RenderThreadImpl class represents a background thread where RenderView
// instances live.  The RenderThread supports an API that is used by its
// consumer to talk indirectly to the RenderViews and supporting objects.
// Likewise, it provides an API for the RenderViews to talk back to the main
// process (i.e., their corresponding WebContentsImpl).
//
// Most of the communication occurs in the form of IPC messages.  They are
// routed to the RenderThread according to the routing IDs of the messages.
// The routing IDs correspond to RenderView instances.
class CONTENT_EXPORT RenderThreadImpl : public content::RenderThread,
                                        public ChildThread,
                                        public GpuChannelHostFactory {
 public:
  static RenderThreadImpl* current();

  RenderThreadImpl();
  // Constructor that's used when running in single process mode.
  explicit RenderThreadImpl(const std::string& channel_name);
  virtual ~RenderThreadImpl();

  // Returns the routing ID of the RenderWidget containing the current script
  // execution context (corresponding to WebFrame::frameForCurrentContext).
  static int32 RoutingIDForCurrentContext();

  // When initializing WebKit, ensure that any schemes needed for the content
  // module are registered properly.  Static to allow sharing with tests.
  static void RegisterSchemes();

  // content::RenderThread implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;
  virtual MessageLoop* GetMessageLoop() OVERRIDE;
  virtual IPC::SyncChannel* GetChannel() OVERRIDE;
  virtual std::string GetLocale() OVERRIDE;
  virtual IPC::SyncMessageFilter* GetSyncMessageFilter() OVERRIDE;
  virtual void AddRoute(int32 routing_id,
                        IPC::Channel::Listener* listener) OVERRIDE;
  virtual void RemoveRoute(int32 routing_id) OVERRIDE;
  virtual int GenerateRoutingID() OVERRIDE;
  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter) OVERRIDE;
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) OVERRIDE;
  virtual void SetOutgoingMessageFilter(
      IPC::ChannelProxy::OutgoingMessageFilter* filter) OVERRIDE;
  virtual void AddObserver(content::RenderProcessObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      content::RenderProcessObserver* observer) OVERRIDE;
  virtual void SetResourceDispatcherDelegate(
      content::ResourceDispatcherDelegate* delegate) OVERRIDE;
  virtual void WidgetHidden() OVERRIDE;
  virtual void WidgetRestored() OVERRIDE;
  virtual void EnsureWebKitInitialized() OVERRIDE;
  virtual void RecordUserMetrics(const std::string& action) OVERRIDE;
  virtual base::SharedMemoryHandle HostAllocateSharedMemoryBuffer(
      uint32 buffer_size) OVERRIDE;
  virtual void RegisterExtension(v8::Extension* extension) OVERRIDE;
  virtual bool IsRegisteredExtension(
      const std::string& v8_extension_name) const OVERRIDE;
  virtual void ScheduleIdleHandler(int64 initial_delay_ms) OVERRIDE;
  virtual void IdleHandler() OVERRIDE;
  virtual int64 GetIdleNotificationDelayInMs() const OVERRIDE;
  virtual void SetIdleNotificationDelayInMs(
      int64 idle_notification_delay_in_ms) OVERRIDE;
#if defined(OS_WIN)
  virtual void PreCacheFont(const LOGFONT& log_font) OVERRIDE;
  virtual void ReleaseCachedFonts() OVERRIDE;
#endif

  // content::ChildThread:
  virtual bool IsWebFrameValid(WebKit::WebFrame* frame) OVERRIDE;

  // GpuChannelHostFactory implementation:
  virtual bool IsMainThread() OVERRIDE;
  virtual bool IsIOThread() OVERRIDE;
  virtual MessageLoop* GetMainLoop() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy> GetIOLoopProxy() OVERRIDE;
  virtual base::WaitableEvent* GetShutDownEvent() OVERRIDE;
  virtual scoped_ptr<base::SharedMemory> AllocateSharedMemory(
      uint32 size) OVERRIDE;
  virtual int32 CreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params) OVERRIDE;

  // Synchronously establish a channel to the GPU plugin if not previously
  // established or if it has been lost (for example if the GPU plugin crashed).
  // If there is a pending asynchronous request, it will be completed by the
  // time this routine returns.
  virtual GpuChannelHost* EstablishGpuChannelSync(
      content::CauseForGpuLaunch) OVERRIDE;


  // These methods modify how the next message is sent.  Normally, when sending
  // a synchronous message that runs a nested message loop, we need to suspend
  // callbacks into WebKit.  This involves disabling timers and deferring
  // resource loads.  However, there are exceptions when we need to customize
  // the behavior.
  void DoNotSuspendWebKitSharedTimer();
  void DoNotNotifyWebKitOfModalLoop();

  // Will be NULL if threaded compositing has not been enabled.
  CompositorThread* compositor_thread() const {
    return compositor_thread_.get();
  }

  AppCacheDispatcher* appcache_dispatcher() const {
    return appcache_dispatcher_.get();
  }

  AudioInputMessageFilter* audio_input_message_filter() {
    return audio_input_message_filter_.get();
  }

  AudioMessageFilter* audio_message_filter() {
    return audio_message_filter_.get();
  }

  // Creates the embedder implementation of WebMediaStreamCenter.
  // The resulting object is owned by WebKit and deleted by WebKit at tear-down.
  WebKit::WebMediaStreamCenter* CreateMediaStreamCenter(
      WebKit::WebMediaStreamCenterClient* client);

  VideoCaptureImplManager* video_capture_impl_manager() const {
    return vc_manager_.get();
  }

  bool plugin_refresh_allowed() const { return plugin_refresh_allowed_; }

  // Get the GPU channel. Returns NULL if the channel is not established or
  // has been lost.
  GpuChannelHost* GetGpuChannel();

  // Returns a MessageLoopProxy instance corresponding to the message loop
  // of the thread on which file operations should be run. Must be called
  // on the renderer's main thread.
  scoped_refptr<base::MessageLoopProxy> GetFileThreadMessageLoopProxy();

  // Causes the idle handler to skip sending idle notifications
  // on the two next scheduled calls, so idle notifications are
  // not sent for at least one notification delay.
  void PostponeIdleNotification();

 private:
  virtual bool OnControlMessageReceived(const IPC::Message& msg) OVERRIDE;

  void Init();

  void OnSetZoomLevelForCurrentURL(const std::string& host, double zoom_level);
  void OnSetCSSColors(const std::vector<CSSColors::CSSColorMapping>& colors);
  void OnCreateNewView(const ViewMsg_New_Params& params);
  void OnTransferBitmap(const SkBitmap& bitmap, int resource_id);
  void OnPurgePluginListCache(bool reload_pages);
  void OnNetworkStateChanged(bool online);
  void OnGetAccessibilityTree();
  void OnTempCrashWithData(const GURL& data);

  void IdleHandlerInForegroundTab();

  // These objects live solely on the render thread.
  scoped_ptr<AppCacheDispatcher> appcache_dispatcher_;
  scoped_ptr<DomStorageDispatcher> dom_storage_dispatcher_;
  scoped_ptr<IndexedDBDispatcher> main_thread_indexed_db_dispatcher_;
  scoped_ptr<RendererWebKitPlatformSupportImpl> webkit_platform_support_;

  // Used on the render thread and deleted by WebKit at shutdown.
  content::MediaStreamCenter* media_stream_center_;

  // Used on the renderer and IPC threads.
  scoped_refptr<DBMessageFilter> db_message_filter_;
  scoped_refptr<AudioInputMessageFilter> audio_input_message_filter_;
  scoped_refptr<AudioMessageFilter> audio_message_filter_;
  scoped_refptr<DevToolsAgentFilter> devtools_agent_message_filter_;

  // Used on multiple threads.
  scoped_refptr<VideoCaptureImplManager> vc_manager_;

  // Used on multiple script execution context threads.
  scoped_ptr<WebDatabaseObserverImpl> web_database_observer_impl_;

  // Initialize COM when using plugins outside the sandbox (Windows only).
  scoped_ptr<base::win::ScopedCOMInitializer> initialize_com_;

  // If true, then a GetPlugins call is allowed to rescan the disk.
  bool plugin_refresh_allowed_;

  // The count of RenderWidgets running through this thread.
  int widget_count_;

  // The count of hidden RenderWidgets running through this thread.
  int hidden_widget_count_;

  // The current value of the idle notification timer delay.
  int64 idle_notification_delay_in_ms_;

  // The number of idle handler calls that skip sending idle notifications.
  int idle_notifications_to_skip_;

  bool suspend_webkit_shared_timer_;
  bool notify_webkit_of_modal_loop_;

  // Timer that periodically calls IdleHandler.
  base::RepeatingTimer<RenderThreadImpl> idle_timer_;

  // The channel from the renderer process to the GPU process.
  scoped_refptr<GpuChannelHost> gpu_channel_;

  // A lazily initiated thread on which file operations are run.
  scoped_ptr<base::Thread> file_thread_;

  // Map of registered v8 extensions. The key is the extension name.
  std::set<std::string> v8_extensions_;

  bool compositor_initialized_;
  scoped_ptr<CompositorThread> compositor_thread_;

  ObserverList<content::RenderProcessObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(RenderThreadImpl);
};

#endif  // CONTENT_RENDERER_RENDER_THREAD_IMPL_H_
