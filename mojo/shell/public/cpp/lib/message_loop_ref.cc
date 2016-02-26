// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/message_loop_ref.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace mojo {

class MessageLoopRefImpl : public MessageLoopRef {
 public:
  MessageLoopRefImpl(
      MessageLoopRefFactory* factory,
      scoped_refptr<base::SingleThreadTaskRunner> app_task_runner)
      : factory_(factory),
        app_task_runner_(app_task_runner) {}
  ~MessageLoopRefImpl() override {
#ifndef NDEBUG
    // Ensure that this object is used on only one thread at a time, or else
    // there could be races where the object is being reset on one thread and
    // cloned on another.
    if (clone_task_runner_)
      DCHECK(clone_task_runner_->BelongsToCurrentThread());
#endif

    if (app_task_runner_->BelongsToCurrentThread()) {
      factory_->Release();
    } else {
      app_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&MessageLoopRefFactory::Release,
                     base::Unretained(factory_)));
    }
  }

 private:
  // MessageLoopRef:
  scoped_ptr<MessageLoopRef> Clone() override {
    if (app_task_runner_->BelongsToCurrentThread()) {
      factory_->AddRef();
    } else {
      app_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&MessageLoopRefFactory::AddRef,
                     base::Unretained(factory_)));
    }

#ifndef NDEBUG
    // Ensure that this object is used on only one thread at a time, or else
    // there could be races where the object is being reset on one thread and
    // cloned on another.
    if (clone_task_runner_) {
      DCHECK(clone_task_runner_->BelongsToCurrentThread());
    } else {
      clone_task_runner_ = base::MessageLoop::current()->task_runner();
    }
#endif

    return make_scoped_ptr(new MessageLoopRefImpl(factory_, app_task_runner_));
  }

  MessageLoopRefFactory* factory_;
  scoped_refptr<base::SingleThreadTaskRunner> app_task_runner_;

#ifndef NDEBUG
  scoped_refptr<base::SingleThreadTaskRunner> clone_task_runner_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MessageLoopRefImpl);
};

MessageLoopRefFactory::MessageLoopRefFactory() {}
MessageLoopRefFactory::~MessageLoopRefFactory() {}

scoped_ptr<MessageLoopRef> MessageLoopRefFactory::CreateRef() {
  AddRef();
  return make_scoped_ptr(new MessageLoopRefImpl(
      this, base::MessageLoop::current()->task_runner()));
}

void MessageLoopRefFactory::AddRef() {
  ++ref_count_;
}

void MessageLoopRefFactory::Release() {
  if (!--ref_count_) {
    if (!quit_closure_.is_null())
      quit_closure_.Run();
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running()) {
      base::MessageLoop::current()->QuitWhenIdle();
    }
  }
}

}  // namespace mojo
