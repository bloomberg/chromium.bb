// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_USER_SCRIPT_SCHEDULER_H_
#define CHROME_RENDERER_EXTENSIONS_USER_SCRIPT_SCHEDULER_H_
#pragma once

#include <map>
#include <queue>

#include "chrome/common/extensions/user_script.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"

class ExtensionDispatcher;
class RenderView;
struct ExtensionMsg_ExecuteCode_Params;

namespace WebKit {
class WebFrame;
}

// Implements support for injecting scripts at different times in the document
// loading process. The different possible time are described in
// UserScript::RunLocation.
//
// Currently, determining idleness is simple: it is whichever of the following
// happens first:
//
// a) When the initial DOM for a page is complete + kUserScriptIdleTimeout,
// b) or when the page has completely loaded including all subresources.
//
// The intent of this mechanism is to prevent user scripts from slowing down
// fast pages (run after load), while still allowing them to run relatively
// timely for pages with lots of slow subresources.
//
// NOTE: this class does not inherit from RenderViewObserver on purpose.  The
// reason is that this object is per frame, and a frame can move across
// RenderViews thanks to adoptNode.  So we have each RenderView's
// ExtensionHelper proxy these calls to the renderer process'
// ExtensionDispatcher, which contains the mapping from WebFrame to us.
class UserScriptScheduler {
 public:
  UserScriptScheduler(WebKit::WebFrame* frame,
                          ExtensionDispatcher* extension_dispatcher);
  ~UserScriptScheduler();

  void ExecuteCode(const ExtensionMsg_ExecuteCode_Params& params);
  void DidCreateDocumentElement();
  void DidFinishDocumentLoad();
  void DidFinishLoad();
  void DidStartProvisionalLoad();

 private:
  typedef
    std::queue<linked_ptr<ExtensionMsg_ExecuteCode_Params> >
    ExecutionQueue;

  // Run user scripts, except if they've already run for this frame, or the
  // frame has been destroyed.
  void MaybeRun();

  // Backend for the IPC Message ExecuteCode in addition to being used
  // internally.
  void ExecuteCodeImpl(const ExtensionMsg_ExecuteCode_Params& params);

  // Get all child frames of parent_frame, returned by frames_vector.
  bool GetAllChildFrames(WebKit::WebFrame* parent_frame,
                         std::vector<WebKit::WebFrame*>* frames_vector) const;

  // Call to signify thet the idle timeout has expired.
  void IdleTimeout();

  base::WeakPtrFactory<UserScriptScheduler> weak_factory_;

  // The Frame we will run scripts in.
  WebKit::WebFrame* frame_;

  // The current location in the document loading process.
  // Will be UserScript::UNDEFINED if it is before any scripts should be run.
  UserScript::RunLocation current_location_;

  // Whether we have already run the idle scripts.
  bool has_run_idle_;

  // This is only used if we're for the main frame.
  std::map<UserScript::RunLocation, ExecutionQueue> pending_execution_map_;

  ExtensionDispatcher* extension_dispatcher_;
};

#endif  // CHROME_RENDERER_EXTENSIONS_USER_SCRIPT_SCHEDULER_H_
