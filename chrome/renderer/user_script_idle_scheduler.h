// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_USER_SCRIPT_IDLE_SCHEDULER_H_
#define CHROME_RENDERER_USER_SCRIPT_IDLE_SCHEDULER_H_
#pragma once

#include "base/task.h"

class RenderView;

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
class UserScriptIdleScheduler {
 public:
  UserScriptIdleScheduler(RenderView* view, WebKit::WebFrame* frame);
  ~UserScriptIdleScheduler();

  bool has_run() { return has_run_; }

  void set_has_run(bool has_run) { has_run_ = has_run; }

  // Called when the DOM has been completely constructed.
  void DidFinishDocumentLoad();

  // Called when the document has completed loading.
  void DidFinishLoad();

  // Called when the client has gone away and we should no longer run scripts.
  void Cancel();

 private:
  // Run user scripts, except if they've already run for this frame, or the
  // frame has been destroyed.
  void MaybeRun();

  ScopedRunnableMethodFactory<UserScriptIdleScheduler> method_factory_;

  // The RenderView we will call back to when it is time to run scripts.
  RenderView* view_;

  // The Frame we will run scripts in.
  WebKit::WebFrame* frame_;

  // Whether we have already run scripts.
  bool has_run_;
};

#endif  // CHROME_RENDERER_USER_SCRIPT_IDLE_SCHEDULER_H_
