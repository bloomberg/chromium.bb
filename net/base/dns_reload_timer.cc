// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dns_reload_timer.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
#include "base/lazy_instance.h"
#include "base/threading/thread_local_storage.h"
#include "base/time.h"

namespace {

// On Linux/BSD, changes to /etc/resolv.conf can go unnoticed thus resulting
// in DNS queries failing either because nameservers are unknown on startup
// or because nameserver info has changed as a result of e.g. connecting to
// a new network. Some distributions patch glibc to stat /etc/resolv.conf
// to try to automatically detect such changes but these patches are not
// universal and even patched systems such as Jaunty appear to need calls
// to res_ninit to reload the nameserver information in different threads.
//
// We adopt the Mozilla solution here which is to call res_ninit when
// lookups fail and to rate limit the reloading to once per second per
// thread.
//
// OpenBSD does not have thread-safe res_ninit/res_nclose so we can't do
// the same trick there.

// Keep a timer per calling thread to rate limit the calling of res_ninit.
class DnsReloadTimer {
 public:
  // Check if the timer for the calling thread has expired. When no
  // timer exists for the calling thread, create one.
  bool Expired() {
    const base::TimeDelta kRetryTime = base::TimeDelta::FromSeconds(1);
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeTicks* timer_ptr =
      static_cast<base::TimeTicks*>(tls_index_.Get());

    if (!timer_ptr) {
      timer_ptr = new base::TimeTicks();
      *timer_ptr = base::TimeTicks::Now();
      tls_index_.Set(timer_ptr);
      // Return true to reload dns info on the first call for each thread.
      return true;
    } else if (now - *timer_ptr > kRetryTime) {
      *timer_ptr = now;
      return true;
    } else {
      return false;
    }
  }

  // Free the allocated timer.
  static void SlotReturnFunction(void* data) {
    base::TimeTicks* tls_data = static_cast<base::TimeTicks*>(data);
    delete tls_data;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<DnsReloadTimer>;

  DnsReloadTimer() {
    // During testing the DnsReloadTimer Singleton may be created and destroyed
    // multiple times. Initialize the ThreadLocalStorage slot only once.
    if (!tls_index_.initialized())
      tls_index_.Initialize(SlotReturnFunction);
  }

  ~DnsReloadTimer() {
  }

  // We use thread local storage to identify which base::TimeTicks to
  // interact with.
  static base::ThreadLocalStorage::Slot tls_index_ ;

  DISALLOW_COPY_AND_ASSIGN(DnsReloadTimer);
};

// A TLS slot to the TimeTicks for the current thread.
// static
base::ThreadLocalStorage::Slot DnsReloadTimer::tls_index_(
    base::LINKER_INITIALIZED);

base::LazyInstance<DnsReloadTimer,
                   base::LeakyLazyInstanceTraits<DnsReloadTimer> >
    g_dns_reload_timer(base::LINKER_INITIALIZED);

}  // namespace

namespace net {

bool DnsReloadTimerHasExpired() {
  DnsReloadTimer* dns_timer = g_dns_reload_timer.Pointer();
  return dns_timer->Expired();
}

}  // namespace net

#endif  // defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
