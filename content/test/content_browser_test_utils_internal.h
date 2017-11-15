// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_
#define CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_

// A collection of functions designed for use with content_shell based browser
// tests internal to the content/ module.
// Note: If a function here also works with browser_tests, it should be in
// the content public API.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;
class RenderFrameHost;
class Shell;
class SiteInstance;
class ToRenderFrameHost;
struct ScreenInfo;

// Navigates the frame represented by |node| to |url|, blocking until the
// navigation finishes.
void NavigateFrameToURL(FrameTreeNode* node, const GURL& url);

// Sets the DialogManager to proceed by default or not when showing a
// BeforeUnload dialog.
void SetShouldProceedOnBeforeUnload(Shell* shell, bool proceed);

// Extends the ToRenderFrameHost mechanism to FrameTreeNodes.
RenderFrameHost* ConvertToRenderFrameHost(FrameTreeNode* frame_tree_node);

// Creates compact textual representations of the state of the frame tree that
// is appropriate for use in assertions.
//
// The diagrams show frame tree structure, the SiteInstance of current frames,
// presence of pending frames, and the SiteInstances of any and all proxies.
// They look like this:
//
//        Site A (D pending) -- proxies for B C
//          |--Site B --------- proxies for A C
//          +--Site C --------- proxies for B A
//               |--Site A ---- proxies for B
//               +--Site A ---- proxies for B
//                    +--Site A -- proxies for B
//       Where A = http://127.0.0.1/
//             B = http://foo.com/ (no process)
//             C = http://bar.com/
//             D = http://next.com/
//
// SiteInstances are assigned single-letter names (A, B, C) which are remembered
// across invocations of the pretty-printer.
class FrameTreeVisualizer {
 public:
  FrameTreeVisualizer();
  ~FrameTreeVisualizer();

  // Formats and returns a diagram for the provided FrameTreeNode.
  std::string DepictFrameTree(FrameTreeNode* root);

 private:
  // Assign or retrive the abbreviated short name (A, B, C) for a site instance.
  std::string GetName(SiteInstance* site_instance);

  // Elements are site instance ids. The index of the SiteInstance in the vector
  // determines the abbreviated name (0->A, 1->B) for that SiteInstance.
  std::vector<int> seen_site_instance_ids_;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeVisualizer);
};

// Uses window.open to open a popup from the frame |opener| with the specified
// |url| and |name|.   Waits for the navigation to |url| to finish and then
// returns the new popup's Shell.  Note that since this navigation to |url| is
// renderer-initiated, it won't cause a process swap unless used in
// --site-per-process mode.
Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name);

// This class can be used to stall any resource request, based on an URL match.
// There is no explicit way to resume the request; it should be used carefully.
// Note: This class likely doesn't work with PlzNavigate.
// TODO(nasko): Reimplement this class using NavigationThrottle, once it has
// the ability to defer navigation requests.
class NavigationStallDelegate : public ResourceDispatcherHostDelegate {
 public:
  explicit NavigationStallDelegate(const GURL& url);

 private:
  // ResourceDispatcherHostDelegate
  void RequestBeginning(net::URLRequest* request,
                        content::ResourceContext* resource_context,
                        content::AppCacheService* appcache_service,
                        ResourceType resource_type,
                        std::vector<std::unique_ptr<content::ResourceThrottle>>*
                            throttles) override;

  GURL url_;
};

// Helper for mocking choosing a file via a file dialog.
class FileChooserDelegate : public WebContentsDelegate {
 public:
  // Constructs a WebContentsDelegate that mocks a file dialog.
  // The mocked file dialog will always reply that the user selected |file|.
  FileChooserDelegate(const base::FilePath& file);

  // Implementation of WebContentsDelegate::RunFileChooser.
  void RunFileChooser(RenderFrameHost* render_frame_host,
                      const FileChooserParams& params) override;

  // Whether the file dialog was shown.
  bool file_chosen() const { return file_chosen_; }

  // Copy of the params passed to RunFileChooser.
  FileChooserParams params() const { return params_; }

 private:
  base::FilePath file_;
  bool file_chosen_;
  FileChooserParams params_;
};

// This class is a TestNavigationManager that only monitors notifications within
// the given frame tree node.
class FrameTestNavigationManager : public TestNavigationManager {
 public:
  FrameTestNavigationManager(int frame_tree_node_id,
                             WebContents* web_contents,
                             const GURL& url);

 private:
  // TestNavigationManager:
  bool ShouldMonitorNavigation(NavigationHandle* handle) override;

  // Notifications are filtered so only this frame is monitored.
  int filtering_frame_tree_node_id_;

  DISALLOW_COPY_AND_ASSIGN(FrameTestNavigationManager);
};

// An observer that can wait for a specific URL to be committed in a specific
// frame.
// Note: it does not track the start of a navigation, unlike other observers.
class UrlCommitObserver : WebContentsObserver {
 public:
  explicit UrlCommitObserver(FrameTreeNode* frame_tree_node, const GURL& url);
  ~UrlCommitObserver() override;

  void Wait();

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // The id of the FrameTreeNode in which navigations are peformed.
  int frame_tree_node_id_;

  // The URL this observer is expecting to be committed.
  GURL url_;

  // The RunLoop used to spin the message loop.
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(UrlCommitObserver);
};

// Class to sniff incoming IPCs for FrameHostMsg_UpdateResizeParams messages.
// This allows the message to continue to the target child so that processing
// can be verified by tests.
class UpdateResizeParamsMessageFilter : public content::BrowserMessageFilter {
 public:
  UpdateResizeParamsMessageFilter();

  gfx::Rect last_rect() const { return last_rect_; }

  void WaitForRect();
  void ResetRectRunLoop();

  // Returns the new viz::FrameSinkId immediately if the IPC has been received.
  // Otherwise this will block the UI thread until it has been received, then it
  // will return the new viz::FrameSinkId.
  viz::FrameSinkId GetOrWaitForId();

 protected:
  ~UpdateResizeParamsMessageFilter() override;

 private:
  void OnUpdateResizeParams(const gfx::Rect& rect,
                            const ScreenInfo& screen_info,
                            uint64_t sequence_number,
                            const viz::SurfaceId& surface_id);
  void OnUpdatedFrameRectOnUI(const gfx::Rect& rect);
  void OnUpdatedFrameSinkIdOnUI();

  bool OnMessageReceived(const IPC::Message& message) override;

  viz::FrameSinkId frame_sink_id_;
  base::RunLoop frame_sink_id_run_loop_;

  std::unique_ptr<base::RunLoop> frame_rect_run_loop_;
  bool frame_rect_received_;
  gfx::Rect last_rect_;

  DISALLOW_COPY_AND_ASSIGN(UpdateResizeParamsMessageFilter);
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_
