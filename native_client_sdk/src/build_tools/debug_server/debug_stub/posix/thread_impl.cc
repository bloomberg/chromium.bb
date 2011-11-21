/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdexcept>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/debug_server/port/mutex.h"
#include "native_client/src/debug_server/port/thread.h"

/*
 * Define the OS specific portions of gdb_utils IThread interface.
 */

namespace port {

static IMutex* ThreadGetLock() {
  static IMutex* mutex_ = IMutex::Allocate();
  return mutex_;
}

static IThread::ThreadMap_t *ThreadGetMap() {
  static IThread::ThreadMap_t* map_ = new IThread::ThreadMap_t;
  return map_;
}

// TODO(noelallen) : Add POSIX implementation.  These functions
// represent a minimal implementation to allow the debugging
// code to link and run.
class Thread : public IThread {
 public:
  explicit Thread(uint32_t id) : ref_(1), id_(id), state_(DEAD) {}
  ~Thread() {}

  uint32_t GetId() {
    return id_;
  }

  State GetState() {
    return state_;
  }

  virtual bool Suspend() {
    return false;
  }

  virtual bool Resume() {
    return false;
  }

  virtual bool SetStep(bool on) {
    UNREFERENCED_PARAMETER(on);
    return false;
  }

  virtual bool GetRegister(uint32_t index, void *dst, uint32_t len) {
    UNREFERENCED_PARAMETER(index);
    UNREFERENCED_PARAMETER(dst);
    UNREFERENCED_PARAMETER(len);
    return false;
  }

  virtual bool SetRegister(uint32_t index, void* src, uint32_t len) {
    UNREFERENCED_PARAMETER(index);
    UNREFERENCED_PARAMETER(src);
    UNREFERENCED_PARAMETER(len);
    return false;
  }

  virtual void* GetContext() { return NULL; }

 private:
  uint32_t ref_;
  uint32_t id_;
  State  state_;

  friend class IThread;
};

IThread* IThread::Acquire(uint32_t id, bool create) {
  MutexLock lock(ThreadGetLock());
  Thread* thread;
  ThreadMap_t &map = *ThreadGetMap();

  // Check if we have that thread
  if (map.count(id)) {
    thread = static_cast<Thread*>(map[id]);
    thread->ref_++;
    return thread;
  }

  // If not, can we create it?
  if (create) {
    // If not add it to the map
    thread = new Thread(id);
    map[id] = thread;
    return thread;
  }

  return NULL;
}

void IThread::Release(IThread *ithread) {
  MutexLock lock(ThreadGetLock());
  Thread* thread = static_cast<Thread*>(ithread);
  thread->ref_--;

  if (thread->ref_ == 0) {
    ThreadGetMap()->erase(thread->id_);
    delete static_cast<IThread*>(thread);
  }
}

void IThread::SetExceptionCatch(IThread::CatchFunc_t func, void *cookie) {
  UNREFERENCED_PARAMETER(func);
  UNREFERENCED_PARAMETER(cookie);
}


}  // End of port namespace

