// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_HELPERS_H_
#define BASE_MESSAGE_LOOP_HELPERS_H_
#pragma once

#include "base/basictypes.h"

namespace tracked_objects {
class Location;
}

namespace base {

namespace subtle {
template <class T, class R> class DeleteHelperInternal;
}

// This is a template helper which uses a function indirection to erase T from
// the function signature while still remembering it so we can call the correct
// destructor. We use this trick so we don't need to include bind.h in a
// header file. We also embed the function in a class to make it easier for
// users of DeleteSoon to declare the helper as a friend.
template <class T>
class DeleteHelper {
 private:
  // TODO(dcheng): Move the return type back to a function template parameter.
  template <class T2, class R> friend class subtle::DeleteHelperInternal;

  static void DoDelete(const void* object) {
    delete reinterpret_cast<const T*>(object);
  }

  DISALLOW_COPY_AND_ASSIGN(DeleteHelper);
};

namespace subtle {

// An internal MessageLoop-like class helper for DeleteHelper. We don't want to
// expose DoDelete directly since the void* argument makes it possible to pass
// an object of the wrong type to delete. Instead, we force callers to go
// through DeleteHelperInternal for type safety. MessageLoop-like classes which
// expose a DeleteSoon method should friend this class and implement a
// DeleteSoonInternal method with the following signature:
// bool(const tracked_objects::Location&,
//      void(*deleter)(const void*),
//      void* object)
// An implementation of DeleteSoonInternal should simply create a base::Closure
// from (deleter, object) and return the result of posting the task.
template <class T, class ReturnType>
class DeleteHelperInternal {
 public:
  template <class MessageLoopType>
  static ReturnType DeleteOnMessageLoop(
      MessageLoopType* message_loop,
      const tracked_objects::Location& from_here,
      const T* object) {
    return message_loop->DeleteSoonInternal(from_here,
                                            &DeleteHelper<T>::DoDelete,
                                            object);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteHelperInternal);
};

}  // namespace subtle

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_HELPERS_H_
