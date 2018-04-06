// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation for TaskRunner::PostTaskAndReply.

#ifndef BASE_THREADING_POST_TASK_AND_REPLY_IMPL_H_
#define BASE_THREADING_POST_TASK_AND_REPLY_IMPL_H_

#include "base/base_export.h"
#include "base/callback.h"
#include "base/location.h"

namespace base {
namespace internal {

// Inherit from this in a class that implements PostTask to send a task to a
// custom execution context.
//
// If you're looking for a concrete implementation of PostTaskAndReply, you
// probably want base::TaskRunner or base/task_scheduler/post_task.h
class BASE_EXPORT PostTaskAndReplyImpl {
 public:
  virtual ~PostTaskAndReplyImpl() = default;

  // Posts |task| by calling PostTask(). On completion, posts |reply| to the
  // sequence that called this. Can only be called when
  // SequencedTaskRunnerHandle::IsSet(). |task| is deleted on the target
  // TaskRunner or on the sequence that called this. |reply| is deleted on the
  // sequence that called this, or leaked if it can't be deleted before the
  // sequence stops accepting tasks.
  bool PostTaskAndReply(const Location& from_here,
                        OnceClosure task,
                        OnceClosure reply);

 private:
  virtual bool PostTask(const Location& from_here, OnceClosure task) = 0;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_THREADING_POST_TASK_AND_REPLY_IMPL_H_
