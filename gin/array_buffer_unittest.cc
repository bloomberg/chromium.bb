// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/array_buffer.h"
#include "build/build_config.h"
#include "gin/per_isolate_data.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"

#if defined(OS_POSIX)
#include <setjmp.h>
#include <signal.h>
#endif

namespace gin {

using ArrayBufferTest = V8Test;

namespace {
const size_t kBufferLength = 65536;
}

TEST_F(ArrayBufferTest, AllocateAndFreeBuffer) {
  v8::Isolate* const isolate = instance_->isolate();
  v8::ArrayBuffer::Allocator* const allocator =
      PerIsolateData::From(isolate)->allocator();

  void* buffer = allocator->Allocate(kBufferLength);
  allocator->Free(buffer, kBufferLength);
}

TEST_F(ArrayBufferTest, ReserveAndReleaseBuffer) {
  v8::Isolate* const isolate = instance_->isolate();
  v8::ArrayBuffer::Allocator* const allocator =
      PerIsolateData::From(isolate)->allocator();

  void* buffer = allocator->Reserve(kBufferLength);
  allocator->Free(buffer, kBufferLength,
                  v8::ArrayBuffer::Allocator::AllocationMode::kReservation);
}

TEST_F(ArrayBufferTest, SetProtectionReadWrite) {
  v8::Isolate* const isolate = instance_->isolate();
  v8::ArrayBuffer::Allocator* const allocator =
      PerIsolateData::From(isolate)->allocator();

  void* buffer = allocator->Reserve(kBufferLength);
  allocator->SetProtection(buffer, kBufferLength,
                           v8::ArrayBuffer::Allocator::Protection::kReadWrite);
  volatile int* int_buffer = static_cast<volatile int*>(buffer);
  // Try assigning to the buffer. This will fault if we don't SetProtection
  // first.
  int_buffer[0] = 42;
  allocator->Free(buffer, kBufferLength,
                  v8::ArrayBuffer::Allocator::AllocationMode::kReservation);
}

#if defined(OS_POSIX)

namespace {
sigjmp_buf g_continuation_;

void SignalHandler(int signal, siginfo_t* info, void*) {
  siglongjmp(g_continuation_, 1);
}
}  // namespace

TEST_F(ArrayBufferTest, ReservationReadOnlyByDefault) {
  v8::Isolate* const isolate = instance_->isolate();
  v8::ArrayBuffer::Allocator* const allocator =
      PerIsolateData::From(isolate)->allocator();

  void* buffer = allocator->Reserve(kBufferLength);
  volatile int* int_buffer = static_cast<volatile int*>(buffer);

  // Install a signal handler so we can catch the fault we're about to trigger.
  struct sigaction action = {};
  struct sigaction old_action = {};
  action.sa_sigaction = SignalHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &action, &old_action);
#if defined(OS_MACOSX)
  // On Mac, sometimes we get SIGBUS instead of SIGSEGV.
  struct sigaction old_bus_action;
  sigaction(SIGBUS, &action, &old_bus_action);
#endif

  int const save_sigs = 1;
  if (!sigsetjmp(g_continuation_, save_sigs)) {
    // Try assigning to the buffer. This will fault if we don't SetProtection
    // first.
    int_buffer[0] = 42;
  } else {
    // if sigsetjmp returns nonzero, then we are returning from our handler.

    sigaction(SIGSEGV, &old_action, nullptr);
#if defined(OS_MACOSX)
    sigaction(SIGBUS, &old_bus_action, nullptr);
#endif

    allocator->Free(buffer, kBufferLength,
                    v8::ArrayBuffer::Allocator::AllocationMode::kReservation);
    SUCCEED();
    return;
  }

  FAIL();
}

#endif  // OS_POSIX

}  // namespace gin
