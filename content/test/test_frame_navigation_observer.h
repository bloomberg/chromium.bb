// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_FRAME_NAVIGATION_OBSERVER_H_
#define CONTENT_TEST_TEST_FRAME_NAVIGATION_OBSERVER_H_

#include <set>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"

class GURL;

namespace content {
class FrameTreeNode;
class RenderFrameHostImpl;
class WebContents;
struct LoadCommittedDetails;

// For content_browsertests, which run on the UI thread, run a second
// MessageLoop and quit when the navigation in a specific frame completes
// loading.
class TestFrameNavigationObserver : public WebContentsObserver {
 public:
  // Create and register a new TestFrameNavigationObserver which will track
  // navigations performed in the specified |node| of the frame tree.
  TestFrameNavigationObserver(FrameTreeNode* node, int number_of_navigations);
  // As above but waits for one navigation.
  explicit TestFrameNavigationObserver(FrameTreeNode* node);

  virtual ~TestFrameNavigationObserver();

  // Runs a nested message loop and blocks until the expected number of
  // navigations are complete.
  void Wait();

 private:
  // WebContentsObserver
  virtual void DidStartProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) OVERRIDE;
  virtual void DidNavigateAnyFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) OVERRIDE;

  // The id of the FrameTreeNode in which navigations are peformed.
  int frame_tree_node_id_;

  // If true the navigation has started.
  bool navigation_started_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  int number_of_navigations_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameNavigationObserver);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_FRAME_NAVIGATION_OBSERVER_H_
