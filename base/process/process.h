// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROCESS_PROCESS_PROCESS_H_
#define BASE_PROCESS_PROCESS_PROCESS_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/move.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base {

// Provides a move-only encapsulation of a process.
//
// This object is not tied to the lifetime of the underlying process: the
// process may be killed and this object may still around, and it will still
// claim to be valid. The actual behavior in that case is OS dependent like so:
//
// Windows: The underlying ProcessHandle will be valid after the process dies
// and can be used to gather some information about that process, but most
// methods will obviously fail.
//
// POSIX: The underlying PorcessHandle is not guaranteed to remain valid after
// the process dies, and it may be reused by the system, which means that it may
// end up pointing to the wrong process.
class BASE_EXPORT Process {
  MOVE_ONLY_TYPE_FOR_CPP_03(Process, RValue)

 public:
  explicit Process(ProcessHandle handle = kNullProcessHandle);

  // Move constructor for C++03 move emulation of this type.
  Process(RValue other);

  // The destructor does not terminate the process.
  ~Process() {}

  // Move operator= for C++03 move emulation of this type.
  Process& operator=(RValue other);

  // Returns an object for the current process.
  static Process Current();

  // Creates an object from a |handle| owned by someone else.
  // Don't use this for new code. It is only intended to ease the migration to
  // a strict ownership model.
  // TODO(rvargas) crbug.com/417532: Remove this code.
  static Process DeprecatedGetProcessFromHandle(ProcessHandle handle);

  // Returns true if processes can be backgrounded.
  static bool CanBackgroundProcesses();

  // Returns true if this objects represents a valid process.
  bool IsValid() const;

  // Returns a handle for this process. There is no guarantee about when that
  // handle becomes invalid because this object retains ownership.
  ProcessHandle Handle() const;

  // Returns a second object that represents this process.
  Process Duplicate() const;

  // Get the PID for this process.
  ProcessId pid() const;

  // Returns true if this process is the current process.
  bool is_current() const;

  // Close the process handle. This will not terminate the process.
  void Close();

  // Terminates the process with extreme prejudice. The given |result_code| will
  // be the exit code of the process.
  // NOTE: On POSIX |result_code| is ignored.
  void Terminate(int result_code);

  // A process is backgrounded when it's priority is lower than normal.
  // Return true if this process is backgrounded, false otherwise.
  bool IsProcessBackgrounded() const;

  // Set a process as backgrounded. If value is true, the priority of the
  // process will be lowered. If value is false, the priority of the process
  // will be made "normal" - equivalent to default process priority.
  // Returns true if the priority was changed, false otherwise.
  bool SetProcessBackgrounded(bool value);

  // Returns an integer representing the priority of a process. The meaning
  // of this value is OS dependent.
  int GetPriority() const;

 private:
#if defined(OS_WIN)
  bool is_current_process_;
  win::ScopedHandle process_;
#else
  ProcessHandle process_;
#endif
};

}  // namespace base

#endif  // BASE_PROCESS_PROCESS_PROCESS_H_
