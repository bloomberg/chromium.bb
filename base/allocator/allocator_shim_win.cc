// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <malloc.h>
#include <new.h>
#include <windows.h>

#include "base/basictypes.h"

// This shim make it possible to perform additional checks on allocations
// before passing them to the Heap functions.

// new_mode behaves similarly to MSVC's _set_new_mode.
// If flag is 0 (default), calls to malloc will behave normally.
// If flag is 1, calls to malloc will behave like calls to new,
// and the std_new_handler will be invoked on failure.
// Can be set by calling _set_new_mode().
static int new_mode = 0;

namespace {

// This is a simple allocator based on the windows heap.
const size_t kWindowsPageSize = 4096;
const size_t kMaxWindowsAllocation = INT_MAX - kWindowsPageSize;
static HANDLE win_heap;

// VS2013 crt uses the process heap as its heap, so we do the same here.
// See heapinit.c in VS CRT sources.
bool win_heap_init() {
  win_heap = GetProcessHeap();
  if (win_heap == NULL)
    return false;

  ULONG enable_lfh = 2;
  // NOTE: Setting LFH may fail.  Vista already has it enabled.
  //       And under the debugger, it won't use LFH.  So we
  //       ignore any errors.
  HeapSetInformation(win_heap, HeapCompatibilityInformation, &enable_lfh,
                     sizeof(enable_lfh));

  return true;
}

void* win_heap_malloc(size_t size) {
  if (size < kMaxWindowsAllocation)
    return HeapAlloc(win_heap, 0, size);
  return NULL;
}

void win_heap_free(void* size) {
  HeapFree(win_heap, 0, size);
}

void* win_heap_realloc(void* ptr, size_t size) {
  if (!ptr)
    return win_heap_malloc(size);
  if (!size) {
    win_heap_free(ptr);
    return NULL;
  }
  if (size < kMaxWindowsAllocation)
    return HeapReAlloc(win_heap, 0, ptr, size);
  return NULL;
}

size_t win_heap_msize(void* ptr) {
  return HeapSize(win_heap, 0, ptr);
}

void* win_heap_memalign(size_t alignment, size_t size) {
  // Reserve enough space to ensure we can align and set aligned_ptr[-1] to the
  // original allocation for use with win_heap_memalign_free() later.
  size_t allocation_size = size + (alignment - 1) + sizeof(void*);

  // Check for overflow.  Alignment and size are checked in allocator_shim.
  if (size >= allocation_size || alignment >= allocation_size) {
    return NULL;
  }

  // Since we're directly calling the allocator function, before OOM handling,
  // we need to NULL check to ensure the allocation succeeded.
  void* ptr = win_heap_malloc(allocation_size);
  if (!ptr)
    return ptr;

  char* aligned_ptr = static_cast<char*>(ptr) + sizeof(void*);
  aligned_ptr +=
      alignment - reinterpret_cast<uintptr_t>(aligned_ptr) & (alignment - 1);

  reinterpret_cast<void**>(aligned_ptr)[-1] = ptr;
  return aligned_ptr;
}

void win_heap_memalign_free(void* ptr) {
  if (ptr)
    win_heap_free(static_cast<void**>(ptr)[-1]);
}

void win_heap_term() {
  win_heap = NULL;
}

}  // namespace

// Call the new handler, if one has been set.
// Returns true on successfully calling the handler, false otherwise.
inline bool call_new_handler(bool nothrow, size_t size) {
  // Get the current new handler.
  _PNH nh = _query_new_handler();
#if defined(_HAS_EXCEPTIONS) && !_HAS_EXCEPTIONS
  if (!nh)
    return false;
  // Since exceptions are disabled, we don't really know if new_handler
  // failed.  Assume it will abort if it fails.
  return nh(size);
#else
#error "Exceptions in allocator shim are not supported!"
#endif  // defined(_HAS_EXCEPTIONS) && !_HAS_EXCEPTIONS
  return false;
}

extern "C" {

void* malloc(size_t size) {
  void* ptr;
  for (;;) {
    ptr = win_heap_malloc(size);
    if (ptr)
      return ptr;

    if (!new_mode || !call_new_handler(true, size))
      break;
  }
  return ptr;
}

void free(void* p) {
  win_heap_free(p);
  return;
}

void* realloc(void* ptr, size_t size) {
  // Webkit is brittle for allocators that return NULL for malloc(0).  The
  // realloc(0, 0) code path does not guarantee a non-NULL return, so be sure
  // to call malloc for this case.
  if (!ptr)
    return malloc(size);

  void* new_ptr;
  for (;;) {
    new_ptr = win_heap_realloc(ptr, size);

    // Subtle warning:  NULL return does not alwas indicate out-of-memory.  If
    // the requested new size is zero, realloc should free the ptr and return
    // NULL.
    if (new_ptr || !size)
      return new_ptr;
    if (!new_mode || !call_new_handler(true, size))
      break;
  }
  return new_ptr;
}


size_t _msize(void* p) {
  return win_heap_msize(p);
}

intptr_t _get_heap_handle() {
  return reinterpret_cast<intptr_t>(win_heap);
}

// The CRT heap initialization stub.
int _heap_init() {
  return win_heap_init() ? 1 : 0;
}

// The CRT heap cleanup stub.
void _heap_term() {
  win_heap_term();
}

// We set this to 1 because part of the CRT uses a check of _crtheap != 0
// to test whether the CRT has been initialized.  Once we've ripped out
// the allocators from libcmt, we need to provide this definition so that
// the rest of the CRT is still usable.
void* _crtheap = reinterpret_cast<void*>(1);

// Provide support for aligned memory through Windows only _aligned_malloc().
void* _aligned_malloc(size_t size, size_t alignment) {
  // _aligned_malloc guarantees parameter validation, so do so here.  These
  // checks are somewhat stricter than _aligned_malloc() since we're effectively
  // using memalign() under the hood.
  if (size == 0U || (alignment & (alignment - 1)) != 0U ||
      (alignment % sizeof(void*)) != 0U)
    return NULL;

  void* ptr;
  for (;;) {
    ptr = win_heap_memalign(alignment, size);

    if (ptr) {
      return ptr;
    }

    if (!new_mode || !call_new_handler(true, size))
      break;
  }
  return ptr;
}

void _aligned_free(void* p) {
  // Pointers allocated with win_heap_memalign() MUST be freed via
  // win_heap_memalign_free() since the aligned pointer is not the real one.
  win_heap_memalign_free(p);
}

#include "generic_allocators.cc"

}  // extern C
