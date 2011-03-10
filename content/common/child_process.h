// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_PROCESS_H_
#define CONTENT_COMMON_CHILD_PROCESS_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"

class ChildThread;

// Base class for child processes of the browser process (i.e. renderer and
// plugin host). This is a singleton object for each child process.
class ChildProcess {
 public:
  // Child processes should have an object that derives from this class.
  // Normally you would immediately call set_main_thread after construction.
  ChildProcess();
  virtual ~ChildProcess();

  // May be NULL if the main thread hasn't been set explicitly.
  ChildThread* main_thread();

  // Sets the object associated with the main thread of this process.
  // Takes ownership of the pointer.
  void set_main_thread(ChildThread* thread);

  MessageLoop* io_message_loop() { return io_thread_.message_loop(); }

  // A global event object that is signalled when the main thread's message
  // loop exits.  This gives background threads a way to observe the main
  // thread shutting down.  This can be useful when a background thread is
  // waiting for some information from the browser process.  If the browser
  // process goes away prematurely, the background thread can at least notice
  // the child processes's main thread exiting to determine that it should give
  // up waiting.
  // For example, see the renderer code used to implement
  // webkit_glue::GetCookies.
  base::WaitableEvent* GetShutDownEvent();

  // These are used for ref-counting the child process.  The process shuts
  // itself down when the ref count reaches 0.
  // For example, in the renderer process, generally each tab managed by this
  // process will hold a reference to the process, and release when closed.
  void AddRefProcess();
  void ReleaseProcess();

  // Getter for the one ChildProcess object for this process.
  static ChildProcess* current() { return child_process_; }

  static void WaitForDebugger(const std::string& label);
 private:
  int ref_count_;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  // The thread that handles IO events.
  base::Thread io_thread_;

  // NOTE: make sure that main_thread_ is listed after shutdown_event_, since
  // it depends on it (indirectly through IPC::SyncChannel).  Same for
  // io_thread_.
  scoped_ptr<ChildThread> main_thread_;

  // The singleton instance for this process.
  static ChildProcess* child_process_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcess);
};

#endif  // CONTENT_COMMON_CHILD_PROCESS_H_
