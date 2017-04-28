// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SHARED_MEMORY_HANDLE_H_
#define BASE_MEMORY_SHARED_MEMORY_HANDLE_H_

#include <stddef.h>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/process/process_handle.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach.h>
#include "base/base_export.h"
#include "base/file_descriptor_posix.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#elif defined(OS_POSIX)
#include <sys/types.h>
#include "base/file_descriptor_posix.h"
#endif

namespace base {

// SharedMemoryHandle is a platform specific type which represents
// the underlying OS handle to a shared memory segment.
// TODO(erikchen): Refactor this to create a single class on all platforms,
// rather than 3 separate class definitions that are very similar.
// https://crbug.com/713763.
#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
class BASE_EXPORT SharedMemoryHandle {
 public:
  // The default constructor returns an invalid SharedMemoryHandle.
  SharedMemoryHandle();

  // This constructor is deprecated, as it fails to propagate the GUID, which
  // will be added in the near future.
  // TODO(rockot): Remove this constructor once Mojo supports GUIDs.
  // https://crbug.com/713763.
  explicit SharedMemoryHandle(const base::FileDescriptor& file_descriptor);

  // Creates a SharedMemoryHandle from an |fd| supplied from an external
  // service.
  static SharedMemoryHandle ImportHandle(int fd);

  // Returns the underlying OS resource.
  int GetHandle() const;

  // Takes ownership of the OS resource.
  void SetHandle(int fd);

  // Whether the underlying OS resource is valid.
  bool IsValid() const;

  // Closes the underlying OS resource.
  // The fact that this method needs to be "const" is an artifact of the
  // original interface for base::SharedMemory::CloseHandle.
  // TODO(erikchen): This doesn't clear the underlying reference, which seems
  // like a bug, but is how this class has always worked. Fix this:
  // https://crbug.com/716072.
  void Close() const;

  // Invalidates [but doesn't close] the underlying OS resource. This will leak
  // unless the caller is careful.
  int Release();

  // Duplicates the underlying OS resource.
  SharedMemoryHandle Duplicate() const;

  // If this returns true, then ownership of the underlying OS resource is
  // implicitly passed to the IPC subsystem during serialization.
  void SetOwnershipPassesToIPC(bool ownership_passes);
  bool OwnershipPassesToIPC() const;

 private:
  // This must be public to support serialization by Chrome IPC.
  FileDescriptor file_descriptor_;
};
#elif defined(OS_WIN)
class BASE_EXPORT SharedMemoryHandle {
 public:
  // The default constructor returns an invalid SharedMemoryHandle.
  SharedMemoryHandle();
  SharedMemoryHandle(HANDLE h, base::ProcessId pid);

  // Standard copy constructor. The new instance shares the underlying OS
  // primitives.
  SharedMemoryHandle(const SharedMemoryHandle& handle);

  // Standard assignment operator. The updated instance shares the underlying
  // OS primitives.
  SharedMemoryHandle& operator=(const SharedMemoryHandle& handle);

  // Comparison operators.
  bool operator==(const SharedMemoryHandle& handle) const;
  bool operator!=(const SharedMemoryHandle& handle) const;

  // Closes the underlying OS resources.
  void Close() const;

  // Whether the underlying OS primitive is valid.
  bool IsValid() const;

  // Whether |pid_| is the same as the current process's id.
  bool BelongsToCurrentProcess() const;

  // Whether handle_ needs to be duplicated into the destination process when
  // an instance of this class is passed over a Chrome IPC channel.
  bool NeedsBrokering() const;

  void SetOwnershipPassesToIPC(bool ownership_passes);
  bool OwnershipPassesToIPC() const;

  HANDLE GetHandle() const;
  base::ProcessId GetPID() const;

 private:
  HANDLE handle_;

  // The process in which |handle_| is valid and can be used. If |handle_| is
  // invalid, this will be kNullProcessId.
  base::ProcessId pid_;

  // Whether passing this object as a parameter to an IPC message passes
  // ownership of |handle_| to the IPC stack. This is meant to mimic the
  // behavior of the |auto_close| parameter of FileDescriptor. This member only
  // affects attachment-brokered SharedMemoryHandles.
  // Defaults to |false|.
  bool ownership_passes_to_ipc_;
};
#else
class BASE_EXPORT SharedMemoryHandle {
 public:
  enum Type {
    // The SharedMemoryHandle is backed by a POSIX fd.
    POSIX,
    // The SharedMemoryHandle is backed by the Mach primitive "memory object".
    MACH,
  };

  // The default constructor returns an invalid SharedMemoryHandle.
  SharedMemoryHandle();

  // Constructs a SharedMemoryHandle backed by the components of a
  // FileDescriptor. The newly created instance has the same ownership semantics
  // as base::FileDescriptor. This typically means that the SharedMemoryHandle
  // takes ownership of the |fd| if |auto_close| is true. Unfortunately, it's
  // common for existing code to make shallow copies of SharedMemoryHandle, and
  // the one that is finally passed into a base::SharedMemory is the one that
  // "consumes" the fd.
  explicit SharedMemoryHandle(const base::FileDescriptor& file_descriptor);

  // Makes a Mach-based SharedMemoryHandle of the given size. On error,
  // subsequent calls to IsValid() return false.
  explicit SharedMemoryHandle(mach_vm_size_t size);

  // Makes a Mach-based SharedMemoryHandle from |memory_object|, a named entry
  // in the task with process id |pid|. The memory region has size |size|.
  SharedMemoryHandle(mach_port_t memory_object,
                     mach_vm_size_t size,
                     base::ProcessId pid);

  // Standard copy constructor. The new instance shares the underlying OS
  // primitives.
  SharedMemoryHandle(const SharedMemoryHandle& handle);

  // Standard assignment operator. The updated instance shares the underlying
  // OS primitives.
  SharedMemoryHandle& operator=(const SharedMemoryHandle& handle);

  // Duplicates the underlying OS resources.
  SharedMemoryHandle Duplicate() const;

  // Comparison operators.
  bool operator==(const SharedMemoryHandle& handle) const;
  bool operator!=(const SharedMemoryHandle& handle) const;

  // Whether the underlying OS primitive is valid. Once the SharedMemoryHandle
  // is backed by a valid OS primitive, it becomes immutable.
  bool IsValid() const;

  // Exposed so that the SharedMemoryHandle can be transported between
  // processes.
  mach_port_t GetMemoryObject() const;

  // Returns false on a failure to determine the size. On success, populates the
  // output variable |size|.
  bool GetSize(size_t* size) const;

  // The SharedMemoryHandle must be valid.
  // Returns whether the SharedMemoryHandle was successfully mapped into memory.
  // On success, |memory| is an output variable that contains the start of the
  // mapped memory.
  bool MapAt(off_t offset, size_t bytes, void** memory, bool read_only);

  // Closes the underlying OS primitive.
  void Close() const;

  void SetOwnershipPassesToIPC(bool ownership_passes);
  bool OwnershipPassesToIPC() const;

 private:
  friend class SharedMemory;

  // Shared code between copy constructor and operator=.
  void CopyRelevantData(const SharedMemoryHandle& handle);

  Type type_;

  // Each instance of a SharedMemoryHandle is backed either by a POSIX fd or a
  // mach port. |type_| determines the backing member.
  union {
    FileDescriptor file_descriptor_;

    struct {
      mach_port_t memory_object_;

      // The size of the shared memory region when |type_| is MACH. Only
      // relevant if |memory_object_| is not |MACH_PORT_NULL|.
      mach_vm_size_t size_;

      // The pid of the process in which |memory_object_| is usable. Only
      // relevant if |memory_object_| is not |MACH_PORT_NULL|.
      base::ProcessId pid_;

      // Whether passing this object as a parameter to an IPC message passes
      // ownership of |memory_object_| to the IPC stack. This is meant to mimic
      // the behavior of the |auto_close| parameter of FileDescriptor.
      // Defaults to |false|.
      bool ownership_passes_to_ipc_;
    };
  };
};
#endif

}  // namespace base

#endif  // BASE_MEMORY_SHARED_MEMORY_HANDLE_H_
