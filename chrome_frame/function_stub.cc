// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/function_stub.h"

#include <new>

#include "base/synchronization/lock.h"
#include "base/logging.h"

#ifndef _M_IX86
#error Only x86 supported right now.
#endif

namespace {
typedef enum AsmConstants {
  POP_EAX = 0x58,
  PUSH_IND = 0x35ff,
  PUSH_EAX = 0x50,
  JUMP_IND = 0x25ff,
};

// A quick and dirty wrapper class that allows us to defer allocating
// the executable heap until first use, and to release it teardown.
class ExecutableHeap {
 public:
  ExecutableHeap() : heap_(NULL) {
  }

  ~ExecutableHeap() {
    if (heap_ != NULL) {
      BOOL ret = ::HeapDestroy(heap_);
      heap_ = NULL;
    }
  }

  void* Allocate(size_t size) {
    if (!heap_)
      CreateHeap();

    DCHECK(heap_);

    return ::HeapAlloc(heap_, 0, size);
  }

  void Free(void* ptr) {
    DCHECK(heap_ != NULL);
    ::HeapFree(heap_, 0, ptr);
  }

  void CreateHeap() {
    base::AutoLock lock(init_lock_);

    if (heap_ == NULL)
      heap_ = ::HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 0, 0);
  }

 private:
  base::Lock init_lock_;
  HANDLE heap_;
};

// Our executable heap instance, all stubs are allocated from here.
ExecutableHeap heap_;

}  // namespace

extern "C" IMAGE_DOS_HEADER __ImageBase;

bool FunctionStub::is_valid() const {
  return signature_ == reinterpret_cast<HMODULE>(&__ImageBase) &&
      !is_bypassed();
}

FunctionStub::FunctionStub(uintptr_t extra_argument, void* dest)
    : signature_(reinterpret_cast<HMODULE>(&__ImageBase)),
      argument_(extra_argument),
      destination_function_(reinterpret_cast<uintptr_t>(dest)) {
  bypass_address_ = reinterpret_cast<uintptr_t>(&stub_.pop_return_addr_);
  Init(&stub_);
}

FunctionStub::~FunctionStub() {
}

void FunctionStub::Init(FunctionStubAsm* stub) {
  DCHECK(stub != NULL);

  stub->jump_to_bypass_ = JUMP_IND;
  stub->bypass_target_addr_ = reinterpret_cast<uintptr_t>(&bypass_address_);
  stub->pop_return_addr_ = POP_EAX;
  stub->push_ = PUSH_IND;
  stub->arg_addr_ = reinterpret_cast<uintptr_t>(&argument_);
  stub->push_return_addr_ = PUSH_EAX;
  stub->jump_to_target = JUMP_IND;
  stub->target_addr_ = reinterpret_cast<uintptr_t>(&destination_function_);

  // Flush the instruction cache for the newly written code.
  BOOL ret = ::FlushInstructionCache(::GetCurrentProcess(),
                                     stub,
                                     sizeof(*stub));
}

void FunctionStub::BypassStub(void* new_target) {
  set_bypass_address(reinterpret_cast<uintptr_t>(new_target));
}

FunctionStub* FunctionStub::Create(uintptr_t extra_argument, void* dest) {
  DCHECK(dest);
  FunctionStub* stub =
      reinterpret_cast<FunctionStub*>(heap_.Allocate(sizeof(FunctionStub)));

  if (stub != NULL)
    new (stub) FunctionStub(extra_argument, dest);

  return stub;
}

FunctionStub* FunctionStub::FromCode(void* address) {
  // Address points to arbitrary code here, which may e.g.
  // lie at the end of an executable segment, which in turn
  // may terminate earlier than the last address we probe.
  // We therefore execute under an SEH, so as not to crash
  // on failed probes.
  __try {
    // Retrieve the candidata function stub.
    FunctionStub* candidate = CONTAINING_RECORD(address, FunctionStub, stub_);
    if (candidate->stub_.jump_to_bypass_ == JUMP_IND &&
        candidate->signature_ == reinterpret_cast<HMODULE>(&__ImageBase)) {
      return candidate;
    }
  } __except(EXCEPTION_EXECUTE_HANDLER) {
  }

  return NULL;
}

bool FunctionStub::Destroy(FunctionStub* stub) {
  heap_.Free(stub);

  return true;
}
