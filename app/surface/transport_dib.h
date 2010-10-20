// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_SURFACE_TRANSPORT_DIB_H_
#define APP_SURFACE_TRANSPORT_DIB_H_
#pragma once

#include "base/basictypes.h"
#include "base/process.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "base/shared_memory.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#elif defined(USE_X11)
#include "app/x11_util.h"
#endif

namespace skia {
class PlatformCanvas;
}

// -----------------------------------------------------------------------------
// A TransportDIB is a block of memory that is used to transport pixels
// between processes: from the renderer process to the browser, and
// between renderer and plugin processes.
// -----------------------------------------------------------------------------
class TransportDIB {
 public:
  ~TransportDIB();

  // Two typedefs are defined. A |Handle| can be sent over the wire so that the
  // remote side can map the |TransportDIB|. These handles may be reused from
  // previous DIBs. An |Id| is unique and never reused, but it is not sufficient
  // to map the DIB.
#if defined(OS_WIN)
  // On Windows, a |Handle| is a combination of the section (i.e., file mapping)
  // handle and the ID of the corresponding process. When the DIB is mapped in
  // a remote process, the section handle is duplicated for use in that process.
  // However, if the remote process does not have permission to duplicate the
  // handle, the first process must duplicate the handle before sending it.
  // E.g., this is necessary if the DIB is created in the browser and will be
  // mapped in the sandboxed renderer.
  class TransferrableSectionHandle {
   public:
    TransferrableSectionHandle()
        : section_(NULL), owner_id_(NULL), should_dup_on_map_(false) {
    }

    TransferrableSectionHandle(HANDLE section, base::ProcessId owner_id,
                               bool should_dup_on_map)
        : section_(section),
          owner_id_(owner_id),
          should_dup_on_map_(should_dup_on_map) {
    }

    // Duplicates the handle for use in the given process.
    TransferrableSectionHandle DupForProcess(
        base::ProcessHandle new_owner) const;

    // Closes this handle. This should be called if this handle was duplicated
    // and is not owned by a TransportDIB.
    void Close() const;

    // Returns true if this handle refers to an actual file mapping.
    bool is_valid() const { return section_ != NULL && owner_id_ != NULL; }

    HANDLE section() const { return section_; }
    base::ProcessId owner_id() const { return owner_id_; }
    bool should_dup_on_map() const { return should_dup_on_map_; }

   private:
    HANDLE section_;
    base::ProcessId owner_id_;
    // Whether the handle should be duplicated when the DIB is mapped.
    bool should_dup_on_map_;
  };
  typedef TransferrableSectionHandle Handle;

  // On Windows, the Id type is a sequence number (epoch) to solve an ABA
  // issue:
  //   1) Process A creates a transport DIB with HANDLE=1 and sends to B.
  //   2) Process B maps the transport DIB and caches 1 -> DIB.
  //   3) Process A closes the transport DIB and creates a new one. The new DIB
  //      is also assigned HANDLE=1.
  //   4) Process A sends the Handle to B, but B incorrectly believes that it
  //      already has it cached.
  typedef uint32 Id;

  // Returns a default, invalid handle, that is meant to indicate a missing
  // Transport DIB.
  static Handle DefaultHandleValue() { return Handle(); }

  // Returns a value that is ONLY USEFUL FOR TESTS WHERE IT WON'T BE
  // ACTUALLY USED AS A REAL HANDLE.
  static Handle GetFakeHandleForTest() {
    static int fake_handle = 10;
    return Handle(reinterpret_cast<HANDLE>(fake_handle++), 1, false);
  }
#elif defined(OS_MACOSX)
  typedef base::SharedMemoryHandle Handle;
  // On Mac, the inode number of the backing file is used as an id.
  typedef base::SharedMemoryId Id;

  // Returns a default, invalid handle, that is meant to indicate a missing
  // Transport DIB.
  static Handle DefaultHandleValue() { return Handle(); }

  // Returns a value that is ONLY USEFUL FOR TESTS WHERE IT WON'T BE
  // ACTUALLY USED AS A REAL HANDLE.
  static Handle GetFakeHandleForTest() {
    static int fake_handle = 10;
    return Handle(fake_handle++, false);
  }
#elif defined(USE_X11)
  typedef int Handle;  // These two ints are SysV IPC shared memory keys
  typedef int Id;

  // Returns a default, invalid handle, that is meant to indicate a missing
  // Transport DIB.
  static Handle DefaultHandleValue() { return -1; }

  // Returns a value that is ONLY USEFUL FOR TESTS WHERE IT WON'T BE
  // ACTUALLY USED AS A REAL HANDLE.
  static Handle GetFakeHandleForTest() {
    static int fake_handle = 10;
    return fake_handle++;
  }
#endif

  // When passing a TransportDIB::Handle across processes, you must always close
  // the handle, even if you return early, or the handle will be leaked. Typical
  // usage will be:
  //
  //   MyIPCHandler(TransportDIB::Handle dib_handle) {
  //     TransportDIB::ScopedHandle handle_scoper(dib_handle);
  //     ... do some stuff, possible returning early ...
  //
  //     TransportDIB* dib = TransportDIB::Map(handle_scoper.release());
  //     // The handle lifetime is now managed by the TransportDIB.
  class ScopedHandle {
   public:
    ScopedHandle() : handle_(DefaultHandleValue()) {}
    explicit ScopedHandle(Handle handle) : handle_(handle) {}

    ~ScopedHandle() {
      Close();
    }

    Handle release() {
      Handle temp = handle_;
      handle_ = DefaultHandleValue();
      return temp;
    }

    operator Handle() { return handle_; }

   private:
    void Close();

    Handle handle_;
    DISALLOW_COPY_AND_ASSIGN(ScopedHandle);
  };

  // Create a new |TransportDIB|, returning NULL on failure.
  //
  // The size is the minimum size in bytes of the memory backing the transport
  // DIB (we may actually allocate more than that to give us better reuse when
  // cached).
  //
  // The sequence number is used to uniquely identify the transport DIB. It
  // should be unique for all transport DIBs ever created in the same
  // renderer.
  //
  // On Linux, this will also map the DIB into the current process.
  static TransportDIB* Create(size_t size, uint32 sequence_num);

  // Map the referenced transport DIB. The caller owns the returned object.
  // Returns NULL on failure.
  static TransportDIB* Map(Handle transport_dib);

  // Create a new |TransportDIB| with a handle to the shared memory. This
  // always returns a valid pointer. The DIB is not mapped.
  static TransportDIB* CreateWithHandle(Handle handle);

  // Returns true if the handle is valid.
  static bool is_valid(Handle dib);

  // Returns a canvas using the memory of this TransportDIB. The returned
  // pointer will be owned by the caller. The bitmap will be of the given size,
  // which should fit inside this memory.
  //
  // On POSIX, this |TransportDIB| will be mapped if not already. On Windows,
  // this |TransportDIB| will NOT be mapped and should not be mapped prior,
  // because PlatformCanvas will map the file internally.
  //
  // Will return NULL on allocation failure. This could be because the image
  // is too large to map into the current process' address space.
  skia::PlatformCanvas* GetPlatformCanvas(int w, int h);

  // Map the DIB into the current process if it is not already. This is used to
  // map a DIB that has already been created. Returns true if the DIB is mapped.
  bool Map();

  // Return a handle for use in a specific process. On POSIX, this simply
  // returns the handle as in the |handle| accessor below. On Windows, this
  // returns a duplicate handle for use in the given process. This should be
  // used instead of the |handle| accessor only if the process that will map
  // this DIB does not have permission to duplicate the handle from the
  // first process.
  //
  // Note: On Windows, if the duplicated handle is not closed by the other side
  // (or this process fails to transmit the handle), the shared memory will be
  // leaked.
  Handle GetHandleForProcess(base::ProcessHandle process_handle) const;

  // Return a pointer to the shared memory.
  void* memory() const;

  // Return the maximum size of the shared memory. This is not the amount of
  // data which is valid, you have to know that via other means, this is simply
  // the maximum amount that /could/ be valid.
  size_t size() const { return size_; }

  // Return the identifier which can be used to refer to this shared memory
  // on the wire.
  Id id() const;

  // Return a handle to the underlying shared memory. This can be sent over the
  // wire to give this transport DIB to another process.
  Handle handle() const;

#if defined(USE_X11)
  // Map the shared memory into the X server and return an id for the shared
  // segment.
  XID MapToX(Display* connection);
#endif

 private:
  TransportDIB();
#if defined(OS_WIN) || defined(OS_MACOSX)
  explicit TransportDIB(base::SharedMemoryHandle dib);
  base::SharedMemory shared_memory_;
  uint32 sequence_num_;
#elif defined(USE_X11)
  int key_;  // SysV shared memory id
  void* address_;  // mapped address
  XSharedMemoryId x_shm_;  // X id for the shared segment
  Display* display_;  // connection to the X server
#endif
  size_t size_;  // length, in bytes

  DISALLOW_COPY_AND_ASSIGN(TransportDIB);
};

#endif  // APP_SURFACE_TRANSPORT_DIB_H_
