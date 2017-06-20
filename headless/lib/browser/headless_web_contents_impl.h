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
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "headless/lib/browser/headless_window_tree_host.h"
#include "headless/lib/headless_render_frame_controller.mojom.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_export.h"
#include "headless/public/headless_web_contents.h"

namespace content {
class DevToolsAgentHost;
class WebContents;
}

namespace gfx {
class Rect;
}

namespace headless {
class HeadlessBrowserImpl;
class HeadlessTabSocketImpl;

// Exported for tests.
class HEADLESS_EXPORT HeadlessWebContentsImpl
    : public HeadlessWebContents,
      public HeadlessDevToolsTarget,
      public content::DevToolsAgentHostObserver,
      public content::RenderProcessHostObserver,
      public content::WebContentsObserver {
 public:
  ~HeadlessWebContentsImpl() override;

  static HeadlessWebContentsImpl* From(HeadlessWebContents* web_contents);

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
  std::string GetUntrustedDevToolsFrameIdForFrameTreeNodeId(
      int process_id,
      int frame_tree_node_id) const override;
  int GetMainFrameRenderProcessId() const override;

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

 private:
  // Takes ownership of |web_contents|.
  HeadlessWebContentsImpl(content::WebContents* web_contents,
                          HeadlessBrowserContextImpl* browser_context);

  void MainFrameTabSocketSetupComplete();
  void MaybeIssueDevToolsTargetReady();

  void InitializeWindow(const gfx::Rect& initial_bounds);

  using MojoService = HeadlessWebContents::Builder::MojoService;
  void CreateMojoService(
      const MojoService::ServiceFactoryCallback& service_factory,
      mojo::ScopedMessagePipeHandle handle);

  class Delegate;
  std::unique_ptr<Delegate> web_contents_delegate_;
  std::unique_ptr<HeadlessWindowTreeHost> window_tree_host_;
  int window_id_ = 0;
  std::string window_state_;
  std::unique_ptr<content::WebContents> web_contents_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::list<MojoService> mojo_services_;
  bool inject_mojo_services_into_isolated_world_;
  std::unique_ptr<HeadlessTabSocketImpl> headless_tab_socket_;
  bool render_view_ready_ = false;
  bool main_frame_tab_socket_setup_complete_ = false;

  HeadlessBrowserContextImpl* browser_context_;      // Not owned.
  // TODO(alexclarke): With OOPIF there may be more than one renderer, we need
  // to fix this. See crbug.com/715924
  content::RenderProcessHost* render_process_host_;  // Not owned.

  HeadlessRenderFrameControllerPtr render_frame_controller_;

  base::ObserverList<HeadlessWebContents::Observer> observers_;

  base::WeakPtrFactory<HeadlessWebContentsImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessWebContentsImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_WEB_CONTENTS_IMPL_H_
