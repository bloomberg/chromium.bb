// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_IMPL_H_
#pragma once

#include <map>
#include <queue>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/timer.h"
#include "content/browser/child_process_launcher.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/surface/transport_dib.h"

class CommandLine;
class GpuMessageFilter;
class RendererMainThread;
class RenderWidgetHelper;

namespace base {
class WaitableEvent;
}

namespace content {
class RenderWidgetHost;
class RenderWidgetHostImpl;
}

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
class CONTENT_EXPORT RenderProcessHostImpl
    : public content::RenderProcessHost,
      public ChildProcessLauncher::Client,
      public base::WaitableEventWatcher::Delegate {
 public:
  explicit RenderProcessHostImpl(content::BrowserContext* browser_context);
  virtual ~RenderProcessHostImpl();

  // RenderProcessHost implementation (public portion).
  virtual void EnableSendQueue() OVERRIDE;
  virtual bool Init() OVERRIDE;
  virtual int GetNextRoutingID() OVERRIDE;
  virtual void CancelResourceRequests(int render_widget_id) OVERRIDE;
  virtual void CrossSiteSwapOutACK(const ViewMsg_SwapOut_Params& params)
      OVERRIDE;
  virtual bool WaitForBackingStoreMsg(int render_widget_id,
                                      const base::TimeDelta& max_delay,
                                      IPC::Message* msg) OVERRIDE;
  virtual void ReceivedBadMessage() OVERRIDE;
  virtual void WidgetRestored() OVERRIDE;
  virtual void WidgetHidden() OVERRIDE;
  virtual int VisibleWidgetCount() const OVERRIDE;
  virtual bool FastShutdownIfPossible() OVERRIDE;
  virtual void DumpHandles() OVERRIDE;
  virtual base::ProcessHandle GetHandle() OVERRIDE;
  virtual TransportDIB* GetTransportDIB(TransportDIB::Id dib_id) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual int GetID() const OVERRIDE;
  virtual bool HasConnection() const OVERRIDE;
  virtual content::RenderWidgetHost* GetRenderWidgetHostByID(int routing_id)
      OVERRIDE;
  virtual void SetIgnoreInputEvents(bool ignore_input_events) OVERRIDE;
  virtual bool IgnoreInputEvents() const OVERRIDE;
  virtual void Attach(content::RenderWidgetHost* host, int routing_id)
      OVERRIDE;
  virtual void Release(int routing_id) OVERRIDE;
  virtual void Cleanup() OVERRIDE;
  virtual void AddPendingView() OVERRIDE;
  virtual void RemovePendingView() OVERRIDE;
  virtual void SetSuddenTerminationAllowed(bool enabled) OVERRIDE;
  virtual bool SuddenTerminationAllowed() const OVERRIDE;
  virtual IPC::ChannelProxy* GetChannel() OVERRIDE;
  virtual RenderWidgetHostsIterator GetRenderWidgetHostsIterator() OVERRIDE;
  virtual bool FastShutdownForPageCount(size_t count) OVERRIDE;
  virtual bool FastShutdownStarted() const OVERRIDE;
  virtual base::TimeDelta GetChildProcessIdleTime() const OVERRIDE;
  virtual void SurfaceUpdated(int32 surface_id) OVERRIDE;

  // IPC::Channel::Sender via RenderProcessHost.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Channel::Listener via RenderProcessHost.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched() OVERRIDE;

  // base::WaitableEventWatcher::Delegate implementation.
  virtual void OnWaitableEventSignaled(
      base::WaitableEvent* waitable_event) OVERRIDE;

  // Call this function when it is evident that the child process is actively
  // performing some operation, for example if we just received an IPC message.
  void mark_child_process_activity_time() {
    child_process_activity_time_ = base::TimeTicks::Now();
  }

  // Register/unregister the host identified by the host id in the global host
  // list.
  static void RegisterHost(int host_id, content::RenderProcessHost* host);
  static void UnregisterHost(int host_id);

  // Returns true if the given host is suitable for launching a new view
  // associated with the given browser context.
  static bool IsSuitableHost(content::RenderProcessHost* host,
                             content::BrowserContext* browser_context,
                             const GURL& site_url);

 protected:
  // A proxy for our IPC::Channel that lives on the IO thread (see
  // browser_process.h)
  scoped_ptr<IPC::ChannelProxy> channel_;

  // The registered render widget hosts. When this list is empty or all NULL,
  // we should delete ourselves
  IDMap<content::RenderWidgetHost> render_widget_hosts_;

  // True if fast shutdown has been performed on this RPH.
  bool fast_shutdown_started_;

  // True if we've posted a DeleteTask and will be deleted soon.
  bool deleting_soon_;

  // The count of currently swapped out but pending RenderViews.  We have
  // started to swap these in, so the renderer process should not exit if
  // this count is non-zero.
  int32 pending_views_;

 private:
  friend class VisitRelayingRenderProcessHost;

  // Creates and adds the IO thread message filters.
  void CreateMessageFilters();

  // Control message handlers.
  void OnShutdownRequest();
  void OnDumpHandlesDone();
  void SuddenTerminationChanged(bool enabled);
  void OnUserMetricsRecordAction(const std::string& action);
  void OnSavedPageAsMHTML(int job_id, int64 mhtml_file_size);

  // CompositorSurfaceBuffersSwapped handler when there's no RWH.
  void OnCompositorSurfaceBuffersSwappedNoHost(int32 surface_id,
                                               uint64 surface_handle,
                                               int32 route_id,
                                               int32 gpu_process_host_id);

  // Generates a command line to be used to spawn a renderer and appends the
  // results to |*command_line|.
  void AppendRendererCommandLine(CommandLine* command_line) const;

  // Copies applicable command line switches from the given |browser_cmd| line
  // flags to the output |renderer_cmd| line flags. Not all switches will be
  // copied over.
  void PropagateBrowserCommandLineToRenderer(const CommandLine& browser_cmd,
                                             CommandLine* renderer_cmd) const;

  // Callers can reduce the RenderProcess' priority.
  void SetBackgrounded(bool backgrounded);

  // Handle termination of our process. |was_alive| indicates that when we
  // tried to retrieve the exit code the process had not finished yet.
  void ProcessDied(base::ProcessHandle handle,
                   base::TerminationStatus status,
                   int exit_code,
                   bool was_alive);

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

  // A map of transport DIB ids to cached TransportDIBs
  std::map<TransportDIB::Id, TransportDIB*> cached_dibs_;

  enum {
    // This is the maximum size of |cached_dibs_|
    MAX_MAPPED_TRANSPORT_DIBS = 3,
  };

  // Map a transport DIB from its Id and return it. Returns NULL on error.
  TransportDIB* MapTransportDIB(TransportDIB::Id dib_id);

  void ClearTransportDIBCache();
  // This is used to clear our cache five seconds after the last use.
  base::DelayTimer<RenderProcessHostImpl> cached_dibs_cleaner_;

  // Used in single-process mode.
  scoped_ptr<RendererMainThread> in_process_renderer_;

  // True after Init() has been called. We can't just check channel_ because we
  // also reset that in the case of process termination.
  bool is_initialized_;

  // Used to launch and terminate the process without blocking the UI thread.
  scoped_ptr<ChildProcessLauncher> child_process_launcher_;

  // Messages we queue while waiting for the process handle.  We queue them here
  // instead of in the channel so that we ensure they're sent after init related
  // messages that are sent once the process handle is available.  This is
  // because the queued messages may have dependencies on the init messages.
  std::queue<IPC::Message*> queued_messages_;

#if defined(OS_WIN)
  // Used to wait until the renderer dies to get an accurrate exit code.
  base::WaitableEventWatcher child_process_watcher_;
#endif

  // The globally-unique identifier for this RPH.
  int id_;

  content::BrowserContext* browser_context_;

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

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostImpl);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_IMPL_H_
