// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_MESSAGE_LOOP_REF_H_
#define MOJO_SHELL_PUBLIC_CPP_MESSAGE_LOOP_REF_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

class MessageLoopRefImpl;

// An interface implementation can keep this object as a member variable to
// hold a reference to the ShellConnection, keeping it alive as long as the
// bound implementation exists.
// Since interface implementations can be bound on different threads than the
// ShellConnection, this class is safe to use on any thread. However, each
// instance should only be used on one thread at a time (otherwise there'll be
// races between the AddRef resulting from cloning and destruction).
class MessageLoopRef {
 public:
  virtual ~MessageLoopRef() {}

  virtual scoped_ptr<MessageLoopRef> Clone() = 0;
};

class MessageLoopRefFactory {
 public:
  MessageLoopRefFactory();
  ~MessageLoopRefFactory();

  void set_quit_closure(const base::Closure& quit_closure) {
    quit_closure_ = quit_closure;
  }

  scoped_ptr<MessageLoopRef> CreateRef();

 private:
  friend MessageLoopRefImpl;

  // Called from MessageLoopRefImpl.
  void AddRef();
  void Release();

  base::Closure quit_closure_;
  int ref_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopRefFactory);
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_MESSAGE_LOOP_REF_H_
