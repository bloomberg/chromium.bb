// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_USER_SCRIPT_IDLE_SCHEDULER_H_
#define CHROME_RENDERER_USER_SCRIPT_IDLE_SCHEDULER_H_
#pragma once

#include <queue>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/task.h"
#include "content/renderer/render_view_observer.h"

class RenderView;
struct ExtensionMsg_ExecuteCode_Params;

namespace WebKit {
class WebFrame;
}

// Implements support for injecting scripts at "document idle". Currently,
// determining idleness is simple: it is whichever of the following happens
// first:
//
// a) When the initial DOM for a page is complete + kUserScriptIdleTimeout,
// b) or when the page has completely loaded including all subresources.
//
// The intent of this mechanism is to prevent user scripts from slowing down
// fast pages (run after load), while still allowing them to run relatively
// timely for pages with lots of slow subresources.
class UserScriptIdleScheduler : public RenderViewObserver {
 public:
  UserScriptIdleScheduler(RenderView* render_view, WebKit::WebFrame* frame);
  ~UserScriptIdleScheduler();

 private:
  // RenderViewObserver implementation:
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame);
  virtual void DidFinishLoad(WebKit::WebFrame* frame);
  virtual void DidStartProvisionalLoad(WebKit::WebFrame* frame);
  virtual void FrameDetached(WebKit::WebFrame* frame);

  // Run user scripts, except if they've already run for this frame, or the
  // frame has been destroyed.
  void MaybeRun();

  void OnExecuteCode(const ExtensionMsg_ExecuteCode_Params& params);

  // Backend for the IPC Message ExecuteCode in addition to being used
  // internally.
  void ExecuteCodeImpl(WebKit::WebFrame* frame,
                       const ExtensionMsg_ExecuteCode_Params& params);

  // Get all child frames of parent_frame, returned by frames_vector.
  bool GetAllChildFrames(WebKit::WebFrame* parent_frame,
                         std::vector<WebKit::WebFrame*>* frames_vector) const;

  WebKit::WebFrame* GetMainFrame();

  ScopedRunnableMethodFactory<UserScriptIdleScheduler> method_factory_;

  // The Frame we will run scripts in.
  WebKit::WebFrame* frame_;

  // Whether we have already run scripts.
  bool has_run_;

  // This is only used if we're for the main frame.
  std::queue<linked_ptr<ExtensionMsg_ExecuteCode_Params> >
      pending_code_execution_queue_;
};

#endif  // CHROME_RENDERER_USER_SCRIPT_IDLE_SCHEDULER_H_
