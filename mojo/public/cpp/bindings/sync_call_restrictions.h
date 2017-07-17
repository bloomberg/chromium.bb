// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SYNC_CALL_RESTRICTIONS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SYNC_CALL_RESTRICTIONS_H_

#include "base/macros.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/public/cpp/bindings/bindings_export.h"

#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))
#define ENABLE_SYNC_CALL_RESTRICTIONS 1
#else
#define ENABLE_SYNC_CALL_RESTRICTIONS 0
#endif

class ChromeSelectFileDialogFactory;

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace content {
class BrowserTestBase;
}

namespace display {
class ForwardingDisplayDelegate;
}

namespace leveldb {
class LevelDBMojoProxy;
}

namespace prefs {
class PersistentPrefStoreClient;
}

namespace ui {
class Gpu;
}

namespace views {
class ClipboardMus;
}

namespace mojo {

// In some processes, sync calls are disallowed. For example, in the browser
// process we don't want any sync calls to child processes for performance,
// security and stability reasons. SyncCallRestrictions helps to enforce such
// rules.
//
// Before processing a sync call, the bindings call
// SyncCallRestrictions::AssertSyncCallAllowed() to check whether sync calls are
// allowed. By default, it is determined by the mojo system property
// MOJO_PROPERTY_TYPE_SYNC_CALL_ALLOWED. If the default setting says no but you
// have a very compelling reason to disregard that (which should be very very
// rare), you can override it by constructing a ScopedAllowSyncCall object,
// which allows making sync calls on the current sequence during its lifetime.
class MOJO_CPP_BINDINGS_EXPORT SyncCallRestrictions {
 public:
#if ENABLE_SYNC_CALL_RESTRICTIONS
  // Checks whether the current sequence is allowed to make sync calls, and
  // causes a DCHECK if not.
  static void AssertSyncCallAllowed();
#else
  // Inline the empty definitions of functions so that they can be compiled out.
  static void AssertSyncCallAllowed() {}
#endif

 private:
  // DO NOT ADD ANY OTHER FRIEND STATEMENTS, talk to mojo/OWNERS first.
  // BEGIN ALLOWED USAGE.
  friend class content::BrowserTestBase;  // Test-only.
  friend class ui::Gpu;  // http://crbug.com/620058
  // LevelDBMojoProxy makes same-process sync calls from the DB thread.
  friend class leveldb::LevelDBMojoProxy;
  // Pref service connection is sync at startup.
  friend class prefs::PersistentPrefStoreClient;
  // Incognito pref service instances are created synchronously.
  friend class sync_preferences::PrefServiceSyncable;
  // For file open and save dialogs created synchronously.
  friend class ::ChromeSelectFileDialogFactory;
  // END ALLOWED USAGE.

  // BEGIN USAGE THAT NEEDS TO BE FIXED.
  // In the non-mus case, we called blocking OS functions in the ui::Clipboard
  // implementation which weren't caught by sync call restrictions. Our blocking
  // calls to mus, however, are.
  friend class views::ClipboardMus;
  // In ash::Shell::Init() it assumes that NativeDisplayDelegate will be
  // synchronous at first. In mushrome ForwardingDisplayDelegate uses a
  // synchronous call to get the display snapshots as a workaround.
  friend class display::ForwardingDisplayDelegate;
  // END USAGE THAT NEEDS TO BE FIXED.

#if ENABLE_SYNC_CALL_RESTRICTIONS
  static void IncreaseScopedAllowCount();
  static void DecreaseScopedAllowCount();
#else
  static void IncreaseScopedAllowCount() {}
  static void DecreaseScopedAllowCount() {}
#endif

  // If a process is configured to disallow sync calls in general, constructing
  // a ScopedAllowSyncCall object temporarily allows making sync calls on the
  // current sequence. Doing this is almost always incorrect, which is why we
  // limit who can use this through friend. If you find yourself needing to use
  // this, talk to mojo/OWNERS.
  class ScopedAllowSyncCall {
   public:
    ScopedAllowSyncCall() { IncreaseScopedAllowCount(); }
    ~ScopedAllowSyncCall() { DecreaseScopedAllowCount(); }

   private:
#if ENABLE_SYNC_CALL_RESTRICTIONS
    base::ThreadRestrictions::ScopedAllowWait allow_wait_;
#endif

    DISALLOW_COPY_AND_ASSIGN(ScopedAllowSyncCall);
  };

  DISALLOW_IMPLICIT_CONSTRUCTORS(SyncCallRestrictions);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SYNC_CALL_RESTRICTIONS_H_
