// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "components/viz/service/display_embedder/shared_bitmap_allocation_notifier_impl.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/renderer_host/frame_sink_provider_impl.h"
#include "content/browser/renderer_host/media/renderer_audio_output_stream_factory_context_impl.h"
#include "content/browser/renderer_host/offscreen_canvas_provider_impl.h"
#include "content/browser/webrtc/webrtc_eventlog_host.h"
#include "content/common/associated_interface_registry_impl.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db.mojom.h"
#include "content/common/media/media_stream.mojom.h"
#include "content/common/media/renderer_audio_output_stream_factory.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/renderer_host.mojom.h"
#include "content/common/storage_partition_service.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_platform_file.h"
#include "media/media_features.h"
#include "mojo/edk/embedder/outgoing_broker_client_invitation.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/ui/public/interfaces/gpu.mojom.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gpu_switching_observer.h"

#if defined(OS_ANDROID)
#include "content/browser/android/synchronous_compositor_browser_filter.h"
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace base {
class CommandLine;
class MessageLoop;
class SharedPersistentMemoryAllocator;
}

namespace content {
class AudioInputRendererHost;
class ChildConnection;
class GpuClient;
class IndexedDBDispatcherHost;
class InProcessChildThreadParams;
class NotificationMessageFilter;
#if BUILDFLAG(ENABLE_WEBRTC)
class MediaStreamDispatcherHost;
class P2PSocketDispatcherHost;
#endif
class PermissionServiceContext;
class PeerConnectionTrackerHost;
class PushMessagingManager;
class RenderFrameMessageFilter;
class RenderProcessHostFactory;
class RenderWidgetHelper;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class ResourceMessageFilter;
class SiteInstance;
class SiteInstanceImpl;
class StoragePartition;
class StoragePartitionImpl;

typedef base::Thread* (*RendererMainThreadFactoryFunction)(
    const InProcessChildThreadParams& params);

// Implements a concrete RenderProcessHost for the browser process for talking
// to actual renderer processes (as opposed to mocks).
//
// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.
//
// This object is refcounted so that it can release its resources when all
// hosts using it go away.
//
// This object communicates back and forth with the RenderProcess object
// running in the renderer process. Each RenderProcessHost and RenderProcess
// keeps a list of RenderView (renderer) and WebContentsImpl (browser) which
// are correlated with IDs. This way, the Views and the corresponding ViewHosts
// communicate through the two process objects.
//
// A RenderProcessHost is also associated with one and only one
// StoragePartition.  This allows us to implement strong storage isolation
// because all the IPCs from the RenderViews (renderer) will only ever be able
// to access the partition they are assigned to.
class CONTENT_EXPORT RenderProcessHostImpl
    : public RenderProcessHost,
      public ChildProcessLauncher::Client,
      public ui::GpuSwitchingObserver,
      public mojom::RouteProvider,
      public mojom::AssociatedInterfaceProvider,
      public mojom::RendererHost {
 public:
  // Use the spare RenderProcessHost if it exists, or create a new one. This
  // should be the usual way to get a new RenderProcessHost.
  // If |storage_partition_impl| is null, the default partition from the
  // browser_context is used, using |site_instance| (for which a null value is
  // legal).
  static RenderProcessHost* CreateOrUseSpareRenderProcessHost(
      BrowserContext* browser_context,
      StoragePartitionImpl* storage_partition_impl,
      SiteInstance* site_instance,
      bool is_for_guests_only);

  // Create a new RenderProcessHost. In most cases
  // CreateOrUseSpareRenderProcessHost, above, should be used instead.
  // If |storage_partition_impl| is null, the default partition from the
  // browser_context is used, using |site_instance| (for which a null value is
  // legal). |site_instance| is not used if |storage_partition_impl| is not
  // null.
  static RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      StoragePartitionImpl* storage_partition_impl,
      SiteInstance* site_instance,
      bool is_for_guests_only);

  ~RenderProcessHostImpl() override;

  // RenderProcessHost implementation (public portion).
  bool Init() override;
  void EnableSendQueue() override;
  int GetNextRoutingID() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  void AddObserver(RenderProcessHostObserver* observer) override;
  void RemoveObserver(RenderProcessHostObserver* observer) override;
  void ShutdownForBadMessage(CrashReportMode crash_report_mode) override;
  void WidgetRestored() override;
  void WidgetHidden() override;
  int VisibleWidgetCount() const override;
  bool IsForGuestsOnly() const override;
  StoragePartition* GetStoragePartition() const override;
  bool Shutdown(int exit_code, bool wait) override;
  bool FastShutdownIfPossible(size_t page_count = 0,
                              bool skip_unload_handlers = false) override;
  base::ProcessHandle GetHandle() const override;
  bool IsReady() const override;
  BrowserContext* GetBrowserContext() const override;
  bool InSameStoragePartition(StoragePartition* partition) const override;
  int GetID() const override;
  bool HasConnection() const override;
  void SetIgnoreInputEvents(bool ignore_input_events) override;
  bool IgnoreInputEvents() const override;
  void Cleanup() override;
  void AddPendingView() override;
  void RemovePendingView() override;
  void AddWidget(RenderWidgetHost* widget) override;
  void RemoveWidget(RenderWidgetHost* widget) override;
#if defined(OS_ANDROID)
  void UpdateWidgetImportance(ChildProcessImportance old_value,
                              ChildProcessImportance new_value) override;
  ChildProcessImportance ComputeEffectiveImportance() override;
#endif
  void SetSuddenTerminationAllowed(bool enabled) override;
  bool SuddenTerminationAllowed() const override;
  IPC::ChannelProxy* GetChannel() override;
  void AddFilter(BrowserMessageFilter* filter) override;
  bool FastShutdownStarted() const override;
  base::TimeDelta GetChildProcessIdleTime() const override;
  void FilterURL(bool empty_allowed, GURL* url) override;
#if BUILDFLAG(ENABLE_WEBRTC)
  void EnableAudioDebugRecordings(const base::FilePath& file) override;
  void DisableAudioDebugRecordings() override;
  bool StartWebRTCEventLog(const base::FilePath& file_path) override;
  bool StopWebRTCEventLog() override;
  void SetEchoCanceller3(bool enable) override;
  void SetWebRtcLogMessageCallback(
      base::Callback<void(const std::string&)> callback) override;
  void ClearWebRtcLogMessageCallback() override;
  WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) override;
#endif
  void ResumeDeferredNavigation(const GlobalRequestID& request_id) override;
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override;
  const service_manager::Identity& GetChildIdentity() const override;
  std::unique_ptr<base::SharedPersistentMemoryAllocator> TakeMetricsAllocator()
      override;
  const base::TimeTicks& GetInitTimeForNavigationMetrics() const override;
  bool IsProcessBackgrounded() const override;
  void IncrementKeepAliveRefCount() override;
  void DecrementKeepAliveRefCount() override;
  void DisableKeepAliveRefCount() override;
  bool IsKeepAliveRefCountDisabled() override;
  void PurgeAndSuspend() override;
  void Resume() override;
  mojom::Renderer* GetRendererInterface() override;
  resource_coordinator::ResourceCoordinatorInterface*
  GetProcessResourceCoordinator() override;

  void SetIsNeverSuitableForReuse() override;
  bool MayReuseHost() override;
  bool IsUnused() override;
  void SetIsUsed() override;

  bool HostHasNotBeenUsed() override;

  mojom::RouteProvider* GetRemoteRouteProvider();

  // IPC::Sender via RenderProcessHost.
  bool Send(IPC::Message* msg) override;

  // IPC::Listener via RenderProcessHost.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnBadMessageReceived(const IPC::Message& message) override;

  // ChildProcessLauncher::Client implementation.
  void OnProcessLaunched() override;
  void OnProcessLaunchFailed(int error_code) override;

  // Call this function when it is evident that the child process is actively
  // performing some operation, for example if we just received an IPC message.
  void mark_child_process_activity_time() {
    child_process_activity_time_ = base::TimeTicks::Now();
  }

  // Used to extend the lifetime of the sessions until the render view
  // in the renderer is fully closed. This is static because its also called
  // with mock hosts as input in test cases.
  static void ReleaseOnCloseACK(
      RenderProcessHost* host,
      const SessionStorageNamespaceMap& sessions,
      int view_route_id);

  // Register/unregister the host identified by the host id in the global host
  // list.
  static void RegisterHost(int host_id, RenderProcessHost* host);
  static void UnregisterHost(int host_id);

  // Implementation of FilterURL below that can be shared with the mock class.
  static void FilterURL(RenderProcessHost* rph, bool empty_allowed, GURL* url);

  // Returns true if |host| is suitable for launching a new view with |site_url|
  // in the given |browser_context|.
  static bool IsSuitableHost(RenderProcessHost* host,
                             BrowserContext* browser_context,
                             const GURL& site_url);

  // Returns an existing RenderProcessHost for |url| in |browser_context|,
  // if one exists.  Otherwise a new RenderProcessHost should be created and
  // registered using RegisterProcessHostForSite().
  // This should only be used for process-per-site mode, which can be enabled
  // globally with a command line flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite.
  static RenderProcessHost* GetProcessHostForSite(
      BrowserContext* browser_context,
      const GURL& url);

  // Registers the given |process| to be used for any instance of |url|
  // within |browser_context|.
  // This should only be used for process-per-site mode, which can be enabled
  // globally with a command line flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite.
  static void RegisterProcessHostForSite(
      BrowserContext* browser_context,
      RenderProcessHost* process,
      const GURL& url);

  // Returns a suitable RenderProcessHost to use for |site_instance|. Depending
  // on the SiteInstance's ProcessReusePolicy and its url, this may be an
  // existing RenderProcessHost or a new one.
  static RenderProcessHost* GetProcessHostForSiteInstance(
      BrowserContext* browser_context,
      SiteInstanceImpl* site_instance);

  // Cleanup and remove any spare renderer. This should be used when a
  // navigation has occurred or will be occurring that will not use the spare
  // renderer and resources should be cleaned up.
  static void CleanupSpareRenderProcessHost();

  static base::MessageLoop* GetInProcessRendererThreadForTesting();

  // This forces a renderer that is running "in process" to shut down.
  static void ShutDownInProcessRenderer();

  static void RegisterRendererMainThreadFactory(
      RendererMainThreadFactoryFunction create);

  // Allows external code to supply a function which creates a
  // StoragePartitionService. Used for supplying test versions of the
  // service.
  using CreateStoragePartitionServiceFunction =
      void (*)(RenderProcessHostImpl* rph,
               mojom::StoragePartitionServiceRequest request);
  static void SetCreateStoragePartitionServiceFunction(
      CreateStoragePartitionServiceFunction function);

  RenderFrameMessageFilter* render_frame_message_filter_for_testing() const {
    return render_frame_message_filter_.get();
  }

  NotificationMessageFilter* notification_message_filter() const {
    return notification_message_filter_.get();
  }

#if defined(OS_ANDROID)
  SynchronousCompositorBrowserFilter* synchronous_compositor_filter() const {
    return synchronous_compositor_filter_.get();
  }
#endif

  void set_is_for_guests_only_for_testing(bool is_for_guests_only) {
    is_for_guests_only_ = is_for_guests_only;
  }

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
  // Launch the zygote early in the browser startup.
  static void EarlyZygoteLaunch();
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

  void RecomputeAndUpdateWebKitPreferences();

  RendererAudioOutputStreamFactoryContext*
  GetRendererAudioOutputStreamFactoryContext() override;

  // Called when a video capture stream or an audio stream is added or removed
  // and used to determine if the process should be backgrounded or not.
  void OnMediaStreamAdded() override;
  void OnMediaStreamRemoved() override;
  int get_media_stream_count_for_testing() const { return media_stream_count_; }

  // Sets the global factory used to create new RenderProcessHosts.  It may be
  // nullptr, in which case the default RenderProcessHost will be created (this
  // is the behavior if you don't call this function).  The factory must be set
  // back to nullptr before it's destroyed; ownership is not transferred.
  static void set_render_process_host_factory(
      const RenderProcessHostFactory* rph_factory);

  // Tracks which sites frames are hosted in which RenderProcessHosts.
  static void AddFrameWithSite(BrowserContext* browser_context,
                               RenderProcessHost* render_process_host,
                               const GURL& site_url);
  static void RemoveFrameWithSite(BrowserContext* browser_context,
                                  RenderProcessHost* render_process_host,
                                  const GURL& site_url);

  // Tracks which sites navigations are expected to commit in which
  // RenderProcessHosts.
  static void AddExpectedNavigationToSite(
      BrowserContext* browser_context,
      RenderProcessHost* render_process_host,
      const GURL& site_url);
  static void RemoveExpectedNavigationToSite(
      BrowserContext* browser_context,
      RenderProcessHost* render_process_host,
      const GURL& site_url);

  viz::SharedBitmapAllocationNotifierImpl* GetSharedBitmapAllocationNotifier()
      override;

  // Return the spare RenderProcessHost, if it exists. There is at most one
  // globally-used spare RenderProcessHost at any time.
  static RenderProcessHost* GetSpareRenderProcessHostForTesting();

 protected:
  // A proxy for our IPC::Channel that lives on the IO thread.
  std::unique_ptr<IPC::ChannelProxy> channel_;

  // True if fast shutdown has been performed on this RPH.
  bool fast_shutdown_started_;

  // True if we've posted a DeleteTask and will be deleted soon.
  bool deleting_soon_;

#ifndef NDEBUG
  // True if this object has deleted itself.
  bool is_self_deleted_;
#endif

  // The count of currently swapped out but pending RenderViews.  We have
  // started to swap these in, so the renderer process should not exit if
  // this count is non-zero.
  int32_t pending_views_;

 private:
  friend class ChildProcessLauncherBrowserTest_ChildSpawnFail_Test;
  friend class VisitRelayingRenderProcessHost;
  friend class StoragePartitonInterceptor;
  class ConnectionFilterController;
  class ConnectionFilterImpl;

  // Use CreateRenderProcessHost() instead of calling this constructor
  // directly.
  RenderProcessHostImpl(BrowserContext* browser_context,
                        StoragePartitionImpl* storage_partition_impl,
                        bool is_for_guests_only);

  // Initializes a new IPC::ChannelProxy in |channel_|, which will be connected
  // to the next child process launched for this host, if any.
  void InitializeChannelProxy();

  // Resets |channel_|, removing it from the attachment broker if necessary.
  // Always call this in lieu of directly resetting |channel_|.
  void ResetChannelProxy();

  // Creates and adds the IO thread message filters.
  void CreateMessageFilters();

  // Registers Mojo interfaces to be exposed to the renderer.
  void RegisterMojoInterfaces();

  // mojom::RouteProvider:
  void GetRoute(
      int32_t routing_id,
      mojom::AssociatedInterfaceProviderAssociatedRequest request) override;

  // mojom::AssociatedInterfaceProvider:
  void GetAssociatedInterface(
      const std::string& name,
      mojom::AssociatedInterfaceAssociatedRequest request) override;

  // mojom::RendererHost
  void GetBlobURLLoaderFactory(mojom::URLLoaderFactoryRequest request) override;

  void BindRouteProvider(mojom::RouteProviderAssociatedRequest request);

  void CreateMusGpuRequest(ui::mojom::GpuRequest request);
  void CreateOffscreenCanvasProvider(
      blink::mojom::OffscreenCanvasProviderRequest request);
  void BindFrameSinkProvider(mojom::FrameSinkProviderRequest request);
  void BindSharedBitmapAllocationNotifier(
      viz::mojom::SharedBitmapAllocationNotifierRequest request);
  void CreateStoragePartitionService(
      mojom::StoragePartitionServiceRequest request);
  void CreateRendererHost(mojom::RendererHostRequest request);
  void CreateURLLoaderFactory(mojom::URLLoaderFactoryRequest request);

  // Control message handlers.
  void OnShutdownRequest();
  void SuddenTerminationChanged(bool enabled);
  void OnUserMetricsRecordAction(const std::string& action);
  void OnCloseACK(int old_route_id);

  // Generates a command line to be used to spawn a renderer and appends the
  // results to |*command_line|.
  void AppendRendererCommandLine(base::CommandLine* command_line);

  // Copies applicable command line switches from the given |browser_cmd| line
  // flags to the output |renderer_cmd| line flags. Not all switches will be
  // copied over.
  void PropagateBrowserCommandLineToRenderer(
      const base::CommandLine& browser_cmd,
      base::CommandLine* renderer_cmd);

  // Inspects the current object state and sets/removes background priority if
  // appropriate. Should be called after any of the involved data members
  // change.
  void UpdateProcessPriority();

  // Creates a PersistentMemoryAllocator and shares it with the renderer
  // process for it to store histograms from that process. The allocator is
  // available for extraction by a SubprocesMetricsProvider in order to
  // report those histograms to UMA.
  void CreateSharedRendererHistogramAllocator();

  // Handle termination of our process.
  void ProcessDied(bool already_dead, RendererClosedDetails* known_details);

  // GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

  // Returns the default subframe RenderProcessHost to use for |site_instance|.
  static RenderProcessHost* GetDefaultSubframeProcessHost(
      BrowserContext* browser_context,
      SiteInstanceImpl* site_instance,
      bool is_for_guests_only);

  // Returns a RenderProcessHost that is rendering |site_url| in one of its
  // frames, or that is expecting a navigation to |site_url|.
  static RenderProcessHost* FindReusableProcessHostForSite(
      BrowserContext* browser_context,
      const GURL& site_url);

#if BUILDFLAG(ENABLE_WEBRTC)
  void CreateMediaStreamDispatcherHost(
      const std::string& salt,
      MediaStreamManager* media_stream_manager,
      mojom::MediaStreamDispatcherHostRequest request);
  void OnRegisterAecDumpConsumer(int id);
  void OnUnregisterAecDumpConsumer(int id);
  void RegisterAecDumpConsumerOnUIThread(int id);
  void UnregisterAecDumpConsumerOnUIThread(int id);
  void EnableAecDumpForId(const base::FilePath& file, int id);
  // Sends |file_for_transit| to the render process.
  void SendAecDumpFileToRenderer(int id,
                                 IPC::PlatformFileForTransit file_for_transit);
  void SendDisableAecDumpToRenderer();
  base::FilePath GetAecDumpFilePathWithExtensions(const base::FilePath& file);
  base::SequencedTaskRunner& GetAecDumpFileTaskRunner();
#endif

  static void OnMojoError(int render_process_id, const std::string& error);

  template <typename InterfaceType>
  using AddInterfaceCallback =
      base::Callback<void(mojo::InterfaceRequest<InterfaceType>)>;

  template <typename CallbackType>
  struct InterfaceGetter;

  template <typename InterfaceType>
  struct InterfaceGetter<AddInterfaceCallback<InterfaceType>> {
    static void GetInterfaceOnUIThread(
        base::WeakPtr<RenderProcessHostImpl> weak_host,
        const AddInterfaceCallback<InterfaceType>& callback,
        mojo::InterfaceRequest<InterfaceType> request) {
      if (!weak_host)
        return;
      callback.Run(std::move(request));
    }
  };

  // Helper to bind an interface callback whose lifetime is limited to that of
  // the render process currently hosted by the RPHI. Callbacks added by this
  // method will never run beyond the next invocation of Cleanup().
  template <typename CallbackType>
  void AddUIThreadInterface(service_manager::BinderRegistry* registry,
                            const CallbackType& callback) {
    registry->AddInterface(
        base::Bind(&InterfaceGetter<CallbackType>::GetInterfaceOnUIThread,
                   instance_weak_factory_->GetWeakPtr(), callback),
        BrowserThread::GetTaskRunnerForThread(BrowserThread::UI));
  }

  std::unique_ptr<mojo::edk::OutgoingBrokerClientInvitation>
      broker_client_invitation_;

  std::unique_ptr<ChildConnection> child_connection_;
  int connection_filter_id_ =
      ServiceManagerConnection::kInvalidConnectionFilterId;
  scoped_refptr<ConnectionFilterController> connection_filter_controller_;
  service_manager::mojom::ServicePtr test_service_;

  size_t keep_alive_ref_count_;

  // Set in DisableKeepAliveRefCount(). When true, |keep_alive_ref_count_| must
  // no longer be modified.
  bool is_keep_alive_ref_count_disabled_;

  // Whether this host is never suitable for reuse as determined in the
  // MayReuseHost() function.
  bool is_never_suitable_for_reuse_ = false;

  // The registered IPC listener objects. When this list is empty, we should
  // delete ourselves.
  base::IDMap<IPC::Listener*> listeners_;

  // Mojo interfaces provided to the child process are registered here if they
  // need consistent delivery ordering with legacy IPC, and are process-wide in
  // nature (e.g. metrics, memory usage).
  std::unique_ptr<AssociatedInterfaceRegistryImpl> associated_interfaces_;

  mojo::AssociatedBinding<mojom::RouteProvider> route_provider_binding_;
  mojo::AssociatedBindingSet<mojom::AssociatedInterfaceProvider, int32_t>
      associated_interface_provider_bindings_;

  // The count of currently visible widgets.  Since the host can be a container
  // for multiple widgets, it uses this count to determine when it should be
  // backgrounded.
  int32_t visible_widgets_;

#if defined(OS_ANDROID)
  // Track count of number of widgets with each possible ChildProcessImportance
  // value.
  int32_t widget_importance_counts_[static_cast<size_t>(
      ChildProcessImportance::COUNT)] = {0};

#endif

  // The set of widgets in this RenderProcessHostImpl.
  std::set<RenderWidgetHostImpl*> widgets_;

  ChildProcessLauncherPriority priority_;

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  scoped_refptr<RenderFrameMessageFilter> render_frame_message_filter_;

  // The filter for Web Notification messages coming from the renderer. Holds a
  // closure per notification that must be freed when the notification closes.
  scoped_refptr<NotificationMessageFilter> notification_message_filter_;

#if defined(OS_ANDROID)
  scoped_refptr<SynchronousCompositorBrowserFilter>
      synchronous_compositor_filter_;
#endif

  // Used in single-process mode.
  std::unique_ptr<base::Thread> in_process_renderer_;

  // True after Init() has been called.
  bool is_initialized_ = false;

  // True after ProcessDied(), until the next call to Init().
  bool is_dead_ = false;

  // PlzNavigate
  // Stores the time at which the first call to Init happened.
  base::TimeTicks init_time_;

  // Used to launch and terminate the process without blocking the UI thread.
  std::unique_ptr<ChildProcessLauncher> child_process_launcher_;

  // The globally-unique identifier for this RPH.
  const int id_;

  // A secondary ID used by the Service Manager to distinguish different
  // incarnations of the same RPH from each other. Unlike |id_| this is not
  // globally unique, but it is guaranteed to change every time ProcessDied() is
  // called.
  int instance_id_ = 1;

  BrowserContext* const browser_context_;

  // Owned by |browser_context_|.
  StoragePartitionImpl* storage_partition_impl_;

  // The observers watching our lifetime.
  base::ObserverList<RenderProcessHostObserver> observers_;

  // True if the process can be shut down suddenly.  If this is true, then we're
  // sure that all the RenderViews in the process can be shutdown suddenly.  If
  // it's false, then specific RenderViews might still be allowed to be shutdown
  // suddenly by checking their SuddenTerminationAllowed() flag.  This can occur
  // if one WebContents has an unload event listener but another WebContents in
  // the same process doesn't.
  bool sudden_termination_allowed_;

  // Set to true if we shouldn't send input events.  We actually do the
  // filtering for this at the render widget level.
  bool ignore_input_events_;

  // Records the last time we regarded the child process active.
  base::TimeTicks child_process_activity_time_;

  // Indicates whether this RenderProcessHost is exclusively hosting guest
  // RenderFrames.
  bool is_for_guests_only_;

  // Indicates whether this RenderProcessHost is unused, meaning that it has
  // not committed any web content, and it has not been given to a SiteInstance
  // that has a site assigned.
  bool is_unused_;

  // Prevents the class from being added as a GpuDataManagerImpl observer more
  // than once.
  bool gpu_observer_registered_;

  // Set if a call to Cleanup is required once the RenderProcessHostImpl is no
  // longer within the RenderProcessHostObserver::RenderProcessExited callbacks.
  bool delayed_cleanup_needed_;

  // Indicates whether RenderProcessHostImpl is currently iterating and calling
  // through RenderProcessHostObserver::RenderProcessExited.
  bool within_process_died_observer_;

  std::unique_ptr<RendererAudioOutputStreamFactoryContextImpl,
                  BrowserThread::DeleteOnIOThread>
      audio_output_stream_factory_context_;

  scoped_refptr<AudioInputRendererHost> audio_input_renderer_host_;

#if BUILDFLAG(ENABLE_WEBRTC)
  scoped_refptr<P2PSocketDispatcherHost> p2p_socket_dispatcher_host_;

  // Must be accessed on UI thread.
  std::vector<int> aec_dump_consumers_;
  base::Optional<bool> override_aec3_;

  WebRtcStopRtpDumpCallback stop_rtp_dump_callback_;

  WebRTCEventLogHost webrtc_eventlog_host_;

  scoped_refptr<base::SequencedTaskRunner>
      audio_debug_recordings_file_task_runner_;

  std::unique_ptr<MediaStreamDispatcherHost, BrowserThread::DeleteOnIOThread>
      media_stream_dispatcher_host_;
#endif

  // Forwards messages between WebRTCInternals in the browser process
  // and PeerConnectionTracker in the renderer process.
  // It holds a raw pointer to webrtc_eventlog_host_, and therefore should be
  // defined below it so it is destructed first.
  scoped_refptr<PeerConnectionTrackerHost> peer_connection_tracker_host_;

  // Records the time when the process starts surviving for workers for UMA.
  base::TimeTicks keep_alive_start_time_;

  // Context shared for each mojom::PermissionService instance created for this
  // RPH.
  std::unique_ptr<PermissionServiceContext> permission_service_context_;

  // The memory allocator, if any, in which the renderer will write its metrics.
  std::unique_ptr<base::SharedPersistentMemoryAllocator> metrics_allocator_;

  std::unique_ptr<IndexedDBDispatcherHost, BrowserThread::DeleteOnIOThread>
      indexed_db_factory_;

  bool channel_connected_;
  bool sent_render_process_ready_;

#if defined(OS_ANDROID)
  // UI thread is the source of sync IPCs and all shutdown signals.
  // Therefore a proper shutdown event to unblock the UI thread is not
  // possible without massive refactoring shutdown code.
  // Luckily Android never performs a clean shutdown. So explicitly
  // ignore this problem.
  base::WaitableEvent never_signaled_;
#endif

  scoped_refptr<ResourceMessageFilter> resource_message_filter_;
  std::unique_ptr<GpuClient, BrowserThread::DeleteOnIOThread> gpu_client_;
  std::unique_ptr<PushMessagingManager, BrowserThread::DeleteOnIOThread>
      push_messaging_manager_;

  std::unique_ptr<OffscreenCanvasProviderImpl> offscreen_canvas_provider_;

  mojom::RouteProviderAssociatedPtr remote_route_provider_;
  mojom::RendererAssociatedPtr renderer_interface_;
  mojo::Binding<mojom::RendererHost> renderer_host_binding_;

  // Tracks active audio and video streams within the render process; used to
  // determine if if a process should be backgrounded.
  int media_stream_count_ = 0;

  std::unique_ptr<resource_coordinator::ResourceCoordinatorInterface>
      process_resource_coordinator_;

  // A WeakPtrFactory which is reset every time Cleanup() runs. Used to vend
  // WeakPtrs which are invalidated any time the RPHI is recycled.
  std::unique_ptr<base::WeakPtrFactory<RenderProcessHostImpl>>
      instance_weak_factory_;

  FrameSinkProviderImpl frame_sink_provider_;

  viz::SharedBitmapAllocationNotifierImpl
      shared_bitmap_allocation_notifier_impl_;

  base::WeakPtrFactory<RenderProcessHostImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_
