// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FRAME_DEVTOOLS_AGENT_DELEGATE_H_
#define COMPONENTS_WEB_VIEW_FRAME_DEVTOOLS_AGENT_DELEGATE_H_

class GURL;

namespace web_view {

class FrameDevToolsAgentDelegate {
 public:
  virtual ~FrameDevToolsAgentDelegate() {}

  virtual void HandlePageNavigateRequest(const GURL& url) = 0;
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FRAME_DEVTOOLS_AGENT_DELEGATE_H_
