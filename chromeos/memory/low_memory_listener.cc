// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/low_memory_listener.h"

#include <fcntl.h>

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/timer.h"
#include "chromeos/memory/low_memory_listener_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/zygote_host_linux.h"

using content::BrowserThread;

namespace chromeos {

namespace {
// This is the file that will exist if low memory notification is available
// on the device.  Whenever it becomes readable, it signals a low memory
// condition.
const char kLowMemFile[] = "/dev/chromeos-low-mem";

// This is the minimum amount of time in milliseconds between checks for
// low memory.
const int kLowMemoryCheckTimeoutMs = 750;
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// LowMemoryListenerImpl
//
// Does the actual work of observing.  The observation work happens on the FILE
// thread, and notification happens on the UI thread.  If low memory is
// detected, then we notify, wait kLowMemoryCheckTimeoutMs milliseconds and then
// start watching again to see if we're still in a low memory state.  This is to
// keep from sending out multiple notifications before the UI has a chance to
// respond (it may take the UI a while to actually deallocate memory). A timer
// isn't the perfect solution, but without any reliable indicator that a tab has
// had all its parts deallocated, it's the next best thing.
class LowMemoryListenerImpl
    : public base::RefCountedThreadSafe<LowMemoryListenerImpl> {
 public:
  LowMemoryListenerImpl() : watcher_delegate_(this), file_descriptor_(-1) {}

  // Start watching the low memory file for readability.
  // Calls to StartObserving should always be matched with calls to
  // StopObserving.  This method should only be called from the FILE thread.
  // |low_memory_callback| is run when memory is low.
  void StartObservingOnFileThread(const base::Closure& low_memory_callback);

  // Stop watching the low memory file for readability.
  // May be safely called if StartObserving has not been called.
  // This method should only be called from the FILE thread.
  void StopObservingOnFileThread();

 private:
  friend class base::RefCountedThreadSafe<LowMemoryListenerImpl>;

  ~LowMemoryListenerImpl() {
    StopObservingOnFileThread();
  }

  // Start a timer to resume watching the low memory file descriptor.
  void ScheduleNextObservation();

  // Actually start watching the file descriptor.
  void StartWatchingDescriptor();

  // Delegate to receive events from WatchFileDescriptor.
  class FileWatcherDelegate : public MessageLoopForIO::Watcher {
   public:
    explicit FileWatcherDelegate(LowMemoryListenerImpl* owner)
        : owner_(owner) {}
    virtual ~FileWatcherDelegate() {}

    // Overrides for MessageLoopForIO::Watcher
    virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {}
    virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
      LOG(WARNING) << "Low memory condition detected.  Discarding a tab.";
      // We can only discard tabs on the UI thread.
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              owner_->low_memory_callback_);
      owner_->ScheduleNextObservation();
    }

   private:
    LowMemoryListenerImpl* owner_;
    DISALLOW_COPY_AND_ASSIGN(FileWatcherDelegate);
  };

  scoped_ptr<MessageLoopForIO::FileDescriptorWatcher> watcher_;
  FileWatcherDelegate watcher_delegate_;
  int file_descriptor_;
  base::OneShotTimer<LowMemoryListenerImpl> timer_;
  base::Closure low_memory_callback_;

  DISALLOW_COPY_AND_ASSIGN(LowMemoryListenerImpl);
};

void LowMemoryListenerImpl::StartObservingOnFileThread(
    const base::Closure& low_memory_callback) {
  low_memory_callback_ = low_memory_callback;
  DCHECK_LE(file_descriptor_, 0)
      << "Attempted to start observation when it was already started.";
  DCHECK(watcher_.get() == NULL);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(MessageLoopForIO::current());

  file_descriptor_ = ::open(kLowMemFile, O_RDONLY);
  // Don't report this error unless we're really running on ChromeOS
  // to avoid testing spam.
  if (file_descriptor_ < 0 && base::chromeos::IsRunningOnChromeOS()) {
    PLOG(ERROR) << "Unable to open " << kLowMemFile;
    return;
  }
  watcher_.reset(new MessageLoopForIO::FileDescriptorWatcher);
  StartWatchingDescriptor();
}

void LowMemoryListenerImpl::StopObservingOnFileThread() {
  // If StartObserving failed, StopObserving will still get called.
  timer_.Stop();
  if (file_descriptor_ >= 0) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    watcher_.reset(NULL);
    ::close(file_descriptor_);
    file_descriptor_ = -1;
  }
}

void LowMemoryListenerImpl::ScheduleNextObservation() {
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kLowMemoryCheckTimeoutMs),
               this,
               &LowMemoryListenerImpl::StartWatchingDescriptor);
}

void LowMemoryListenerImpl::StartWatchingDescriptor() {
  DCHECK(watcher_.get());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(MessageLoopForIO::current());
  if (file_descriptor_ < 0)
    return;
  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          file_descriptor_,
          false,  // persistent=false: We want it to fire once and reschedule.
          MessageLoopForIO::WATCH_READ,
          watcher_.get(),
          &watcher_delegate_)) {
    LOG(ERROR) << "Unable to watch " << kLowMemFile;
  }
}

////////////////////////////////////////////////////////////////////////////////
// LowMemoryListener

LowMemoryListener::LowMemoryListener(LowMemoryListenerDelegate* delegate)
    : observer_(new LowMemoryListenerImpl),
      delegate_(delegate),
      weak_factory_(this) {
}

LowMemoryListener::~LowMemoryListener() {
  Stop();
}

void LowMemoryListener::Start() {
  base::Closure memory_low_callback =
      base::Bind(&LowMemoryListener::OnMemoryLow, weak_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&LowMemoryListenerImpl::StartObservingOnFileThread,
                 observer_.get(),
                 memory_low_callback));
}

void LowMemoryListener::Stop() {
  weak_factory_.InvalidateWeakPtrs();
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&LowMemoryListenerImpl::StopObservingOnFileThread,
                 observer_.get()));
}

void LowMemoryListener::OnMemoryLow() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delegate_->OnMemoryLow();
}

}  // namespace chromeos
