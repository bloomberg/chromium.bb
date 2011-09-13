// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some basic utilities for aiding in the management of Tasks and Callbacks.
//
// AutoCallbackRunner is akin to scoped_ptr for callbacks.  It is useful for
// ensuring a callback is executed and delete in the face of multiple return
// points in a function.
//
// TaskToCallbackAdapter converts a Task to a Callback0::Type since the two type
// hierarchies are strangely separate.
//
// CleanupCallback wraps another Callback and provides the ability to register
// objects for deletion as well as cleanup tasks that will be run on the
// callback's destruction.  The deletion and cleanup tasks will be run on
// whatever thread the CleanupCallback is destroyed in.

#ifndef MEDIA_BASE_CALLBACK_
#define MEDIA_BASE_CALLBACK_

#include <vector>

#include "base/callback.h"
#include "base/callback_old.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT AutoCallbackRunner {
 public:
  // Takes ownership of the callback.
  explicit AutoCallbackRunner(Callback0::Type* callback)
      : callback_(callback) {
  }

  ~AutoCallbackRunner();

  Callback0::Type* release() { return callback_.release(); }

 private:
  scoped_ptr<Callback0::Type> callback_;

  DISALLOW_COPY_AND_ASSIGN(AutoCallbackRunner);
};

class TaskToCallbackAdapter : public Callback0::Type {
 public:
  static Callback0::Type* NewCallback(Task* task);

  virtual ~TaskToCallbackAdapter();

  virtual void RunWithParams(const Tuple0& params);

 private:
  TaskToCallbackAdapter(Task* task);

  scoped_ptr<Task> task_;

  DISALLOW_COPY_AND_ASSIGN(TaskToCallbackAdapter);
};

// TODO(acolwell): Delete this once all old style callbacks have been
// removed from the media code.
//
// The new callback stores a copy of |cb| so the lifetime of the copy
// matches the lifetime of the new callback.
Callback0::Type* NewCallbackForClosure(const base::Closure& cb);

}  // namespace media

#endif  // MEDIA_BASE_CALLBACK_
