// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>

#include "chrome/browser/process_singleton.h"

#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "chrome/common/chrome_constants.h"

namespace {

// From "man 2 intro", the largest errno is |EOPNOTSUPP|, which is
// |102|.  Since the histogram memory usage is proportional to this
// number, using the |102| directly rather than the macro.
const int kMaxErrno = 102;

}  // namespace

// This class is used to funnel messages to a single instance of the browser
// process. This is needed for several reasons on other platforms.
//
// On Windows, when the user re-opens the application from the shell (e.g. an
// explicit double-click, a shortcut that opens a webpage, etc.) we need to send
// the message to the currently-existing copy of the browser.
//
// On Linux, opening a URL is done by creating an instance of the web browser
// process and passing it the URL to go to on its commandline.
//
// Neither of those cases apply on the Mac. Launch Services ensures that there
// is only one instance of the process, and we get URLs to open via AppleEvents
// and, once again, the Launch Services system. We have no need to manage this
// ourselves.  An exclusive lock is used to flush out anyone making incorrect
// assumptions.

ProcessSingleton::ProcessSingleton(const FilePath& user_data_dir)
    : locked_(false),
      foreground_window_(NULL),
      lock_path_(user_data_dir.Append(chrome::kSingletonLockFilename)),
      lock_fd_(-1) {
}

ProcessSingleton::~ProcessSingleton() {
  // Make sure the lock is released.  Process death will also release
  // it, even if this is not called.
  Cleanup();
}

ProcessSingleton::NotifyResult ProcessSingleton::NotifyOtherProcess() {
  // This space intentionally left blank.
  return PROCESS_NONE;
}

ProcessSingleton::NotifyResult ProcessSingleton::NotifyOtherProcessOrCreate() {
  // Windows tries NotifyOtherProcess() first.
  return Create() ? PROCESS_NONE : PROFILE_IN_USE;
}

// Attempt to acquire an exclusive lock on an empty file in the
// profile directory.  Returns |true| if it gets the lock.  Returns
// |false| if the lock is held, or if there is an error.
// TODO(shess): Rather than logging failures, popup an alert.  Punting
// that for now because it would require confidence that this code is
// never called in a situation where an alert wouldn't work.
// http://crbug.com/59061
bool ProcessSingleton::Create() {
  DCHECK_EQ(-1, lock_fd_) << "lock_path_ is already open.";

  lock_fd_ = HANDLE_EINTR(open(lock_path_.value().c_str(),
                               O_RDONLY | O_CREAT, 0644));
  if (lock_fd_ == -1) {
    const int capture_errno = errno;
    DPCHECK(lock_fd_ != -1) << "Unexpected failure opening profile lockfile";
    UMA_HISTOGRAM_ENUMERATION("ProcessSingleton.OpenError",
                              capture_errno, kMaxErrno);
    return false;
  }

  // Acquire an exclusive lock in non-blocking fashion.  If the lock
  // is already held, this will return |EWOULDBLOCK|.
  int rc = HANDLE_EINTR(flock(lock_fd_, LOCK_EX|LOCK_NB));
  if (rc == -1) {
    const int capture_errno = errno;
    DPCHECK(errno == EWOULDBLOCK)
        << "Unexpected failure locking profile lockfile";

    Cleanup();

    // Other errors indicate something crazy is happening.
    if (capture_errno != EWOULDBLOCK) {
      UMA_HISTOGRAM_ENUMERATION("ProcessSingleton.LockError",
                                capture_errno, kMaxErrno);
      return false;
    }

    // The file is open by another process and locked.
    LOG(ERROR) << "Unable to obtain profile lock.";
    return false;
  }

  return true;
}

void ProcessSingleton::Cleanup() {
  // Closing the file also releases the lock.
  if (lock_fd_ != -1) {
    int rc = HANDLE_EINTR(close(lock_fd_));
    DPCHECK(!rc) << "Closing lock_fd_:";
  }
  lock_fd_ = -1;
}
