// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_THREAD_H_
#define IOS_WEB_PUBLIC_WEB_THREAD_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace base {
class SequencedWorkerPool;
}

namespace tracked_objects {
class Location;
}

namespace web {

// Use DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::ID) to assert that a function
// can only be called on the named WebThread.
// TODO(ios): rename to DCHECK_CURRENTLY_ON once iOS is independent from
// content/ so it won't collide with the macro DCHECK_CURRENTLY_ON in content/.
// http://crbug.com/438202
#define DCHECK_CURRENTLY_ON_WEB_THREAD(thread_identifier)   \
  (DCHECK(::web::WebThread::CurrentlyOn(thread_identifier)) \
   << ::web::WebThread::GetDCheckCurrentlyOnErrorMessage(thread_identifier))

// WebThread
//
// Utility functions for threads that are known by name. For example, there is
// one IO thread for the entire process, and various pieces of code find it
// useful to retrieve a pointer to the IO thread's message loop.
class WebThread {
 public:
  // An enumeration of the well-known threads.
  // NOTE: threads must be listed in the order of their life-time, with each
  // thread outliving every other thread below it.
  enum ID {
    // The main thread in the browser.
    UI,
  };

  // These are the same methods as in message_loop.h, but are guaranteed to
  // either get posted to the MessageLoop if it's still alive, or be deleted
  // otherwise.
  // They return true iff the thread existed and the task was posted.
  static bool PostTask(ID identifier,
                       const tracked_objects::Location& from_here,
                       const base::Closure& task);

  // Returns the thread pool used for blocking file I/O. Use this object to
  // perform random blocking operations such as file writes.
  static base::SequencedWorkerPool* GetBlockingPool() WARN_UNUSED_RESULT;

  // Callable on any thread.  Returns whether execution is currently on the
  // given thread.  To DCHECK this, use the DCHECK_CURRENTLY_ON_WEB_THREAD()
  // macro above.
  static bool CurrentlyOn(ID identifier) WARN_UNUSED_RESULT;

  // Returns an appropriate error message for when
  // DCHECK_CURRENTLY_ON_WEB_THREAD() fails.
  static std::string GetDCheckCurrentlyOnErrorMessage(ID expected);

 private:
  WebThread() {}
  DISALLOW_COPY_AND_ASSIGN(WebThread);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_THREAD_H_
