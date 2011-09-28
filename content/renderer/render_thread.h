// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_THREAD_H_
#define CONTENT_RENDERER_RENDER_THREAD_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/shared_memory.h"
#include "base/time.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "content/common/child_thread.h"
#include "content/common/content_export.h"
#include "content/common/css_colors.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/gfx/native_widget_types.h"

class AppCacheDispatcher;
class AudioInputMessageFilter;
class AudioMessageFilter;
class DBMessageFilter;
class DevToolsAgentFilter;
class FilePath;
class GpuChannelHost;
class IndexedDBDispatcher;
class RendererHistogram;
class RendererHistogramSnapshots;
class RenderProcessObserver;
class RendererNetPredictor;
class RendererWebKitPlatformSupportImpl;
class SkBitmap;
class VideoCaptureImplManager;
class WebDatabaseObserverImpl;

struct RendererPreferences;
struct DOMStorageMsg_Event_Params;
struct GPUInfo;
struct ViewMsg_New_Params;
struct WebPreferences;

namespace base {
class MessageLoopProxy;
class Thread;
}

namespace IPC {
struct ChannelHandle;
}

namespace WebKit {
class WebStorageEventDispatcher;
}

namespace v8 {
class Extension;
}

// The RenderThreadBase is the minimal interface that a RenderView/Widget
// expects from a render thread. The interface basically abstracts a way to send
// and receive messages.
//
// TODO(brettw): This has two different and opposing usage patterns which
// make it confusing. It can be accessed through RenderThread::current(), which
// can be NULL during tests, or it can be passed as RenderThreadBase, which is
// mocked during tests. It should be changed to RenderThread::current()
// everywhere.
//
// See crbug.com/98375 for more details.
class CONTENT_EXPORT RenderThreadBase {
 public:
  virtual ~RenderThreadBase() {}

  virtual bool Send(IPC::Message* msg) = 0;

  // Called to add or remove a listener for a particular message routing ID.
  // These methods normally get delegated to a MessageRouter.
  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener) = 0;
  virtual void RemoveRoute(int32 routing_id) = 0;

  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter) = 0;
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) = 0;

  // Called by a RenderWidget when it is hidden or restored.
  virtual void WidgetHidden() = 0;
  virtual void WidgetRestored() = 0;
};

// The RenderThread class represents a background thread where RenderView
// instances live.  The RenderThread supports an API that is used by its
// consumer to talk indirectly to the RenderViews and supporting objects.
// Likewise, it provides an API for the RenderViews to talk back to the main
// process (i.e., their corresponding TabContents).
//
// Most of the communication occurs in the form of IPC messages.  They are
// routed to the RenderThread according to the routing IDs of the messages.
// The routing IDs correspond to RenderView instances.
class CONTENT_EXPORT RenderThread : public RenderThreadBase,
                                    public ChildThread {
 public:
  // Grabs the IPC channel name from the command line.
  RenderThread();
  // Constructor that's used when running in single process mode.
  explicit RenderThread(const std::string& channel_name);
  virtual ~RenderThread();

  // Returns the one render thread for this process.  Note that this should only
  // be accessed when running on the render thread itself
  //
  // TODO(brettw) this should be on the abstract base class instead of here,
  // and return the base class' interface instead. See crbug.com/98375.
  static RenderThread* current();

  // Returns the routing ID of the RenderWidget containing the current script
  // execution context (corresponding to WebFrame::frameForCurrentContext).
  static int32 RoutingIDForCurrentContext();

  // Returns the locale string to be used in WebKit.
  static std::string GetLocale();

  // Overridden from RenderThreadBase.
  virtual bool Send(IPC::Message* msg);
  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);
  virtual void RemoveRoute(int32 routing_id);
  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter);
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter);
  virtual void WidgetHidden();
  virtual void WidgetRestored();

  void AddObserver(RenderProcessObserver* observer);
  void RemoveObserver(RenderProcessObserver* observer);

  // These methods modify how the next message is sent.  Normally, when sending
  // a synchronous message that runs a nested message loop, we need to suspend
  // callbacks into WebKit.  This involves disabling timers and deferring
  // resource loads.  However, there are exceptions when we need to customize
  // the behavior.
  void DoNotSuspendWebKitSharedTimer();
  void DoNotNotifyWebKitOfModalLoop();

  AppCacheDispatcher* appcache_dispatcher() const {
    return appcache_dispatcher_.get();
  }

  IndexedDBDispatcher* indexed_db_dispatcher() const {
    return indexed_db_dispatcher_.get();
  }

  AudioInputMessageFilter* audio_input_message_filter() {
    return audio_input_message_filter_.get();
  }

  AudioMessageFilter* audio_message_filter() {
    return audio_message_filter_.get();
  }

  VideoCaptureImplManager* video_capture_impl_manager() const {
    return vc_manager_.get();
  }

  bool plugin_refresh_allowed() const { return plugin_refresh_allowed_; }

  double idle_notification_delay_in_s() const {
    return idle_notification_delay_in_s_;
  }
  void set_idle_notification_delay_in_s(double idle_notification_delay_in_s) {
    idle_notification_delay_in_s_ = idle_notification_delay_in_s;
  }

  // Synchronously establish a channel to the GPU plugin if not previously
  // established or if it has been lost (for example if the GPU plugin crashed).
  // If there is a pending asynchronous request, it will be completed by the
  // time this routine returns.
  GpuChannelHost* EstablishGpuChannelSync(content::CauseForGpuLaunch);

  // Get the GPU channel. Returns NULL if the channel is not established or
  // has been lost.
  GpuChannelHost* GetGpuChannel();

  // Returns a MessageLoopProxy instance corresponding to the message loop
  // of the thread on which file operations should be run. Must be called
  // on the renderer's main thread.
  scoped_refptr<base::MessageLoopProxy> GetFileThreadMessageLoopProxy();

  // Schedule a call to IdleHandler with the given initial delay.
  void ScheduleIdleHandler(double initial_delay_s);

  // A task we invoke periodically to assist with idle cleanup.
  void IdleHandler();

  // Registers the given V8 extension with WebKit.
  void RegisterExtension(v8::Extension* extension);

  // Returns true iff the extension is registered.
  bool IsRegisteredExtension(const std::string& v8_extension_name) const;

  // We initialize WebKit as late as possible.
  void EnsureWebKitInitialized();

  // Helper function to send over a string to be recorded by user metrics
  static void RecordUserMetrics(const std::string& action);

#if defined(OS_WIN)
  // Request that the given font be loaded by the browser so it's cached by the
  // OS. Please see ChildProcessHost::PreCacheFont for details.
  static bool PreCacheFont(const LOGFONT& log_font);

  // Release cached font.
  static bool ReleaseCachedFonts();
#endif  // OS_WIN

 private:
  virtual bool OnControlMessageReceived(const IPC::Message& msg);

  void Init();

  void OnSetZoomLevelForCurrentURL(const GURL& url, double zoom_level);
  void OnDOMStorageEvent(const DOMStorageMsg_Event_Params& params);
  void OnSetNextPageID(int32 next_page_id);
  void OnSetCSSColors(const std::vector<CSSColors::CSSColorMapping>& colors);
  void OnCreateNewView(const ViewMsg_New_Params& params);
  void OnTransferBitmap(const SkBitmap& bitmap, int resource_id);
  void OnPurgePluginListCache(bool reload_pages);
  void OnNetworkStateChanged(bool online);
  void OnGetAccessibilityTree();

  // These objects live solely on the render thread.
  scoped_ptr<ScopedRunnableMethodFactory<RenderThread> > task_factory_;
  scoped_ptr<AppCacheDispatcher> appcache_dispatcher_;
  scoped_ptr<IndexedDBDispatcher> indexed_db_dispatcher_;
  scoped_ptr<RendererWebKitPlatformSupportImpl> webkit_platform_support_;
  scoped_ptr<WebKit::WebStorageEventDispatcher> dom_storage_event_dispatcher_;

  // Used on the renderer and IPC threads.
  scoped_refptr<DBMessageFilter> db_message_filter_;
  scoped_refptr<AudioInputMessageFilter> audio_input_message_filter_;
  scoped_refptr<AudioMessageFilter> audio_message_filter_;
  scoped_refptr<DevToolsAgentFilter> devtools_agent_message_filter_;

  // Used on multiple threads.
  scoped_refptr<VideoCaptureImplManager> vc_manager_;

  // Used on multiple script execution context threads.
  scoped_ptr<WebDatabaseObserverImpl> web_database_observer_impl_;

  // If true, then a GetPlugins call is allowed to rescan the disk.
  bool plugin_refresh_allowed_;

  // The count of RenderWidgets running through this thread.
  int widget_count_;

  // The count of hidden RenderWidgets running through this thread.
  int hidden_widget_count_;

  // The current value of the idle notification timer delay.
  double idle_notification_delay_in_s_;

  bool suspend_webkit_shared_timer_;
  bool notify_webkit_of_modal_loop_;

  // Timer that periodically calls IdleHandler.
  base::RepeatingTimer<RenderThread> idle_timer_;

  // The channel from the renderer process to the GPU process.
  scoped_refptr<GpuChannelHost> gpu_channel_;

  // A lazily initiated thread on which file operations are run.
  scoped_ptr<base::Thread> file_thread_;

  // Map of registered v8 extensions. The key is the extension name.
  std::set<std::string> v8_extensions_;

  ObserverList<RenderProcessObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(RenderThread);
};

#endif  // CONTENT_RENDERER_RENDER_THREAD_H_
