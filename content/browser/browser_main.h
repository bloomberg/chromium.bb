// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_MAIN_H_
#define CONTENT_BROWSER_BROWSER_MAIN_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class BrowserThread;
class CommandLine;
class HighResolutionTimerManager;
class MessageLoop;
struct MainFunctionParams;

namespace base {
class SystemMonitor;
}

namespace net {
class NetworkChangeNotifier;
}

namespace content {

// BrowserMainParts:
// This class contains different "stages" to be executed in |BrowserMain()|,
// mostly initialization. This is made into a class rather than just functions
// so each stage can create and maintain state. Each part is represented by a
// single method (e.g., "EarlyInitialization()"), which does the following:
//  - calls a method (e.g., "PreEarlyInitialization()") which individual
//    platforms can override to provide platform-specific code which is to be
//    executed before the common code;
//  - calls various methods for things common to all platforms (for that given
//    stage); and
//  - calls a method (e.g., "PostEarlyInitialization()") for platform-specific
//    code to be called after the common code.
// As indicated above, platforms should override the default "Pre...()" and
// "Post...()" methods when necessary; they need not call the superclass's
// implementation (which is empty).
//
// Parts:
//  - EarlyInitialization: things which should be done as soon as possible on
//    program start (such as setting up signal handlers) and things to be done
//    at some generic time before the start of the main message loop.
//  - MainMessageLoopStart: things beginning with the start of the main message
//    loop and ending with initialization of the main thread; platform-specific
//    things which should be done immediately before the start of the main
//    message loop should go in |PreMainMessageLoopStart()|.
//  - (more to come)
//
// How to add stuff (to existing parts):
//  - Figure out when your new code should be executed. What must happen
//    before/after your code is executed? Are there performance reasons for
//    running your code at a particular time? Document these things!
//  - Split out any platform-specific bits. Please avoid #ifdefs it at all
//    possible. You have two choices for platform-specific code: (1) Execute it
//    from one of the platform-specific |Pre/Post...()| methods; do this if the
//    code is unique to a platform type. Or (2) execute it from one of the
//    "parts" (e.g., |EarlyInitialization()|) and provide platform-specific
//    implementations of your code (in a virtual method); do this if you need to
//    provide different implementations across most/all platforms.
//  - Unless your new code is just one or two lines, put it into a separate
//    method with a well-defined purpose. (Likewise, if you're adding to an
//    existing chunk which makes it longer than one or two lines, please move
//    the code out into a separate method.)
class BrowserMainParts {
 public:
  explicit BrowserMainParts(const MainFunctionParams& parameters);
  virtual ~BrowserMainParts();

  // Parts to be called by |BrowserMain()|.
  void EarlyInitialization();
  void InitializeToolkit();
  void MainMessageLoopStart();
  void RunMainMessageLoopParts();

  int result_code() const { return result_code_; }

 protected:
  // Methods to be overridden to provide platform-specific code; these
  // correspond to the "parts" above.
  virtual void PreEarlyInitialization();
  virtual void PostEarlyInitialization();
  virtual void PreMainMessageLoopStart();
  virtual void PostMainMessageLoopStart();
  virtual void PreMainMessageLoopRun();
  virtual void MainMessageLoopRun();
  virtual void PostMainMessageLoopRun();

  // Allows an embedder to do any extra toolkit initialization.
  virtual void ToolkitInitialized();

  // Accessors for data members (below) ----------------------------------------
  const MainFunctionParams& parameters() const {
    return parameters_;
  }
  const CommandLine& parsed_command_line() const {
    return parsed_command_line_;
  }
  MessageLoop& main_message_loop() const {
    return *main_message_loop_;
  }
  void set_result_code(int result_code) { result_code_ = result_code; }

 private:
  void InitializeMainThread();

  // Members initialized on construction ---------------------------------------

  const MainFunctionParams& parameters_;
  const CommandLine& parsed_command_line_;
  int result_code_;

  // Members initialized in |MainMessageLoopStart()| ---------------------------
  scoped_ptr<MessageLoop> main_message_loop_;
  scoped_ptr<base::SystemMonitor> system_monitor_;
  scoped_ptr<HighResolutionTimerManager> hi_res_timer_manager_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<BrowserThread> main_thread_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMainParts);
};

// Perform platform-specific work that needs to be done after the main event
// loop has ended. The embedder must be sure to call this.
// TODO(jam): change this so that content calls it so that we don't depend on
// the embedder.
void DidEndMainMessageLoop();

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_MAIN_H_
