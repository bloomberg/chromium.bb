// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_CORE_IMPL_H_
#define MOJO_SYSTEM_CORE_IMPL_H_

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/public/system/core.h"

namespace mojo {
namespace system {

class CoreImpl;
class Dispatcher;

namespace test {
class CoreTestBase;
}

// |CoreImpl| is a singleton object that implements the Mojo system calls. With
// the (obvious) exception of |Init()|, which must be called first (and the call
// completed) before making any other calls, all the public methods are
// thread-safe.
class MOJO_SYSTEM_EXPORT CoreImpl {
 public:
  static void Init();

  static CoreImpl* Get() {
    return singleton_;
  }

  MojoResult Close(MojoHandle handle);

  MojoResult Wait(MojoHandle handle,
                  MojoWaitFlags flags,
                  MojoDeadline deadline);

  MojoResult WaitMany(const MojoHandle* handles,
                      const MojoWaitFlags* flags,
                      uint32_t num_handles,
                      MojoDeadline deadline);

  MojoResult CreateMessagePipe(MojoHandle* handle_0, MojoHandle* handle_1);

  MojoResult WriteMessage(MojoHandle handle,
                          const void* bytes, uint32_t num_bytes,
                          const MojoHandle* handles, uint32_t num_handles,
                          MojoWriteMessageFlags flags);

  MojoResult ReadMessage(MojoHandle handle,
                         void* bytes, uint32_t* num_bytes,
                         MojoHandle* handles, uint32_t* num_handles,
                         MojoReadMessageFlags flags);

 private:
  friend class test::CoreTestBase;

  typedef base::hash_map<MojoHandle, scoped_refptr<Dispatcher> >
      HandleTableMap;

  CoreImpl();
  ~CoreImpl();

  // Looks up the dispatcher for the given handle. Returns null if the handle is
  // invalid.
  scoped_refptr<Dispatcher> GetDispatcher(MojoHandle handle);

  // Assigns a new handle for the given dispatcher (which must be valid);
  // returns |MOJO_HANDLE_INVALID| on failure (due to hitting resource limits).
  // Must be called under |handle_table_lock_|.
  MojoHandle AddDispatcherNoLock(scoped_refptr<Dispatcher> dispatcher);

  // Internal implementation of |Wait()| and |WaitMany()|; doesn't do basic
  // validation of arguments.
  MojoResult WaitManyInternal(const MojoHandle* handles,
                              const MojoWaitFlags* flags,
                              uint32_t num_handles,
                              MojoDeadline deadline);

  // ---------------------------------------------------------------------------

  static CoreImpl* singleton_;

  // ---------------------------------------------------------------------------

  // TODO(vtl): |handle_table_lock_| should be a reader-writer lock (if only we
  // had them).
  base::Lock handle_table_lock_;  // Protects the immediately-following members.
  HandleTableMap handle_table_;
  MojoHandle next_handle_;  // Invariant: never |MOJO_HANDLE_INVALID|.

  // ---------------------------------------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(CoreImpl);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_CORE_IMPL_H_
