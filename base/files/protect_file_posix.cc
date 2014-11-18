// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/hash_tables.h"
#include "base/files/file.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/lock.h"

// These hooks provided for base::File perform additional sanity checks when
// files are closed. These extra checks are hard to understand and maintain,
// hence they are temporary. TODO(pasko): Remove these extra checks as soon as
// crbug.com/424562 is fixed.
//
// Background:
//   1. The browser process crashes if a call to close() provided by the C
//      library (i.e. close(3)) fails. This is done for security purposes. See
//      base/files/scoped_file.cc. When a crash like this happens, there is not
//      enough context in the minidump to triage the problem.
//   2. base::File provides a good abstraction to prevent closing incorrect
//      file descriptors or double-closing. Closing non-owned file descriptors
//      would more likely happen from outside base::File and base::ScopedFD.
//
// These hooks intercept base::File operations to 'protect' file handles /
// descriptors from accidental close(3) by other portions of the code being
// linked into the browser. Also, this file provides an interceptor for the
// close(3) itself, and requires to be linked with cooperation of
// --Wl,--wrap=close (i.e. linker wrapping).
//
// Wrapping close(3) on all libraries can lead to confusion, particularly for
// the libraries that do not use ::base (I am also looking at you,
// crazy_linker). Instead two hooks are added to base::File, which are
// implemented as no-op by default. This file should be linked into the Chrome
// native binary(-ies) for a whitelist of targets where "file descriptor
// protection" is useful.

// With compilers other than GCC/Clang the wrapper is trivial. This is to avoid
// overexercising mechanisms for overriding weak symbols.
#if !defined(COMPILER_GCC)
extern "C" {

int __real_close(int fd);

BASE_EXPORT int __wrap_close(int fd) {
  return __real_close(fd);
}

}  // extern "C"

#else  // defined(COMPILER_GCC)

namespace {

// Protects the |g_protected_fd_set|.
base::LazyInstance<base::Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;

// Holds the set of all 'protected' file descriptors.
base::LazyInstance<base::hash_set<int> >::Leaky g_protected_fd_set =
    LAZY_INSTANCE_INITIALIZER;

bool IsFileDescriptorProtected(int fd) {
  base::AutoLock lock(*g_lock.Pointer());
  return g_protected_fd_set.Get().count(fd) != 0;
}

}  // namespace

namespace base {

BASE_EXPORT void ProtectFileDescriptor(int fd) {
  base::AutoLock lock(g_lock.Get());
  CHECK(!g_protected_fd_set.Get().count(fd)) << "fd: " << fd;
  g_protected_fd_set.Get().insert(fd);
}

BASE_EXPORT void UnprotectFileDescriptor(int fd) {
  base::AutoLock lock(*g_lock.Pointer());
  CHECK(g_protected_fd_set.Get().erase(fd)) << "fd: " << fd;
}

}  // namespace base

extern "C" {

int __real_close(int fd);

BASE_EXPORT int __wrap_close(int fd) {
  // The crash happens here if a protected file descriptor was attempted to be
  // closed without first being unprotected. Unprotection happens only in
  // base::File. In other words this is an "early crash" as compared to the one
  // happening in scoped_file.cc.
  //
  // Getting an earlier crash provides a more useful stack trace (minidump)
  // allowing to debug deeper into the thread that freed the wrong resource.
  CHECK(!IsFileDescriptorProtected(fd)) << "fd: " << fd;

  // Crash by the same reason as in scoped_file.cc.
  PCHECK(0 == IGNORE_EINTR(__real_close(fd)));
  return 0;
}

}  // extern "C"

#endif  // defined(COMPILER_GCC)
