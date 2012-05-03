// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/process.h"
#include "base/process_util.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/surface/transport_dib.h"

class GURL;
struct ViewMsg_SwapOut_Params;

namespace content {
class BrowserContext;
class RenderWidgetHost;
}

namespace base {
class TimeDelta;
}

namespace content {

// Interface that represents the browser side of the browser <-> renderer
// communication channel. There will generally be one RenderProcessHost per
// renderer process.
class CONTENT_EXPORT RenderProcessHost : public IPC::Message::Sender,
                                         public IPC::Channel::Listener {
 public:
  typedef IDMap<RenderProcessHost>::iterator iterator;
  typedef IDMap<RenderWidgetHost>::const_iterator RenderWidgetHostsIterator;

  // Details for RENDERER_PROCESS_CLOSED notifications.
  struct RendererClosedDetails {
    RendererClosedDetails(base::ProcessHandle handle,
                          base::TerminationStatus status,
                          int exit_code,
                          bool was_alive) {
      this->handle = handle;
      this->status = status;
      this->exit_code = exit_code;
      this->was_alive = was_alive;
    }
    base::ProcessHandle handle;
    base::TerminationStatus status;
    int exit_code;
    bool was_alive;
  };

  virtual ~RenderProcessHost() {}

  // Initialize the new renderer process, returning true on success. This must
  // be called once before the object can be used, but can be called after
  // that with no effect. Therefore, if the caller isn't sure about whether
  // the process has been created, it should just call Init().
  virtual bool Init() = 0;

  // Gets the next available routing id.
  virtual int GetNextRoutingID() = 0;

  // Called on the UI thread to cancel any outstanding resource requests for
  // the specified render widget.
  virtual void CancelResourceRequests(int render_widget_id) = 0;

  // Called on the UI thread to simulate a SwapOut_ACK message to the
  // ResourceDispatcherHost.  Necessary for a cross-site request, in the case
  // that the original RenderViewHost is not live and thus cannot run an
  // unload handler.
  virtual void SimulateSwapOutACK(const ViewMsg_SwapOut_Params& params) = 0;

  // Called to wait for the next UpdateRect message for the specified render
  // widget.  Returns true if successful, and the msg out-param will contain a
  // copy of the received UpdateRect message.
  virtual bool WaitForBackingStoreMsg(int render_widget_id,
                                      const base::TimeDelta& max_delay,
                                      IPC::Message* msg) = 0;

  // Called when a received message cannot be decoded.
  virtual void ReceivedBadMessage() = 0;

  // Track the count of visible widgets. Called by listeners to register and
  // unregister visibility.
  virtual void WidgetRestored() = 0;
  virtual void WidgetHidden() = 0;
  virtual int VisibleWidgetCount() const = 0;

  // Try to shutdown the associated renderer process as fast as possible.
  // If this renderer has any RenderViews with unload handlers, then this
  // function does nothing.  The current implementation uses TerminateProcess.
  // Returns True if it was able to do fast shutdown.
  virtual bool FastShutdownIfPossible() = 0;

  // Returns true if fast shutdown was started for the renderer.
  virtual bool FastShutdownStarted() const = 0;

  // Dump the child process' handle table before shutting down.
  virtual void DumpHandles() = 0;

  // Returns the process object associated with the child process.  In certain
  // tests or single-process mode, this will actually represent the current
  // process.
  //
  // NOTE: this is not necessarily valid immediately after calling Init, as
  // Init starts the process asynchronously.  It's guaranteed to be valid after
  // the first IPC arrives.
  virtual base::ProcessHandle GetHandle() = 0;

  // Transport DIB functions ---------------------------------------------------

  // Return the TransportDIB for the given id. On Linux, this can involve
  // mapping shared memory. On Mac, the shared memory is created in the browser
  // process and the cached metadata is returned. On Windows, this involves
  // duplicating the handle from the remote process.  The RenderProcessHost
  // still owns the returned DIB.
  virtual TransportDIB* GetTransportDIB(TransportDIB::Id dib_id) = 0;

  // Returns the user browser context associated with this renderer process.
  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Returns the unique ID for this child process. This can be used later in
  // a call to FromID() to get back to this object (this is used to avoid
  // sending non-threadsafe pointers to other threads).
  //
  // This ID will be unique for all child processes, including workers, plugins,
  // etc.
  virtual int GetID() const = 0;

  // Returns the render widget host for the routing id passed in.
  virtual RenderWidgetHost* GetRenderWidgetHostByID(int routing_id) = 0;

  // Returns true iff channel_ has been set to non-NULL. Use this for checking
  // if there is connection or not. Virtual for mocking out for tests.
  virtual bool HasConnection() const = 0;

  // Call this to allow queueing of IPC messages that are sent before the
  // process is launched.
  virtual void EnableSendQueue() = 0;

  // Returns the renderer channel.
  virtual IPC::ChannelProxy* GetChannel() = 0;

  // Returns the list of attached render widget hosts.
  virtual RenderWidgetHostsIterator GetRenderWidgetHostsIterator() = 0;

  // Try to shutdown the associated render process as fast as possible
  virtual bool FastShutdownForPageCount(size_t count) = 0;

  // TODO(ananta)
  // Revisit whether the virtual functions declared from here on need to be
  // part of the interface.
  virtual void SetIgnoreInputEvents(bool ignore_input_events) = 0;
  virtual bool IgnoreInputEvents() const = 0;

  // Used for refcounting, each holder of this object must Attach and Release
  // just like it would for a COM object. This object should be allocated on
  // the heap; when no listeners own it any more, it will delete itself.
  virtual void Attach(content::RenderWidgetHost* host, int routing_id) = 0;

  // See Attach()
  virtual void Release(int routing_id) = 0;

  // Schedules the host for deletion and removes it from the all_hosts list.
  virtual void Cleanup() = 0;

  // Track the count of pending views that are being swapped back in.  Called
  // by listeners to register and unregister pending views to prevent the
  // process from exiting.
  virtual void AddPendingView() = 0;
  virtual void RemovePendingView() = 0;

  // Sets a flag indicating that the process can be abnormally terminated.
  virtual void SetSuddenTerminationAllowed(bool allowed) = 0;
  // Returns true if the process can be abnormally terminated.
  virtual bool SuddenTerminationAllowed() const = 0;

  // Returns how long the child has been idle. The definition of idle
  // depends on when a derived class calls mark_child_process_activity_time().
  // This is a rough indicator and its resolution should not be better than
  // 10 milliseconds.
  virtual base::TimeDelta GetChildProcessIdleTime() const = 0;

  // Signals that a compositing surface has been updated after a lost context
  // event, so that we can process requests from the renderer to create contexts
  // with that surface.
  virtual void SurfaceUpdated(int32 surface_id) = 0;

  // Static management functions -----------------------------------------------

  // Flag to run the renderer in process.  This is primarily
  // for debugging purposes.  When running "in process", the
  // browser maintains a single RenderProcessHost which communicates
  // to a RenderProcess which is instantiated in the same process
  // with the Browser.  All IPC between the Browser and the
  // Renderer is the same, it's just not crossing a process boundary.

  static bool run_renderer_in_process();
  static void set_run_renderer_in_process(bool value);

  // Allows iteration over all the RenderProcessHosts in the browser. Note
  // that each host may not be active, and therefore may have NULL channels.
  static iterator AllHostsIterator();

  // Returns the RenderProcessHost given its ID.  Returns NULL if the ID does
  // not correspond to a live RenderProcessHost.
  static RenderProcessHost* FromID(int render_process_id);

  // Returns true if the caller should attempt to use an existing
  // RenderProcessHost rather than creating a new one.
  static bool ShouldTryToUseExistingProcessHost(
      content::BrowserContext* browser_context, const GURL& site_url);

  // Get an existing RenderProcessHost associated with the given browser
  // context, if possible.  The renderer process is chosen randomly from
  // suitable renderers that share the same context and type (determined by the
  // site url).
  // Returns NULL if no suitable renderer process is available, in which case
  // the caller is free to create a new renderer.
  static RenderProcessHost* GetExistingProcessHost(
      content::BrowserContext* browser_context, const GURL& site_url);

  // Overrides the default heuristic for limiting the max renderer process
  // count.  This is useful for unit testing process limit behaviors.  It is
  // also used to allow a command line parameter to configure the max number of
  // renderer processes and should only be called once during startup.
  // A value of zero means to use the default heuristic.
  static void SetMaxRendererProcessCount(size_t count);

  // Returns the current max number of renderer processes used by the content
  // module.
  static size_t GetMaxRendererProcessCount();
};

}  // namespace content.

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_H_
