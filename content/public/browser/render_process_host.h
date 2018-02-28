// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <list>

#include "base/containers/id_map.h"
#include "base/memory/ptr_util.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/bind_interface_helpers.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sender.h"
#include "media/media_features.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace base {
class SharedPersistentMemoryAllocator;
class TimeDelta;
}

namespace service_manager {
class Identity;
}

namespace resource_coordinator {
class ProcessResourceCoordinator;
}

namespace viz {
class SharedBitmapAllocationNotifierImpl;
}

namespace content {
class BrowserContext;
class BrowserMessageFilter;
class RenderProcessHostObserver;
class RenderWidgetHost;
class RendererAudioOutputStreamFactoryContext;
class StoragePartition;
struct GlobalRequestID;

#if defined(OS_ANDROID)
enum class ChildProcessImportance;
#endif

namespace mojom {
class Renderer;
}

// Interface that represents the browser side of the browser <-> renderer
// communication channel. There will generally be one RenderProcessHost per
// renderer process.
class CONTENT_EXPORT RenderProcessHost : public IPC::Sender,
                                         public IPC::Listener,
                                         public base::SupportsUserData {
 public:
  using iterator = base::IDMap<RenderProcessHost*>::iterator;

  // Details for RENDERER_PROCESS_CLOSED notifications.
  struct RendererClosedDetails {
    RendererClosedDetails(base::TerminationStatus status,
                          int exit_code) {
      this->status = status;
      this->exit_code = exit_code;
    }
    base::TerminationStatus status;
    int exit_code;
  };

  // Crash reporting mode for ShutdownForBadMessage.
  enum class CrashReportMode {
    NO_CRASH_DUMP,
    GENERATE_CRASH_DUMP,
  };

  // General functions ---------------------------------------------------------

  ~RenderProcessHost() override {}

  // Initialize the new renderer process, returning true on success. This must
  // be called once before the object can be used, but can be called after
  // that with no effect. Therefore, if the caller isn't sure about whether
  // the process has been created, it should just call Init().
  virtual bool Init() = 0;

  // Ensures that a Channel exists and is at least queueing outgoing messages
  // if there isn't a render process connected to it yet. This may be used to
  // ensure that in the event of a renderer crash and restart, subsequent
  // messages sent via Send() will eventually reach the new process.
  virtual void EnableSendQueue() = 0;

  // Gets the next available routing id.
  virtual int GetNextRoutingID() = 0;

  // These methods add or remove listener for a specific message routing ID.
  // Used for refcounting, each holder of this object must AddRoute and
  // RemoveRoute. This object should be allocated on the heap; when no
  // listeners own it any more, it will delete itself.
  virtual void AddRoute(int32_t routing_id, IPC::Listener* listener) = 0;
  virtual void RemoveRoute(int32_t routing_id) = 0;

  // Add and remove observers for lifecycle events. The order in which
  // notifications are sent to observers is undefined. Observers must be sure to
  // remove the observer before they go away.
  virtual void AddObserver(RenderProcessHostObserver* observer) = 0;
  virtual void RemoveObserver(RenderProcessHostObserver* observer) = 0;

  // Called when a received message cannot be decoded. Terminates the renderer.
  // Most callers should not call this directly, but instead should call
  // bad_message::BadMessageReceived() or an equivalent method outside of the
  // content module.
  //
  // If |crash_report_mode| is GENERATE_CRASH_DUMP, then a browser crash dump
  // will be reported as well.
  virtual void ShutdownForBadMessage(CrashReportMode crash_report_mode) = 0;

  // Track the count of visible widgets. Called by listeners to register and
  // unregister visibility.
  virtual void WidgetRestored() = 0;
  virtual void WidgetHidden() = 0;
  virtual int VisibleWidgetCount() const = 0;

  virtual RendererAudioOutputStreamFactoryContext*
  GetRendererAudioOutputStreamFactoryContext() = 0;

  // Called when a video capture stream or an audio stream is added or removed
  // and used to determine if the process should be backgrounded or not.
  virtual void OnMediaStreamAdded() = 0;
  virtual void OnMediaStreamRemoved() = 0;

  // Indicates whether the current RenderProcessHost is exclusively hosting
  // guest RenderFrames. Not all guest RenderFrames are created equal.  A guest,
  // as indicated by BrowserPluginGuest::IsGuest, may coexist with other
  // non-guest RenderFrames in the same process if IsForGuestsOnly() is false.
  virtual bool IsForGuestsOnly() const = 0;

  // Returns the storage partition associated with this process.
  virtual StoragePartition* GetStoragePartition() const = 0;

  // Try to shut down the associated renderer process without running unload
  // handlers, etc, giving it the specified exit code. If |wait| is true, wait
  // for the process to be actually terminated before returning.  Returns true
  // if it was able to shut down.  On Windows, this must not be called before
  // RenderProcessReady was called on a RenderProcessHostObserver, otherwise
  // RenderProcessExited may never be called.
  virtual bool Shutdown(int exit_code, bool wait) = 0;

  // Try to shut down the associated renderer process as fast as possible.
  // If a non-zero |page_count| value is provided, then a fast shutdown will
  // only happen if the count matches the active view count. If
  // |skip_unload_handlers| is false and this renderer has any RenderViews with
  // unload handlers, then this function does nothing. Otherwise, the function
  // will ingnore checking for those handlers. Returns true if it was able to do
  // fast shutdown.
  virtual bool FastShutdownIfPossible(size_t page_count = 0,
                                      bool skip_unload_handlers = false) = 0;

  // Returns true if fast shutdown was started for the renderer.
  virtual bool FastShutdownStarted() const = 0;

  // Returns the process object associated with the child process.  In certain
  // tests or single-process mode, this will actually represent the current
  // process.
  //
  // NOTE: this is not necessarily valid immediately after calling Init, as
  // Init starts the process asynchronously.  It's guaranteed to be valid after
  // the first IPC arrives or RenderProcessReady was called on a
  // RenderProcessHostObserver for this. At that point, IsReady() returns true.
  virtual base::ProcessHandle GetHandle() const = 0;

  // Returns whether the process is ready. The process is ready once both
  // conditions (which can happen in arbitrary order) are true:
  // 1- the launcher reported a successful launch
  // 2- the channel is connected.
  //
  // After that point, GetHandle() is valid, and deferred messages have been
  // sent.
  virtual bool IsReady() const = 0;

  // Returns the user browser context associated with this renderer process.
  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Returns whether this process is using the same StoragePartition as
  // |partition|.
  virtual bool InSameStoragePartition(StoragePartition* partition) const = 0;

  // Returns the unique ID for this child process host. This can be used later
  // in a call to FromID() to get back to this object (this is used to avoid
  // sending non-threadsafe pointers to other threads).
  //
  // This ID will be unique across all child process hosts, including workers,
  // plugins, etc.
  //
  // This will never return ChildProcessHost::kInvalidUniqueID.
  virtual int GetID() const = 0;

  // Returns true iff channel_ has been set to non-nullptr. Use this for
  // checking if there is connection or not. Virtual for mocking out for tests.
  virtual bool HasConnection() const = 0;

  // Returns the renderer channel.
  virtual IPC::ChannelProxy* GetChannel() = 0;

  // Adds a message filter to the IPC channel.
  virtual void AddFilter(BrowserMessageFilter* filter) = 0;

  // Sets whether input events should be ignored for this process.
  virtual void SetIgnoreInputEvents(bool ignore_input_events) = 0;
  virtual bool IgnoreInputEvents() const = 0;

  // Schedules the host for deletion and removes it from the all_hosts list.
  virtual void Cleanup() = 0;

  // Track the count of pending views that are being swapped back in.  Called
  // by listeners to register and unregister pending views to prevent the
  // process from exiting.
  virtual void AddPendingView() = 0;
  virtual void RemovePendingView() = 0;

  // Adds and removes the widgets owned by this process.
  virtual void AddWidget(RenderWidgetHost* widget) = 0;
  virtual void RemoveWidget(RenderWidgetHost* widget) = 0;

#if defined(OS_ANDROID)
  // Called by an already added widget when its importance changes.
  virtual void UpdateWidgetImportance(ChildProcessImportance old_value,
                                      ChildProcessImportance new_value) = 0;

  // Return the highest importance of all widgets in this process.
  virtual ChildProcessImportance ComputeEffectiveImportance() = 0;
#endif

  // Sets a flag indicating that the process can be abnormally terminated.
  virtual void SetSuddenTerminationAllowed(bool allowed) = 0;
  // Returns true if the process can be abnormally terminated.
  virtual bool SuddenTerminationAllowed() const = 0;

  // Returns how long the child has been idle. The definition of idle
  // depends on when a derived class calls mark_child_process_activity_time().
  // This is a rough indicator and its resolution should not be better than
  // 10 milliseconds.
  virtual base::TimeDelta GetChildProcessIdleTime() const = 0;

  // Checks that the given renderer can request |url|, if not it sets it to
  // about:blank.
  // |empty_allowed| must be set to false for navigations for security reasons.
  virtual void FilterURL(bool empty_allowed, GURL* url) = 0;

#if BUILDFLAG(ENABLE_WEBRTC)
  virtual void EnableAudioDebugRecordings(const base::FilePath& file) = 0;
  virtual void DisableAudioDebugRecordings() = 0;

  // Enables or disables WebRTC's echo canceller AEC3. Disabled implies
  // selecting the older AEC2. The operation is asynchronous, |callback| is run
  // when done with the boolean indicating if successful and an error message.
  // The error message is empty if successful.
  // TODO(crbug.com/696930): Remove once the AEC3 is fully rolled out and the
  // old AEC is deprecated.
  virtual void SetEchoCanceller3(
      bool enable,
      base::OnceCallback<void(bool /* success */,
                              const std::string& /* error_message */)>
          callback) = 0;

  using WebRtcRtpPacketCallback =
      base::Callback<void(std::unique_ptr<uint8_t[]> packet_header,
                          size_t header_length,
                          size_t packet_length,
                          bool incoming)>;

  using WebRtcStopRtpDumpCallback =
      base::Callback<void(bool incoming, bool outgoing)>;

  // Starts passing RTP packets to |packet_callback| and returns the callback
  // used to stop dumping.
  virtual WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) = 0;
#endif

  // Tells the ResourceDispatcherHost to resume a deferred navigation without
  // transferring it to a new renderer process.
  virtual void ResumeDeferredNavigation(const GlobalRequestID& request_id) = 0;

  // Binds interfaces exposed to the browser process from the renderer.
  virtual void BindInterface(const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle interface_pipe) = 0;

  virtual const service_manager::Identity& GetChildIdentity() const = 0;

  // Extracts any persistent-memory-allocator used for renderer metrics.
  // Ownership is passed to the caller. To support sharing of histogram data
  // between the Renderer and the Browser, the allocator is created when the
  // process is created and later retrieved by the SubprocessMetricsProvider
  // for management.
  virtual std::unique_ptr<base::SharedPersistentMemoryAllocator>
  TakeMetricsAllocator() = 0;

  // PlzNavigate
  // Returns the time the first call to Init completed successfully (after a new
  // renderer process was created); further calls to Init won't change this
  // value.
  // Note: Do not use! Will disappear after PlzNavitate is completed.
  virtual const base::TimeTicks& GetInitTimeForNavigationMetrics() const = 0;

  // Returns true if this process currently has backgrounded priority.
  virtual bool IsProcessBackgrounded() const = 0;

  // "Keep alive ref count" represents the number of the customers of this
  // render process who wish the renderer process to be alive. While the ref
  // count is positive, |this| object will keep the renderer process alive,
  // unless DisableKeepAliveRefCount() is called.
  //
  // Here is the list of users:
  //  - Service Worker:
  //    While there are service workers who live in this process, they wish
  //    the renderer process to be alive. The ref count is incremented when this
  //    process is allocated to the worker, and decremented when worker's
  //    shutdown sequence is completed.
  //  - Shared Worker:
  //    While there are shared workers who live in this process, they wish
  //    the renderer process to be alive. The ref count is incremented when
  //    a shared worker is created in the process, and decremented when
  //    it is terminated (it self-destructs when it no longer has clients).
  //  - Keepalive request (if the KeepAliveRendererForKeepaliveRequests
  //    feature is enabled):
  //    When a fetch request with keepalive flag
  //    (https://fetch.spec.whatwg.org/#request-keepalive-flag) specified is
  //    pending, it wishes the renderer process to be kept alive.
  virtual void IncrementKeepAliveRefCount() = 0;
  virtual void DecrementKeepAliveRefCount() = 0;

  // Sets keep alive ref counts to zero. Called when the browser context will be
  // destroyed so this RenderProcessHost can immediately die.
  //
  // After this is called, the Increment/DecrementKeepAliveRefCount() functions
  // must not be called.
  virtual void DisableKeepAliveRefCount() = 0;

  // Returns true if DisableKeepAliveRefCount() was called.
  virtual bool IsKeepAliveRefCountDisabled() = 0;

  // Purges and suspends the renderer process.
  virtual void PurgeAndSuspend() = 0;

  // Resumes the renderer process.
  virtual void Resume() = 0;

  // Acquires the |mojom::Renderer| interface to the render process. This is for
  // internal use only, and is only exposed here to support
  // MockRenderProcessHost usage in tests.
  virtual mojom::Renderer* GetRendererInterface() = 0;

  // Acquires the interface to the Global Resource Coordinator for this process.
  virtual resource_coordinator::ProcessResourceCoordinator*
  GetProcessResourceCoordinator() = 0;

  // Whether this process is locked out from ever being reused for sites other
  // than the ones it currently has.
  virtual void SetIsNeverSuitableForReuse() = 0;
  virtual bool MayReuseHost() = 0;

  // Indicates whether this RenderProcessHost is "unused".  This starts out as
  // true for new processes and becomes false after one of the following:
  // (1) This process commits any page.
  // (2) This process is given to a SiteInstance that already has a site
  //     assigned.
  // Note that a process hosting ServiceWorkers will be implicitly handled by
  // (2) during ServiceWorker initialization, and SharedWorkers will be handled
  // by (1) since a page needs to commit before it can create a SharedWorker.
  //
  // While a process is unused, it is still suitable to host a URL that
  // requires a dedicated process.
  virtual bool IsUnused() = 0;
  virtual void SetIsUsed() = 0;

  // Return true if the host has not been used. This is stronger than IsUnused()
  // in that it checks if this RPH has ever been used to render at all, rather
  // than just no being suitable to host a URL that requires a dedicated
  // process.
  // TODO(alexmos): can this be unified with IsUnused()? See also
  // crbug.com/738634.
  virtual bool HostHasNotBeenUsed() = 0;

  // Returns the current number of active views in this process.  Excludes
  // any RenderViewHosts that are swapped out.
  size_t GetActiveViewCount();

  // Posts |task|, if this RenderProcessHost is ready or when it becomes ready
  // (see RenderProcessHost::IsReady method).  The |task| might not run at all
  // (e.g. if |render_process_host| is destroyed before becoming ready).  This
  // function can only be called on the browser's UI thread (and the |task| will
  // be posted back on the UI thread).
  void PostTaskWhenProcessIsReady(base::OnceClosure task);

  // Returns the SharedBitmapAllocationNotifier associated with this process.
  // SharedBitmapAllocationNotifier manages viz::SharedBitmaps created by this
  // process and can notify observers when a new SharedBitmap is allocated.
  virtual viz::SharedBitmapAllocationNotifierImpl*
  GetSharedBitmapAllocationNotifier() = 0;

  // Static management functions -----------------------------------------------

  // Possibly start an unbound, spare RenderProcessHost. A subsequent creation
  // of a RenderProcessHost with a matching browser_context may use this
  // preinitialized RenderProcessHost, improving performance.
  //
  // It is safe to call this multiple times or when it is not certain that the
  // spare renderer will be used, although calling this too eagerly may reduce
  // performance as unnecessary RenderProcessHosts are created. The spare
  // renderer will only be used if it using the default StoragePartition of a
  // matching BrowserContext.
  //
  // The spare RenderProcessHost is meant to be created in a situation where a
  // navigation is imminent and it is unlikely an existing RenderProcessHost
  // will be used, for example in a cross-site navigation when a Service Worker
  // will need to be started.
  static void WarmupSpareRenderProcessHost(
      content::BrowserContext* browser_context);

  // Flag to run the renderer in process.  This is primarily
  // for debugging purposes.  When running "in process", the
  // browser maintains a single RenderProcessHost which communicates
  // to a RenderProcess which is instantiated in the same process
  // with the Browser.  All IPC between the Browser and the
  // Renderer is the same, it's just not crossing a process boundary.
  static bool run_renderer_in_process();

  // This also calls out to ContentBrowserClient::GetApplicationLocale and
  // modifies the current process' command line.
  static void SetRunRendererInProcess(bool value);

  // Allows iteration over all the RenderProcessHosts in the browser. Note
  // that each host may not be active, and therefore may have nullptr channels.
  static iterator AllHostsIterator();

  // Returns the RenderProcessHost given its ID.  Returns nullptr if the ID does
  // not correspond to a live RenderProcessHost.
  static RenderProcessHost* FromID(int render_process_id);

  // Returns the RenderProcessHost given its renderer's service Identity.
  // Returns nullptr if the Identity does not correspond to a live
  // RenderProcessHost.
  static RenderProcessHost* FromRendererIdentity(
      const service_manager::Identity& identity);

  // Returns whether the process-per-site model is in use (globally or just for
  // the current site), in which case we should ensure there is only one
  // RenderProcessHost per site for the entire browser context.
  static bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                                      const GURL& url);

  // Returns true if the caller should attempt to use an existing
  // RenderProcessHost rather than creating a new one.
  static bool ShouldTryToUseExistingProcessHost(
      content::BrowserContext* browser_context, const GURL& site_url);

  // Get an existing RenderProcessHost associated with the given browser
  // context, if possible.  The renderer process is chosen randomly from
  // suitable renderers that share the same context and type (determined by the
  // site url).
  // Returns nullptr if no suitable renderer process is available, in which case
  // the caller is free to create a new renderer.
  static RenderProcessHost* GetExistingProcessHost(
      content::BrowserContext* browser_context, const GURL& site_url);

  // Overrides the default heuristic for limiting the max renderer process
  // count.  This is useful for unit testing process limit behaviors.  It is
  // also used to allow a command line parameter to configure the max number of
  // renderer processes and should only be called once during startup.
  // A value of zero means to use the default heuristic.
  static void SetMaxRendererProcessCount(size_t count);

  // Returns the current maximum number of renderer process hosts kept by the
  // content module.
  static size_t GetMaxRendererProcessCount();

  // TODO(siggi): Remove once https://crbug.com/806661 is resolved.
  using AnalyzeHungRendererFunction = void (*)(const base::Process& renderer);
  static void SetHungRendererAnalysisFunction(
      AnalyzeHungRendererFunction analyze_hung_renderer);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_
