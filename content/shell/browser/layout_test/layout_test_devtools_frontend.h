// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_DEVTOOLS_FRONTEND_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_DEVTOOLS_FRONTEND_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/shell/browser/shell_devtools_frontend.h"

namespace content {

class Shell;
class WebContents;

class LayoutTestDevToolsFrontend : public ShellDevToolsFrontend {
 public:
  static LayoutTestDevToolsFrontend* Show(WebContents* inspected_contents,
                                          const std::string& settings,
                                          const std::string& frontend_url);

  static GURL GetDevToolsPathAsURL(const std::string& frontend_url);

  static GURL MapJSTestURL(const GURL& test_url);

  void ReuseFrontend(const std::string& settings,
                     const std::string frontend_url);
  void EvaluateInFrontend(int call_id, const std::string& expression);

 private:
  LayoutTestDevToolsFrontend(Shell* frontend_shell,
                             WebContents* inspected_contents);
  ~LayoutTestDevToolsFrontend() override;

  // content::DevToolsAgentHostClient implementation.
  void AgentHostClosed(DevToolsAgentHost* agent_host, bool replaced) override;

  // ShellDevToolsFrontend overrides.
  void HandleMessageFromDevToolsFrontend(const std::string& message) override;

  // WebContentsObserver implementation.
  void RenderProcessGone(base::TerminationStatus status) override;
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  bool ready_for_test_;
  std::vector<std::pair<int, std::string>> pending_evaluations_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestDevToolsFrontend);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_DEVTOOLS_FRONTEND_H_
