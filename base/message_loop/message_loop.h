// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_LOOP_FORWARD_H_
#define BASE_MESSAGE_LOOP_MESSAGE_LOOP_FORWARD_H_

#include "base/message_loop/message_loop_impl.h"
#include "build/build_config.h"

// TODO(crbug.com/891670) : Extract a common interface for base::MessageLoop
// in this file (to be shared with base::SequenceManager during the migration).

namespace base {

#if !defined(OS_NACL)

//-----------------------------------------------------------------------------
// MessageLoopForUI extends MessageLoop with methods that are particular to a
// MessageLoop instantiated with TYPE_UI.
//
// By instantiating a MessageLoopForUI on the current thread, the owner enables
// native UI message pumping.
//
// MessageLoopCurrentForUI is exposed statically on its thread via
// MessageLoopCurrentForUI::Get() to provide additional functionality.
//
class BASE_EXPORT MessageLoopForUI : public MessageLoop {
 public:
  explicit MessageLoopForUI(Type type = TYPE_UI);

  // TODO(gab): Mass migrate callers to MessageLoopCurrentForUI::Get()/IsSet().
  static MessageLoopCurrentForUI current();
  static bool IsCurrent();

#if defined(OS_IOS)
  // On iOS, the main message loop cannot be Run().  Instead call Attach(),
  // which connects this MessageLoop to the UI thread's CFRunLoop and allows
  // PostTask() to work.
  void Attach();
#endif

#if defined(OS_ANDROID)
  // On Android there are cases where we want to abort immediately without
  // calling Quit(), in these cases we call Abort().
  void Abort();

  // True if this message pump has been aborted.
  bool IsAborted();

  // Since Run() is never called on Android, and the message loop is run by the
  // java Looper, quitting the RunLoop won't join the thread, so we need a
  // callback to run when the RunLoop goes idle to let the Java thread know when
  // it can safely quit.
  void QuitWhenIdle(base::OnceClosure callback);
#endif

#if defined(OS_WIN)
  // See method of the same name in the Windows MessagePumpForUI implementation.
  void EnableWmQuit();
#endif
};

// Do not add any member variables to MessageLoopForUI!  This is important b/c
// MessageLoopForUI is often allocated via MessageLoop(TYPE_UI).  Any extra
// data that you need should be stored on the MessageLoop's pump_ instance.
static_assert(sizeof(MessageLoop) == sizeof(MessageLoopForUI),
              "MessageLoopForUI should not have extra member variables");

#endif  // !defined(OS_NACL)

//-----------------------------------------------------------------------------
// MessageLoopForIO extends MessageLoop with methods that are particular to a
// MessageLoop instantiated with TYPE_IO.
//
// By instantiating a MessageLoopForIO on the current thread, the owner enables
// native async IO message pumping.
//
// MessageLoopCurrentForIO is exposed statically on its thread via
// MessageLoopCurrentForIO::Get() to provide additional functionality.
//
class BASE_EXPORT MessageLoopForIO : public MessageLoop {
 public:
  MessageLoopForIO() : MessageLoop(TYPE_IO) {}

  // TODO(gab): Mass migrate callers to MessageLoopCurrentForIO::Get()/IsSet().
  static MessageLoopCurrentForIO current();
  static bool IsCurrent();
};

// Do not add any member variables to MessageLoopForIO!  This is important b/c
// MessageLoopForIO is often allocated via MessageLoop(TYPE_IO).  Any extra
// data that you need should be stored on the MessageLoop's pump_ instance.
static_assert(sizeof(MessageLoop) == sizeof(MessageLoopForIO),
              "MessageLoopForIO should not have extra member variables");

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_LOOP_FORWARD_H_
