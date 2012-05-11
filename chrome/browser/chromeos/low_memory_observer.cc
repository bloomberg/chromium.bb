// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/low_memory_observer.h"

#include <fcntl.h>

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/oom_priority_manager.h"
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
// LowMemoryObserverImpl
//
// Does the actual work of observing.  The observation work happens on the FILE
// thread, and the discarding of tabs happens on the UI thread.
// If low memory is detected, then we discard a tab, wait
// kLowMemoryCheckTimeoutMs milliseconds and then start watching again to see
// if we're still in a low memory state.  This is to keep from discarding all
// tabs the first time we enter the state, because it takes time for the
// tabs to deallocate their memory.  A timer isn't the perfect solution, but
// without any reliable indicator that a tab has had all its parts deallocated,
// it's the next best thing.
class LowMemoryObserverImpl
    : public base::RefCountedThreadSafe<LowMemoryObserverImpl> {
 public:
  LowMemoryObserverImpl() : watcher_delegate_(this), file_descriptor_(-1) {}
  ~LowMemoryObserverImpl() {
    StopObservingOnFileThread();
  }

  // Start watching the low memory file for readability.
  // Calls to StartObserving should always be matched with calls to
  // StopObserving.  This method should only be called from the FILE thread.
  void StartObservingOnFileThread();

  // Stop watching the low memory file for readability.
  // May be safely called if StartObserving has not been called.
  // This method should only be called from the FILE thread.
  void StopObservingOnFileThread();

 private:
  // Start a timer to resume watching the low memory file descriptor.
  void ScheduleNextObservation();

  // Actually start watching the file descriptor.
  void StartWatchingDescriptor();

  // Delegate to receive events from WatchFileDescriptor.
  class FileWatcherDelegate : public MessageLoopForIO::Watcher {
   public:
    explicit FileWatcherDelegate(LowMemoryObserverImpl* owner)
        : owner_(owner) {}
    virtual ~FileWatcherDelegate() {}

    // Overrides for MessageLoopForIO::Watcher
    virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {}
    virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
      LOG(WARNING) << "Low memory condition detected.  Discarding a tab.";
      // We can only discard tabs on the UI thread.
      base::Callback<void(void)> callback = base::Bind(&DiscardTab);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
      owner_->ScheduleNextObservation();
    }

    // Sends off a discard request to the OomPriorityManager.  Must be run on
    // the UI thread.
    static void DiscardTab() {
      CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      if (g_browser_process && g_browser_process->oom_priority_manager())
        g_browser_process->oom_priority_manager()->LogMemoryAndDiscardTab();
    }

   private:
    LowMemoryObserverImpl* owner_;
    DISALLOW_COPY_AND_ASSIGN(FileWatcherDelegate);
  };

  scoped_ptr<MessageLoopForIO::FileDescriptorWatcher> watcher_;
  FileWatcherDelegate watcher_delegate_;
  int file_descriptor_;
  base::OneShotTimer<LowMemoryObserverImpl> timer_;

  DISALLOW_COPY_AND_ASSIGN(LowMemoryObserverImpl);
};

void LowMemoryObserverImpl::StartObservingOnFileThread() {
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

void LowMemoryObserverImpl::StopObservingOnFileThread() {
  // If StartObserving failed, StopObserving will still get called.
  timer_.Stop();
  if (file_descriptor_ >= 0) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    watcher_.reset(NULL);
    ::close(file_descriptor_);
    file_descriptor_ = -1;
  }
}

void LowMemoryObserverImpl::ScheduleNextObservation() {
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kLowMemoryCheckTimeoutMs),
               this,
               &LowMemoryObserverImpl::StartWatchingDescriptor);
}

void LowMemoryObserverImpl::StartWatchingDescriptor() {
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
// LowMemoryObserver

LowMemoryObserver::LowMemoryObserver() : observer_(new LowMemoryObserverImpl) {}

LowMemoryObserver::~LowMemoryObserver() { Stop(); }

void LowMemoryObserver::Start() {
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&LowMemoryObserverImpl::StartObservingOnFileThread,
                 observer_.get()));
}

void LowMemoryObserver::Stop() {
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&LowMemoryObserverImpl::StopObservingOnFileThread,
                 observer_.get()));
}

// static
void LowMemoryObserver::SetLowMemoryMargin(int64 margin_mb) {
  content::ZygoteHost::GetInstance()->AdjustLowMemoryMargin(margin_mb);
}

}  // namespace chromeos
