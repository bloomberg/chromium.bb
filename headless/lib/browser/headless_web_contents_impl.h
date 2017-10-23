// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_WEB_CONTENTS_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_WEB_CONTENTS_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/observer_list.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "headless/lib/browser/headless_window_tree_host.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_export.h"
#include "headless/public/headless_web_contents.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/compositor/external_begin_frame_client.h"

class SkBitmap;

namespace content {
class DevToolsAgentHost;
class WebContents;
}

namespace gfx {
class Rect;
}

namespace headless {
class HeadlessBrowser;
class HeadlessBrowserImpl;
class HeadlessTabSocketImpl;

// Exported for tests.
class HEADLESS_EXPORT HeadlessWebContentsImpl
    : public HeadlessWebContents,
      public HeadlessDevToolsTarget,
      public content::DevToolsAgentHostObserver,
      public content::RenderProcessHostObserver,
      public content::WebContentsObserver,
      public ui::ExternalBeginFrameClient {
 public:
  ~HeadlessWebContentsImpl() override;

  static HeadlessWebContentsImpl* From(HeadlessWebContents* web_contents);
  static HeadlessWebContentsImpl* From(HeadlessBrowser* browser,
                                       content::WebContents* contents);

  static std::unique_ptr<HeadlessWebContentsImpl> Create(
      HeadlessWebContents::Builder* builder);

  // Takes ownership of |child_contents|.
  static std::unique_ptr<HeadlessWebContentsImpl> CreateForChildContents(
      HeadlessWebContentsImpl* parent,
      content::WebContents* child_contents);

  // HeadlessWebContents implementation:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  HeadlessDevToolsTarget* GetDevToolsTarget() override;
  HeadlessTabSocket* GetHeadlessTabSocket() const override;
  int GetMainFrameRenderProcessId() const override;
  int GetMainFrameTreeNodeId() const override;
  std::string GetMainFrameDevToolsId() const override;

  // HeadlessDevToolsTarget implementation:
  bool AttachClient(HeadlessDevToolsClient* client) override;
  void ForceAttachClient(HeadlessDevToolsClient* client) override;
  void DetachClient(HeadlessDevToolsClient* client) override;
  bool IsAttached() override;

  // content::DevToolsAgentHostObserver implementation:
  void DevToolsAgentHostAttached(
      content::DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDetached(
      content::DevToolsAgentHost* agent_host) override;

  // content::RenderProcessHostObserver implementation:
  void RenderProcessExited(content::RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // content::WebContentsObserver implementation:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderViewReady() override;
  void DidReceiveCompositorFrame() override;
  void OnInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;

  // ui::ExternalBeginFrameClient implementation:
  void OnDisplayDidFinishFrame(const viz::BeginFrameAck& ack) override;
  void OnNeedsExternalBeginFrames(bool needs_begin_frames) override;

  content::WebContents* web_contents() const;
  bool OpenURL(const GURL& url);

  void Close() override;

  std::string GetDevToolsAgentHostId();

  HeadlessBrowserImpl* browser() const;
  HeadlessBrowserContextImpl* browser_context() const;

  void set_window_tree_host(std::unique_ptr<HeadlessWindowTreeHost> host) {
    window_tree_host_ = std::move(host);
  }
  HeadlessWindowTreeHost* window_tree_host() const {
    return window_tree_host_.get();
  }
  int window_id() const { return window_id_; }
  void set_window_state(const std::string& state) {
    DCHECK(state == "normal" || state == "minimized" || state == "maximized" ||
           state == "fullscreen");
    window_state_ = state;
  }
  const std::string& window_state() const { return window_state_; }

  // Set bounds of WebContent's platform window.
  void SetBounds(const gfx::Rect& bounds);

  void CreateTabSocketMojoService(mojo::ScopedMessagePipeHandle handle);

  bool begin_frame_control_enabled() const {
    return begin_frame_control_enabled_;
  }

  bool needs_external_begin_frames() const {
    return needs_external_begin_frames_;
  }

  void SetBeginFrameEventsEnabled(int session_id, bool enabled);

  using FrameFinishedCallback =
      base::Callback<void(bool /* has_damage */,
                          bool /* main_frame_content_updated */,
                          std::unique_ptr<SkBitmap>)>;
  void BeginFrame(const base::TimeTicks& frame_timeticks,
                  const base::TimeTicks& deadline,
                  const base::TimeDelta& interval,
                  bool capture_screenshot,
                  const FrameFinishedCallback& frame_finished_callback);
  bool HasPendingFrame() const { return !pending_frames_.empty(); }

 private:
  struct PendingFrame;

  // Takes ownership of |web_contents|.
  HeadlessWebContentsImpl(content::WebContents* web_contents,
                          HeadlessBrowserContextImpl* browser_context);

  void InitializeWindow(const gfx::Rect& initial_bounds);

  using MojoService = HeadlessWebContents::Builder::MojoService;
  void CreateMojoService(
      const MojoService::ServiceFactoryCallback& service_factory,
      mojo::ScopedMessagePipeHandle handle);

  void SendNeedsBeginFramesEvent(int session_id);
  void PendingFrameReadbackComplete(PendingFrame* pending_frame,
                                    const SkBitmap& bitmap,
                                    content::ReadbackResponse response);

  uint32_t begin_frame_source_id_ = viz::BeginFrameArgs::kManualSourceId;
  uint64_t begin_frame_sequence_number_ =
      viz::BeginFrameArgs::kStartingFrameNumber;
  bool begin_frame_control_enabled_ = false;
  std::list<int> begin_frame_events_enabled_sessions_;
  bool needs_external_begin_frames_ = false;
  std::list<std::unique_ptr<PendingFrame>> pending_frames_;
  bool first_compositor_frame_received_ = false;

  class Delegate;
  std::unique_ptr<Delegate> web_contents_delegate_;
  std::unique_ptr<HeadlessWindowTreeHost> window_tree_host_;
  int window_id_ = 0;
  std::string window_state_;
  std::unique_ptr<HeadlessTabSocketImpl> headless_tab_socket_;
  std::unique_ptr<content::WebContents> web_contents_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::list<MojoService> mojo_services_;
  bool inject_mojo_services_into_isolated_world_;

  HeadlessBrowserContextImpl* browser_context_;      // Not owned.
  // TODO(alexclarke): With OOPIF there may be more than one renderer, we need
  // to fix this. See crbug.com/715924
  content::RenderProcessHost* render_process_host_;  // Not owned.

  base::ObserverList<HeadlessWebContents::Observer> observers_;

  service_manager::BinderRegistry registry_;

  base::WeakPtrFactory<HeadlessWebContentsImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessWebContentsImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_WEB_CONTENTS_IMPL_H_
