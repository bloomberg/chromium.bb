// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_DEVTOOLS_BINDINGS_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_DEVTOOLS_BINDINGS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/shell/browser/shell_devtools_frontend.h"

namespace content {

class WebContents;

class LayoutTestDevToolsBindings : public ShellDevToolsBindings {
 public:
  static GURL GetDevToolsPathAsURL(const std::string& frontend_url);

  static GURL MapTestURLIfNeeded(const GURL& test_url,
                                 bool* is_devtools_js_test);

  static GURL GetInspectedPageURL(const GURL& test_url);

  static LayoutTestDevToolsBindings* LoadDevTools(
      WebContents* devtools_contents_,
      WebContents* inspected_contents_,
      const std::string& settings,
      const std::string& frontend_url);
  void EvaluateInFrontend(int call_id, const std::string& expression);

  ~LayoutTestDevToolsBindings() override;

 private:
  LayoutTestDevToolsBindings(WebContents* devtools_contents,
                             WebContents* inspected_contents);

  // ShellDevToolsBindings overrides.
  void HandleMessageFromDevToolsFrontend(const std::string& message) override;

  // WebContentsObserver implementation.
  void RenderProcessGone(base::TerminationStatus status) override;
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  bool ready_for_test_;
  std::vector<std::pair<int, std::string>> pending_evaluations_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestDevToolsBindings);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_DEVTOOLS_BINDINGS_H_
