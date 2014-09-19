// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_

#include <vector>

#include "base/basictypes.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_view_host.h"
#include "ui/base/page_transition_types.h"

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {

class TestRenderFrameHostCreationObserver : public WebContentsObserver {
 public:
  explicit TestRenderFrameHostCreationObserver(WebContents* web_contents);
  virtual ~TestRenderFrameHostCreationObserver();

  // WebContentsObserver implementation.
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) OVERRIDE;

  RenderFrameHost* last_created_frame() const { return last_created_frame_; }

 private:
  RenderFrameHost* last_created_frame_;
};

class TestRenderFrameHost : public RenderFrameHostImpl,
                            public RenderFrameHostTester {
 public:
  TestRenderFrameHost(RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int routing_id,
                      bool is_swapped_out);
  virtual ~TestRenderFrameHost();

  // RenderFrameHostImpl overrides (same values, but in Test* types)
  virtual TestRenderViewHost* GetRenderViewHost() OVERRIDE;

  // RenderFrameHostTester implementation.
  virtual TestRenderFrameHost* AppendChild(
      const std::string& frame_name) OVERRIDE;
  virtual void SendNavigateWithTransition(
      int page_id,
      const GURL& url,
      ui::PageTransition transition) OVERRIDE;

  void SendNavigate(int page_id, const GURL& url);
  void SendFailedNavigate(int page_id, const GURL& url);
  void SendNavigateWithTransitionAndResponseCode(
      int page_id,
      const GURL& url, ui::PageTransition transition,
      int response_code);
  void SendNavigateWithOriginalRequestURL(
      int page_id,
      const GURL& url,
      const GURL& original_request_url);
  void SendNavigateWithFile(
      int page_id,
      const GURL& url,
      const base::FilePath& file_path);
  void SendNavigateWithParams(
      FrameHostMsg_DidCommitProvisionalLoad_Params* params);
  void SendNavigateWithRedirects(
      int page_id,
      const GURL& url,
      const std::vector<GURL>& redirects);
  void SendNavigateWithParameters(
      int page_id,
      const GURL& url,
      ui::PageTransition transition,
      const GURL& original_request_url,
      int response_code,
      const base::FilePath* file_path_for_history_item,
      const std::vector<GURL>& redirects);
  void SendBeginNavigationWithURL(const GURL& url);

  void DidDisownOpener();

  void set_contents_mime_type(const std::string& mime_type) {
    contents_mime_type_ = mime_type;
  }

  // If set, navigations will appear to have cleared the history list in the
  // RenderFrame
  // (FrameHostMsg_DidCommitProvisionalLoad_Params::history_list_was_cleared).
  // False by default.
  void set_simulate_history_list_was_cleared(bool cleared) {
    simulate_history_list_was_cleared_ = cleared;
  }

  // TODO(nick): As necessary for testing, override behavior of RenderFrameHost
  // here.

 private:
  TestRenderFrameHostCreationObserver child_creation_observer_;

  std::string contents_mime_type_;

  // See set_simulate_history_list_was_cleared() above.
  bool simulate_history_list_was_cleared_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderFrameHost);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_
