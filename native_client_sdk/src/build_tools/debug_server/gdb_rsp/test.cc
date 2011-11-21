/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/debug_server/gdb_rsp/test.h"
#include "native_client/src/debug_server/port/platform.h"

//  Mock portability objects
namespace port {
void IPlatform::Relinquish(uint32_t msec) {
  (void) msec;
  return;
}

void IPlatform::LogInfo(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  vprintf(fmt, argptr);
}

void IPlatform::LogWarning(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  vprintf(fmt, argptr);
}

void IPlatform::LogError(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  vprintf(fmt, argptr);
}

//  The unit tests are singly threaded, so we just do nothing
//  for synchronization
class MutexMock : public IMutex {
  void Lock() {}
  void Unlock() {}
  bool Try() { return true; }
};

IMutex* IMutex::Allocate() { return new MutexMock; }
void IMutex::Free(IMutex* mtx) { delete static_cast<MutexMock*>(mtx); }

class EventMock : public IEvent {
  void Signal() {}
  void Wait() {}
};

IEvent* IEvent::Allocate() { return new EventMock; }
void IEvent::Free(IEvent* e) { delete static_cast<EventMock*>(e); }

class ThreadMock : public IThread {
 public:
  explicit ThreadMock(uint32_t id) {
    id_ = id;
    ctx_ = new uint8_t[gdb_rsp::Abi::Get()->GetContextSize()];
  }
  ~ThreadMock() { delete[] ctx_; }

  uint32_t GetId() { return id_; }
  State GetState() { return IThread::SIGNALED; }

  bool SetStep(bool on) {
    (void) on;
    return true;
  }

  bool GetRegister(uint32_t index, void *dst, uint32_t len) {
    const gdb_rsp::Abi* abi = gdb_rsp::Abi::Get();
    const gdb_rsp::Abi::RegDef *reg = abi->GetRegisterDef(index);
    memcpy(dst, ctx_ + reg->offset_, len);
    return true;
  }

  bool SetRegister(uint32_t index, void *src, uint32_t len) {
    const gdb_rsp::Abi* abi = gdb_rsp::Abi::Get();
    const gdb_rsp::Abi::RegDef *reg = abi->GetRegisterDef(index);
    memcpy(ctx_ + reg->offset_, src, len);
    return true;
  }

  bool Suspend() { return true; }
  bool Resume()  { return true; }

  virtual void *GetContext() { return ctx_; }

 private:
  uint8_t *ctx_;
  uint32_t id_;
};

IThread* IThread::Acquire(uint32_t id, bool create) {
  if (create) return new ThreadMock(id);
  return NULL;
}


bool port::IPlatform::GetMemory(uint64_t addr, uint32_t len, void *dst) {
  intptr_t iptr = static_cast<intptr_t>(addr);
  void *src = reinterpret_cast<void*>(iptr);
  memcpy(dst, src, len);
  return true;
}

bool port::IPlatform::SetMemory(uint64_t addr, uint32_t len, void *src) {
  intptr_t iptr = static_cast<intptr_t>(addr);
  void *dst = reinterpret_cast<void*>(iptr);
  memcpy(dst, src, len);
  return true;
}


}  // End of namespace port

int main(int argc, const char *argv[]) {
  int errs = 0;

  (void) argc;
  (void) argv;

  printf("Testing Utils.\n");
  errs += TestUtil();

  printf("Testing ABI.\n");
  errs += TestAbi();

  printf("Testing Packets.\n");
  errs += TestPacket();

  printf("Testing Session.\n");
  errs += TestSession();

  printf("Testing Host.\n");
  errs += TestHost();

  printf("Testing Target.\n");
  errs += TestTarget();

  if (errs) printf("FAILED with %d errors.\n", errs);
  return errs;
}

