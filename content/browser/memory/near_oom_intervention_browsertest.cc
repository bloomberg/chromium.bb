// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/oom_intervention.mojom.h"

// TODO(bashi): Move this browsertest into chrome/browser/android once
// crbug.com/611756 is fixed.

namespace content {

class NearOomInterventionBrowserTest : public ContentBrowserTest {
 protected:
  void NavigateAndInitializeIntervention() {
    GURL url = GetTestUrl("", "simple_page.html");
    EXPECT_TRUE(NavigateToURL(shell(), url));

    RenderFrameHost* main_frame = shell()->web_contents()->GetMainFrame();
    EXPECT_TRUE(main_frame != nullptr);
    main_frame->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&intervention_));
  }

  blink::mojom::OomInterventionPtr intervention_;
};

IN_PROC_BROWSER_TEST_F(NearOomInterventionBrowserTest, DetectedAndDeclined) {
  NavigateAndInitializeIntervention();
  {
    base::RunLoop run_loop;
    intervention_->OnNearOomDetected(run_loop.QuitClosure());
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    intervention_->OnInterventionDeclined(run_loop.QuitClosure());
    run_loop.Run();
  }
}

}  // namespace content
