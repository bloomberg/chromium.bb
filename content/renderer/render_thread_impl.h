// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_THREAD_IMPL_H_
#define CONTENT_RENDERER_RENDER_THREAD_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/common/content_export.h"
#include "content/common/frame_replication_state.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_result_codes.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/gpu/compositor_dependencies.h"
#include "net/base/network_change_notifier.h"
#include "third_party/WebKit/public/platform/WebConnectionType.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_MACOSX)
#include "third_party/WebKit/public/web/mac/WebScrollbarTheme.h"
#endif

class GrContext;
class SkBitmap;
struct FrameMsg_NewFrame_WidgetParams;
struct ViewMsg_New_Params;
struct WorkerProcessMsg_CreateWorker_Params;

namespace blink {
class WebGamepads;
class WebGraphicsContext3D;
class WebMediaStreamCenter;
class WebMediaStreamCenterClient;
}

namespace base {
class MessageLoopProxy;
class SingleThreadTaskRunner;
class Thread;
}

namespace cc {
class ContextProvider;
}

namespace cc_blink {
class ContextProviderWebContext;
}

namespace IPC {
class MessageFilter;
}

namespace media {
class AudioHardwareConfig;
class GpuVideoAcceleratorFactories;
}

namespace v8 {
class Extension;
}

namespace content {

class AppCacheDispatcher;
class AecDumpMessageFilter;
class AudioInputMessageFilter;
class AudioMessageFilter;
class AudioRendererMixerManager;
class BrowserPluginManager;
class CompositorForwardingMessageFilter;
class ContextProviderCommandBuffer;
class DBMessageFilter;
class DevToolsAgentFilter;
class DomStorageDispatcher;
class EmbeddedWorkerDispatcher;
class GpuChannelHost;
class IndexedDBDispatcher;
class InputHandlerManager;
class MediaStreamCenter;
class MemoryObserver;
class PeerConnectionDependencyFactory;
class MidiMessageFilter;
class NetInfoDispatcher;
class P2PSocketDispatcher;
class PeerConnectionTracker;
class RenderProcessObserver;
class RendererBlinkPlatformImpl;
class RendererDemuxerAndroid;
class RendererScheduler;
class ResourceDispatchThrottler;
class ResourceSchedulingFilter;
class V8SamplingProfiler;
class VideoCaptureImplManager;
class WebGraphicsContext3DCommandBufferImpl;
class WebRTCIdentityService;

#if defined(COMPILER_MSVC)
// See explanation for other RenderViewHostImpl which is the same issue.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// The RenderThreadImpl class represents a background thread where RenderView
// instances live.  The RenderThread supports an API that is used by its
// consumer to talk indirectly to the RenderViews and supporting objects.
// Likewise, it provides an API for the RenderViews to talk back to the main
// process (i.e., their corresponding WebContentsImpl).
//
// Most of the communication occurs in the form of IPC messages.  They are
// routed to the RenderThread according to the routing IDs of the messages.
// The routing IDs correspond to RenderView instances.
class CONTENT_EXPORT RenderThreadImpl
    : public RenderThread,
      public ChildThreadImpl,
      public GpuChannelHostFactory,
      NON_EXPORTED_BASE(public CompositorDependencies) {
 public:
  static RenderThreadImpl* current();

  explicit RenderThreadImpl(const InProcessChildThreadParams& params);
  explicit RenderThreadImpl(scoped_ptr<base::MessageLoop> main_message_loop);
  ~RenderThreadImpl() override;
  void Shutdown() override;

  // When initializing WebKit, ensure that any schemes needed for the content
  // module are registered properly.  Static to allow sharing with tests.
  static void RegisterSchemes();

  // Notify V8 that the date/time configuration of the system might have
  // changed.
  static void NotifyTimezoneChange();

  // RenderThread implementation:
  bool Send(IPC::Message* msg) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override;
  IPC::SyncChannel* GetChannel() override;
  std::string GetLocale() override;
  IPC::SyncMessageFilter* GetSyncMessageFilter() override;
  scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() override;
  void AddRoute(int32 routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32 routing_id) override;
  int GenerateRoutingID() override;
  void AddFilter(IPC::MessageFilter* filter) override;
  void RemoveFilter(IPC::MessageFilter* filter) override;
  void AddObserver(RenderProcessObserver* observer) override;
  void RemoveObserver(RenderProcessObserver* observer) override;
  void SetResourceDispatcherDelegate(
      ResourceDispatcherDelegate* delegate) override;
  void EnsureWebKitInitialized() override;
  void RecordAction(const base::UserMetricsAction& action) override;
  void RecordComputedAction(const std::string& action) override;
  scoped_ptr<base::SharedMemory> HostAllocateSharedMemoryBuffer(
      size_t buffer_size) override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  void RegisterExtension(v8::Extension* extension) override;
  void ScheduleIdleHandler(int64 initial_delay_ms) override;
  void IdleHandler() override;
  int64 GetIdleNotificationDelayInMs() const override;
  void SetIdleNotificationDelayInMs(
      int64 idle_notification_delay_in_ms) override;
  void UpdateHistograms(int sequence_number) override;
  int PostTaskToAllWebWorkers(const base::Closure& closure) override;
  bool ResolveProxy(const GURL& url, std::string* proxy_list) override;
  base::WaitableEvent* GetShutdownEvent() override;
  ServiceRegistry* GetServiceRegistry() override;

  // CompositorDependencies implementation.
  bool IsImplSidePaintingEnabled() override;
  bool IsGpuRasterizationForced() override;
  bool IsGpuRasterizationEnabled() override;
  bool IsThreadedGpuRasterizationEnabled() override;
  int GetGpuRasterizationMSAASampleCount() override;
  bool IsLcdTextEnabled() override;
  bool IsDistanceFieldTextEnabled() override;
  bool IsZeroCopyEnabled() override;
  bool IsOneCopyEnabled() override;
  bool IsElasticOverscrollEnabled() override;
  bool UseSingleThreadScheduler() override;
  uint32 GetImageTextureTarget() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorMainThreadTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorImplThreadTaskRunner() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  RendererScheduler* GetRendererScheduler() override;
  cc::ContextProvider* GetSharedMainThreadContextProvider() override;
  scoped_ptr<cc::BeginFrameSource> CreateExternalBeginFrameSource(
      int routing_id) override;

  // Synchronously establish a channel to the GPU plugin if not previously
  // established or if it has been lost (for example if the GPU plugin crashed).
  // If there is a pending asynchronous request, it will be completed by the
  // time this routine returns.
  GpuChannelHost* EstablishGpuChannelSync(CauseForGpuLaunch);


  // These methods modify how the next message is sent.  Normally, when sending
  // a synchronous message that runs a nested message loop, we need to suspend
  // callbacks into WebKit.  This involves disabling timers and deferring
  // resource loads.  However, there are exceptions when we need to customize
  // the behavior.
  void DoNotSuspendWebKitSharedTimer();
  void DoNotNotifyWebKitOfModalLoop();

  // True if we are running layout tests. This currently disables forwarding
  // various status messages to the console, skips network error pages, and
  // short circuits size update and focus events.
  bool layout_test_mode() const {
    return layout_test_mode_;
  }
  void set_layout_test_mode(bool layout_test_mode) {
    layout_test_mode_ = layout_test_mode;
  }

  RendererBlinkPlatformImpl* blink_platform_impl() const {
    DCHECK(blink_platform_impl_);
    return blink_platform_impl_.get();
  }

  CompositorForwardingMessageFilter* compositor_message_filter() const {
    return compositor_message_filter_.get();
  }

  InputHandlerManager* input_handler_manager() const {
    return input_handler_manager_.get();
  }

  // Will be null if threaded compositing has not been enabled.
  scoped_refptr<base::MessageLoopProxy> compositor_message_loop_proxy() const {
    return compositor_message_loop_proxy_;
  }

  AppCacheDispatcher* appcache_dispatcher() const {
    return appcache_dispatcher_.get();
  }

  DomStorageDispatcher* dom_storage_dispatcher() const {
    return dom_storage_dispatcher_.get();
  }

  EmbeddedWorkerDispatcher* embedded_worker_dispatcher() const {
    return embedded_worker_dispatcher_.get();
  }

  AudioInputMessageFilter* audio_input_message_filter() {
    return audio_input_message_filter_.get();
  }

  AudioMessageFilter* audio_message_filter() {
    return audio_message_filter_.get();
  }

  MidiMessageFilter* midi_message_filter() {
    return midi_message_filter_.get();
  }

#if defined(OS_ANDROID)
  RendererDemuxerAndroid* renderer_demuxer() {
    return renderer_demuxer_.get();
  }
#endif

  // Creates the embedder implementation of WebMediaStreamCenter.
  // The resulting object is owned by WebKit and deleted by WebKit at tear-down.
  blink::WebMediaStreamCenter* CreateMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client);

  BrowserPluginManager* browser_plugin_manager() const {
    return browser_plugin_manager_.get();
  }

#if defined(ENABLE_WEBRTC)
  // Returns a factory used for creating RTC PeerConnection objects.
  PeerConnectionDependencyFactory* GetPeerConnectionDependencyFactory();

  PeerConnectionTracker* peer_connection_tracker() {
    return peer_connection_tracker_.get();
  }

  // Current P2PSocketDispatcher. Set to NULL if P2P API is disabled.
  P2PSocketDispatcher* p2p_socket_dispatcher() {
    return p2p_socket_dispatcher_.get();
  }
#endif

  VideoCaptureImplManager* video_capture_impl_manager() const {
    return vc_manager_.get();
  }

  // Get the GPU channel. Returns NULL if the channel is not established or
  // has been lost.
  GpuChannelHost* GetGpuChannel();

  // Returns a MessageLoopProxy instance corresponding to the message loop
  // of the thread on which file operations should be run. Must be called
  // on the renderer's main thread.
  scoped_refptr<base::MessageLoopProxy> GetFileThreadMessageLoopProxy();

  // Returns a SingleThreadTaskRunner instance corresponding to the message loop
  // of the thread on which media operations should be run. Must be called
  // on the renderer's main thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetMediaThreadTaskRunner();

  // Causes the idle handler to skip sending idle notifications
  // on the two next scheduled calls, so idle notifications are
  // not sent for at least one notification delay.
  void PostponeIdleNotification();

  scoped_refptr<media::GpuVideoAcceleratorFactories> GetGpuFactories();

  scoped_refptr<cc_blink::ContextProviderWebContext>
  SharedMainThreadContextProvider();

  // AudioRendererMixerManager instance which manages renderer side mixer
  // instances shared based on configured audio parameters.  Lazily created on
  // first call.
  AudioRendererMixerManager* GetAudioRendererMixerManager();

  // AudioHardwareConfig contains audio hardware configuration for
  // renderer side clients.  Creation requires a synchronous IPC call so it is
  // lazily created on the first call.
  media::AudioHardwareConfig* GetAudioHardwareConfig();

#if defined(OS_WIN)
  void PreCacheFontCharacters(const LOGFONT& log_font,
                              const base::string16& str);
#endif

#if defined(ENABLE_WEBRTC)
  WebRTCIdentityService* get_webrtc_identity_service() {
    return webrtc_identity_service_.get();
  }
#endif

  // For producing custom V8 histograms. Custom histograms are produced if all
  // RenderViews share the same host, and the host is in the pre-specified set
  // of hosts we want to produce custom diagrams for. The name for a custom
  // diagram is the name of the corresponding generic diagram plus a
  // host-specific suffix.
  class CONTENT_EXPORT HistogramCustomizer {
   public:
    HistogramCustomizer();
    ~HistogramCustomizer();

    // Called when a top frame of a RenderView navigates. This function updates
    // RenderThreadImpl's information about whether all RenderViews are
    // displaying a page from the same host. |host| is the host where a
    // RenderView navigated, and |view_count| is the number of RenderViews in
    // this process.
    void RenderViewNavigatedToHost(const std::string& host, size_t view_count);

    // Used for customizing some histograms if all RenderViews share the same
    // host. Returns the current custom histogram name to use for
    // |histogram_name|, or |histogram_name| if it shouldn't be customized.
    std::string ConvertToCustomHistogramName(const char* histogram_name) const;

   private:
    friend class RenderThreadImplUnittest;

    // Used for updating the information on which is the common host which all
    // RenderView's share (if any). If there is no common host, this function is
    // called with an empty string.
    void SetCommonHost(const std::string& host);

    // The current common host of the RenderViews; empty string if there is no
    // common host.
    std::string common_host_;
    // The corresponding suffix.
    std::string common_host_histogram_suffix_;
    // Set of histograms for which we want to produce a custom histogram if
    // possible.
    std::set<std::string> custom_histograms_;

    DISALLOW_COPY_AND_ASSIGN(HistogramCustomizer);
  };

  HistogramCustomizer* histogram_customizer() {
    return &histogram_customizer_;
  }

  // Retrieve current gamepad data.
  void SampleGamepads(blink::WebGamepads* data);

  // Called by a RenderWidget when it is created or destroyed. This
  // allows the process to know when there are no visible widgets.
  void WidgetCreated();
  void WidgetDestroyed();
  void WidgetHidden();
  void WidgetRestored();

  void AddEmbeddedWorkerRoute(int32 routing_id, IPC::Listener* listener);
  void RemoveEmbeddedWorkerRoute(int32 routing_id);

  void RegisterPendingRenderFrameConnect(
      int routing_id,
      mojo::InterfaceRequest<mojo::ServiceProvider> services,
      mojo::ServiceProviderPtr exposed_services);

 protected:
  virtual void SetResourceDispatchTaskQueue(
    const scoped_refptr<base::SingleThreadTaskRunner>& resource_task_queue);

 private:
  // ChildThread
  bool OnControlMessageReceived(const IPC::Message& msg) override;

  // GpuChannelHostFactory implementation:
  bool IsMainThread() override;
  base::MessageLoop* GetMainLoop() override;
  scoped_refptr<base::MessageLoopProxy> GetIOLoopProxy() override;
  scoped_ptr<base::SharedMemory> AllocateSharedMemory(size_t size) override;
  CreateCommandBufferResult CreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id) override;

  void Init();

  void OnCreateNewFrame(int routing_id,
                        int parent_routing_id,
                        int proxy_routing_id,
                        const FrameReplicationState& replicated_state,
                        FrameMsg_NewFrame_WidgetParams params);
  void OnCreateNewFrameProxy(int routing_id,
                             int parent_routing_id,
                             int render_view_routing_id,
                             const FrameReplicationState& replicated_state);
  void OnSetZoomLevelForCurrentURL(const std::string& scheme,
                                   const std::string& host,
                                   double zoom_level);
  void OnCreateNewView(const ViewMsg_New_Params& params);
  void OnTransferBitmap(const SkBitmap& bitmap, int resource_id);
#if defined(ENABLE_PLUGINS)
  void OnPurgePluginListCache(bool reload_pages);
#endif
  void OnNetworkTypeChanged(net::NetworkChangeNotifier::ConnectionType type);
  void OnGetAccessibilityTree();
  void OnTempCrashWithData(const GURL& data);
  void OnUpdateTimezone();
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);
#if defined(OS_ANDROID)
  void OnSetWebKitSharedTimersSuspended(bool suspend);
#endif
#if defined(OS_MACOSX)
  void OnUpdateScrollbarTheme(float initial_button_delay,
                              float autoscroll_button_delay,
                              bool jump_on_track_click,
                              blink::ScrollerStyle preferred_scroller_style,
                              bool redraw);
#endif
  void OnCreateNewSharedWorker(
      const WorkerProcessMsg_CreateWorker_Params& params);

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateOffscreenContext3d();

  // These objects live solely on the render thread.
  scoped_ptr<AppCacheDispatcher> appcache_dispatcher_;
  scoped_ptr<DomStorageDispatcher> dom_storage_dispatcher_;
  scoped_ptr<IndexedDBDispatcher> main_thread_indexed_db_dispatcher_;
  scoped_ptr<RendererScheduler> renderer_scheduler_;
  scoped_ptr<RendererBlinkPlatformImpl> blink_platform_impl_;
  scoped_ptr<ResourceDispatchThrottler> resource_dispatch_throttler_;
  scoped_ptr<EmbeddedWorkerDispatcher> embedded_worker_dispatcher_;

  // Used on the render thread and deleted by WebKit at shutdown.
  blink::WebMediaStreamCenter* media_stream_center_;

  // Used on the renderer and IPC threads.
  scoped_refptr<DBMessageFilter> db_message_filter_;
  scoped_refptr<AudioInputMessageFilter> audio_input_message_filter_;
  scoped_refptr<AudioMessageFilter> audio_message_filter_;
  scoped_refptr<MidiMessageFilter> midi_message_filter_;
#if defined(OS_ANDROID)
  scoped_refptr<RendererDemuxerAndroid> renderer_demuxer_;
#endif
  scoped_refptr<DevToolsAgentFilter> devtools_agent_message_filter_;
  scoped_ptr<V8SamplingProfiler> v8_sampling_profiler_;

  scoped_ptr<BrowserPluginManager> browser_plugin_manager_;

#if defined(ENABLE_WEBRTC)
  scoped_ptr<PeerConnectionDependencyFactory> peer_connection_factory_;

  // This is used to communicate to the browser process the status
  // of all the peer connections created in the renderer.
  scoped_ptr<PeerConnectionTracker> peer_connection_tracker_;

  // Dispatches all P2P sockets.
  scoped_refptr<P2PSocketDispatcher> p2p_socket_dispatcher_;
#endif

  // Used on the render thread.
  scoped_ptr<VideoCaptureImplManager> vc_manager_;

  // Used for communicating registering AEC dump consumers with the browser and
  // receving AEC dump file handles when AEC dump is enabled. An AEC dump is
  // diagnostic audio data for WebRTC stored locally when enabled by the user in
  // chrome://webrtc-internals.
  scoped_refptr<AecDumpMessageFilter> aec_dump_message_filter_;

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
  bool webkit_shared_timer_suspended_;

  // The following flag is used to control layout test specific behavior.
  bool layout_test_mode_;

  // Timer that periodically calls IdleHandler.
  base::RepeatingTimer<RenderThreadImpl> idle_timer_;

  // The channel from the renderer process to the GPU process.
  scoped_refptr<GpuChannelHost> gpu_channel_;

  // Cache of variables that are needed on the compositor thread by
  // GpuChannelHostFactory methods.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  // The message loop of the renderer main thread.
  // This message loop should be destructed before the RenderThreadImpl
  // shuts down Blink.
  scoped_ptr<base::MessageLoop> main_message_loop_;

  // A lazily initiated thread on which file operations are run.
  scoped_ptr<base::Thread> file_thread_;

  // May be null if overridden by ContentRendererClient.
  scoped_ptr<base::Thread> compositor_thread_;

  // Thread for running multimedia operations (e.g., video decoding).
  scoped_ptr<base::Thread> media_thread_;

  // Will point to appropriate MessageLoopProxy after initialization,
  // regardless of whether |compositor_thread_| is overriden.
  scoped_refptr<base::MessageLoopProxy> compositor_message_loop_proxy_;

  base::CancelableCallback<void(const IPC::Message&)> main_input_callback_;
  scoped_refptr<IPC::MessageFilter> input_event_filter_;
  scoped_ptr<InputHandlerManager> input_handler_manager_;
  scoped_refptr<CompositorForwardingMessageFilter> compositor_message_filter_;

  scoped_refptr<cc_blink::ContextProviderWebContext>
      shared_main_thread_contexts_;

  ObserverList<RenderProcessObserver> observers_;

  scoped_refptr<ContextProviderCommandBuffer> gpu_va_context_provider_;

  scoped_ptr<AudioRendererMixerManager> audio_renderer_mixer_manager_;
  scoped_ptr<media::AudioHardwareConfig> audio_hardware_config_;

  HistogramCustomizer histogram_customizer_;

  scoped_ptr<base::MemoryPressureListener> memory_pressure_listener_;

#if defined(ENABLE_WEBRTC)
  scoped_ptr<WebRTCIdentityService> webrtc_identity_service_;
#endif

  scoped_ptr<MemoryObserver> memory_observer_;

  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_compositor_task_runner_;

  scoped_refptr<ResourceSchedulingFilter> resource_scheduling_filter_;

  // Compositor settings.
  bool is_gpu_rasterization_enabled_;
  bool is_gpu_rasterization_forced_;
  int gpu_rasterization_msaa_sample_count_;
  bool is_threaded_gpu_rasterization_enabled_;
  bool is_impl_side_painting_enabled_;
  bool is_lcd_text_enabled_;
  bool is_distance_field_text_enabled_;
  bool is_zero_copy_enabled_;
  bool is_one_copy_enabled_;
  bool is_elastic_overscroll_enabled_;
  unsigned use_image_texture_target_;

  class PendingRenderFrameConnect
      : public base::RefCounted<PendingRenderFrameConnect>,
        public mojo::ErrorHandler {
   public:
    PendingRenderFrameConnect(
        int routing_id,
        mojo::InterfaceRequest<mojo::ServiceProvider> services,
        mojo::ServiceProviderPtr exposed_services);

    mojo::InterfaceRequest<mojo::ServiceProvider>& services() {
      return services_;
    }

    mojo::ServiceProviderPtr& exposed_services() { return exposed_services_; }

   private:
    friend class base::RefCounted<PendingRenderFrameConnect>;

    ~PendingRenderFrameConnect() override;

    void OnConnectionError() override;

    int routing_id_;
    mojo::InterfaceRequest<mojo::ServiceProvider> services_;
    mojo::ServiceProviderPtr exposed_services_;
  };

  typedef std::map<int, scoped_refptr<PendingRenderFrameConnect>>
      PendingRenderFrameConnectMap;
  PendingRenderFrameConnectMap pending_render_frame_connects_;

  DISALLOW_COPY_AND_ASSIGN(RenderThreadImpl);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_THREAD_IMPL_H_
