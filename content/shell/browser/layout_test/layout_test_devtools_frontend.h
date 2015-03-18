// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_DEVTOOLS_FRONTEND_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_DEVTOOLS_FRONTEND_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/shell/browser/shell_devtools_frontend.h"

namespace content {

class RenderViewHost;
class Shell;
class WebContents;

class LayoutTestDevToolsFrontend : public ShellDevToolsFrontend {
 public:
  static LayoutTestDevToolsFrontend* Show(WebContents* inspected_contents,
                                          const std::string& settings,
                                          const std::string& frontend_url);

  static GURL GetDevToolsPathAsURL(const std::string& settings,
                                   const std::string& frontend_url);

  void ReuseFrontend(const std::string& settings,
                     const std::string frontend_url);

 private:
  LayoutTestDevToolsFrontend(Shell* frontend_shell,
                             WebContents* inspected_contents);
  ~LayoutTestDevToolsFrontend() override;

  // content::DevToolsAgentHostClient implementation.
  void AgentHostClosed(DevToolsAgentHost* agent_host, bool replaced) override;

  // WebContentsObserver implementation.
  void RenderProcessGone(base::TerminationStatus status) override;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestDevToolsFrontend);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_DEVTOOLS_FRONTEND_H_
