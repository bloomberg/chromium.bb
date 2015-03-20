// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_

#include <map>
#include <queue>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/power_monitor_message_broadcaster.h"
#include "content/common/content_export.h"
#include "content/common/mojo/service_registry_impl.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_ptr.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gpu_switching_observer.h"

namespace base {
class CommandLine;
class MessageLoop;
}

namespace gfx {
class Size;
}

namespace gpu {
class ValueStateMap;
}

namespace IPC {
class ChannelMojoHost;
}

namespace content {
class AudioRendererHost;
class BrowserCdmManager;
class BrowserDemuxerAndroid;
class GpuMessageFilter;
class InProcessChildThreadParams;
class MessagePortMessageFilter;
class MojoApplicationHost;
class NotificationMessageFilter;
#if defined(ENABLE_WEBRTC)
class P2PSocketDispatcherHost;
#endif
class PermissionServiceContext;
class PeerConnectionTrackerHost;
class RendererMainThread;
class RenderWidgetHelper;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class RenderWidgetHostViewFrameSubscriber;
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
      public ui::GpuSwitchingObserver {
 public:
  RenderProcessHostImpl(BrowserContext* browser_context,
                        StoragePartitionImpl* storage_partition_impl,
                        bool is_isolated_guest);
  ~RenderProcessHostImpl() override;

  // RenderProcessHost implementation (public portion).
  void EnableSendQueue() override;
  bool Init() override;
  int GetNextRoutingID() override;
  void AddRoute(int32 routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32 routing_id) override;
  void AddObserver(RenderProcessHostObserver* observer) override;
  void RemoveObserver(RenderProcessHostObserver* observer) override;
  void ShutdownForBadMessage() override;
  void WidgetRestored() override;
  void WidgetHidden() override;
  int VisibleWidgetCount() const override;
  bool IsIsolatedGuest() const override;
  StoragePartition* GetStoragePartition() const override;
  bool Shutdown(int exit_code, bool wait) override;
  bool FastShutdownIfPossible() override;
  void DumpHandles() override;
  base::ProcessHandle GetHandle() const override;
  BrowserContext* GetBrowserContext() const override;
  bool InSameStoragePartition(StoragePartition* partition) const override;
  int GetID() const override;
  bool HasConnection() const override;
  void SetIgnoreInputEvents(bool ignore_input_events) override;
  bool IgnoreInputEvents() const override;
  void Cleanup() override;
  void AddPendingView() override;
  void RemovePendingView() override;
  bool SuddenTerminationAllowed() const override;
  IPC::ChannelProxy* GetChannel() override;
  void AddFilter(BrowserMessageFilter* filter) override;
  bool FastShutdownForPageCount(size_t count) override;
  bool FastShutdownStarted() const override;
  base::TimeDelta GetChildProcessIdleTime() const override;
  void ResumeRequestsForView(int route_id) override;
  void FilterURL(bool empty_allowed, GURL* url) override;
#if defined(ENABLE_WEBRTC)
  void EnableAecDump(const base::FilePath& file) override;
  void DisableAecDump() override;
  void SetWebRtcLogMessageCallback(
      base::Callback<void(const std::string&)> callback) override;
  WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) override;
#endif
  void ResumeDeferredNavigation(const GlobalRequestID& request_id) override;
  void NotifyTimezoneChange() override;
  ServiceRegistry* GetServiceRegistry() override;
  const base::TimeTicks& GetInitTimeForNavigationMetrics() const override;
  bool SubscribeUniformEnabled() const override;
  void OnAddSubscription(unsigned int target) override;
  void OnRemoveSubscription(unsigned int target) override;
  void SendUpdateValueState(
      unsigned int target, const gpu::ValueState& state) override;
#if defined(ENABLE_BROWSER_CDMS)
  media::BrowserCdm* GetBrowserCdm(int render_frame_id,
                                   int cdm_id) const override;
#endif

  // IPC::Sender via RenderProcessHost.  SendHelper() can be used to ensure
  // allocated messages are not leaked after RPHI is destructed.
  bool Send(IPC::Message* msg) override;
  bool SendHelper(scoped_ptr<IPC::Message> msg);

  // IPC::Listener via RenderProcessHost.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32 peer_pid) override;
  void OnChannelError() override;
  void OnBadMessageReceived(const IPC::Message& message) override;

  // ChildProcessLauncher::Client implementation.
  void OnProcessLaunched() override;
  void OnProcessLaunchFailed() override;

  scoped_refptr<AudioRendererHost> audio_renderer_host() const;

  // Call this function when it is evident that the child process is actively
  // performing some operation, for example if we just received an IPC message.
  void mark_child_process_activity_time() {
    child_process_activity_time_ = base::TimeTicks::Now();
  }

  // Start and end frame subscription for a specific renderer.
  // This API only supports subscription to accelerated composited frames.
  void BeginFrameSubscription(
      int route_id,
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber);
  void EndFrameSubscription(int route_id);

#if defined(ENABLE_WEBRTC)
  // Fires the webrtc log message callback with |message|, if callback is set.
  void WebRtcLogMessage(const std::string& message);
#endif

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

  static base::MessageLoop* GetInProcessRendererThreadForTesting();

  // This forces a renderer that is running "in process" to shut down.
  static void ShutDownInProcessRenderer();

  static void RegisterRendererMainThreadFactory(
      RendererMainThreadFactoryFunction create);

#if defined(OS_ANDROID)
  const scoped_refptr<BrowserDemuxerAndroid>& browser_demuxer_android() {
    return browser_demuxer_android_;
  }
#endif

#if defined(ENABLE_BROWSER_CDMS)
  const scoped_refptr<BrowserCdmManager>& browser_cdm_manager() {
    return browser_cdm_manager_;
  }
#endif

  MessagePortMessageFilter* message_port_message_filter() const {
    return message_port_message_filter_.get();
  }

  NotificationMessageFilter* notification_message_filter() const {
    return notification_message_filter_.get();
  }

  void set_is_isolated_guest_for_testing(bool is_isolated_guest) {
    is_isolated_guest_ = is_isolated_guest;
  }

  // Called when the existence of the other renderer process which is connected
  // to the Worker in this renderer process has changed.
  // It is only called when "enable-embedded-shared-worker" flag is set.
  void IncrementWorkerRefCount();
  void DecrementWorkerRefCount();

  // Call this function to resume the navigation when it was deferred
  // immediately after receiving response headers.
  void ResumeResponseDeferredAtStart(const GlobalRequestID& request_id);

  void GetAudioOutputControllers(
      const GetAudioOutputControllersCallback& callback) const override;

 protected:
  // A proxy for our IPC::Channel that lives on the IO thread (see
  // browser_process.h)
  scoped_ptr<IPC::ChannelProxy> channel_;

  // A host object ChannelMojo needs. The lifetime is bound to
  // the RenderProcessHostImpl, not the channel.
  scoped_ptr<IPC::ChannelMojoHost> channel_mojo_host_;

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
  int32 pending_views_;

 private:
  friend class VisitRelayingRenderProcessHost;

  scoped_ptr<IPC::ChannelProxy> CreateChannelProxy(
      const std::string& channel_id);

  // Creates and adds the IO thread message filters.
  void CreateMessageFilters();

  // Registers Mojo services to be exposed to the renderer.
  void RegisterMojoServices();

  // Control message handlers.
  void OnShutdownRequest();
  void OnDumpHandlesDone();
  void OnSuddenTerminationChanged(bool enabled);
  void OnUserMetricsRecordAction(const std::string& action);
  void OnSavedPageAsMHTML(int job_id, int64 mhtml_file_size);
  void OnCloseACK(int old_route_id);

  // Generates a command line to be used to spawn a renderer and appends the
  // results to |*command_line|.
  void AppendRendererCommandLine(base::CommandLine* command_line) const;

  // Copies applicable command line switches from the given |browser_cmd| line
  // flags to the output |renderer_cmd| line flags. Not all switches will be
  // copied over.
  void PropagateBrowserCommandLineToRenderer(
      const base::CommandLine& browser_cmd,
      base::CommandLine* renderer_cmd) const;

  // Callers can reduce the RenderProcess' priority.
  void SetBackgrounded(bool backgrounded);

  // Handle termination of our process.
  void ProcessDied(bool already_dead, RendererClosedDetails* known_details);

  // GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

#if defined(ENABLE_WEBRTC)
  void OnRegisterAecDumpConsumer(int id);
  void OnUnregisterAecDumpConsumer(int id);
  void RegisterAecDumpConsumerOnUIThread(int id);
  void UnregisterAecDumpConsumerOnUIThread(int id);
  void EnableAecDumpForId(const base::FilePath& file, int id);
  // Sends |file_for_transit| to the render process.
  void SendAecDumpFileToRenderer(int id,
                                 IPC::PlatformFileForTransit file_for_transit);
  void SendDisableAecDumpToRenderer();
#endif

  scoped_ptr<MojoApplicationHost> mojo_application_host_;

  // The registered IPC listener objects. When this list is empty, we should
  // delete ourselves.
  IDMap<IPC::Listener> listeners_;

  // The count of currently visible widgets.  Since the host can be a container
  // for multiple widgets, it uses this count to determine when it should be
  // backgrounded.
  int32 visible_widgets_;

  // Does this process have backgrounded priority.
  bool backgrounded_;

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  // The filter for GPU-related messages coming from the renderer.
  // Thread safety note: this field is to be accessed from the UI thread.
  // We don't keep a reference to it, to avoid it being destroyed on the UI
  // thread, but we clear this field when we clear channel_. When channel_ goes
  // away, it posts a task to the IO thread to destroy it there, so we know that
  // it's valid if non-NULL.
  GpuMessageFilter* gpu_message_filter_;

  // The filter for MessagePort messages coming from the renderer.
  scoped_refptr<MessagePortMessageFilter> message_port_message_filter_;

  // The filter for Web Notification messages coming from the renderer. Holds a
  // closure per notification that must be freed when the notification closes.
  scoped_refptr<NotificationMessageFilter> notification_message_filter_;

  // Used in single-process mode.
  scoped_ptr<base::Thread> in_process_renderer_;

  // True after Init() has been called. We can't just check channel_ because we
  // also reset that in the case of process termination.
  bool is_initialized_;

  // PlzNavigate
  // Stores the time at which the first call to Init happened.
  base::TimeTicks init_time_;

  // Used to launch and terminate the process without blocking the UI thread.
  scoped_ptr<ChildProcessLauncher> child_process_launcher_;

  // Messages we queue while waiting for the process handle.  We queue them here
  // instead of in the channel so that we ensure they're sent after init related
  // messages that are sent once the process handle is available.  This is
  // because the queued messages may have dependencies on the init messages.
  std::queue<IPC::Message*> queued_messages_;

  // The globally-unique identifier for this RPH.
  int id_;

  BrowserContext* browser_context_;

  // Owned by |browser_context_|.
  StoragePartitionImpl* storage_partition_impl_;

  // The observers watching our lifetime.
  ObserverList<RenderProcessHostObserver> observers_;

  // True if the process can be shut down suddenly. If this is true, then it's
  // sure that all the RenderFrames in the process can be shutdown suddenly. If
  // it's false, then specific RenderFrames might still be allowed to be
  // shutdown suddenly by checking their SuddenTerminationAllowed() flag. This
  // can occur when a RenderFrame has an unload/beforeUnload event listener
  // registered but another RenderFrame in the same process doesn't.
  bool sudden_termination_allowed_;

  // Set to true if we shouldn't send input events.  We actually do the
  // filtering for this at the render widget level.
  bool ignore_input_events_;

  // Records the last time we regarded the child process active.
  base::TimeTicks child_process_activity_time_;

  // Indicates whether this is a RenderProcessHost of a Browser Plugin guest
  // renderer.
  bool is_isolated_guest_;

  // Forwards messages between WebRTCInternals in the browser process
  // and PeerConnectionTracker in the renderer process.
  scoped_refptr<PeerConnectionTrackerHost> peer_connection_tracker_host_;

  // Prevents the class from being added as a GpuDataManagerImpl observer more
  // than once.
  bool gpu_observer_registered_;

  // Set if a call to Cleanup is required once the RenderProcessHostImpl is no
  // longer within the RenderProcessHostObserver::RenderProcessExited callbacks.
  bool delayed_cleanup_needed_;

  // Indicates whether RenderProcessHostImpl is currently iterating and calling
  // through RenderProcessHostObserver::RenderProcessExited.
  bool within_process_died_observer_;

  // Forwards power state messages to the renderer process.
  PowerMonitorMessageBroadcaster power_monitor_broadcaster_;

  scoped_refptr<AudioRendererHost> audio_renderer_host_;

#if defined(OS_ANDROID)
  scoped_refptr<BrowserDemuxerAndroid> browser_demuxer_android_;
#endif

#if defined(ENABLE_BROWSER_CDMS)
  scoped_refptr<BrowserCdmManager> browser_cdm_manager_;
#endif

#if defined(ENABLE_WEBRTC)
  base::Callback<void(const std::string&)> webrtc_log_message_callback_;

  scoped_refptr<P2PSocketDispatcherHost> p2p_socket_dispatcher_host_;

  // Must be accessed on UI thread.
  std::vector<int> aec_dump_consumers_;

  WebRtcStopRtpDumpCallback stop_rtp_dump_callback_;
#endif

  int worker_ref_count_;

  // Records the time when the process starts surviving for workers for UMA.
  base::TimeTicks survive_for_worker_start_time_;

  // Records the maximum # of workers simultaneously hosted in this process
  // for UMA.
  int max_worker_count_;

  // Context shared for each PermissionService instance created for this RPH.
  scoped_ptr<PermissionServiceContext> permission_service_context_;

  // This is a set of all subscription targets valuebuffers in the GPU process
  // are currently subscribed too. Used to prevent sending unnecessary
  // ValueState updates.
  typedef base::hash_set<unsigned int> SubscriptionSet;
  SubscriptionSet subscription_set_;

  // Maintains ValueStates which are not currently subscribed too so we can
  // pass them to the GpuService if a Valuebuffer ever subscribes to the
  // respective subscription target
  scoped_refptr<gpu::ValueStateMap> pending_valuebuffer_state_;

  // Whether or not the CHROMIUM_subscribe_uniform WebGL extension is enabled
  bool subscribe_uniform_enabled_;

  base::WeakPtrFactory<RenderProcessHostImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_IMPL_H_
