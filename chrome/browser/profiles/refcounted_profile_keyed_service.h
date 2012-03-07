// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_REFCOUNTED_PROFILE_KEYED_SERVICE_H_
#define CHROME_BROWSER_PROFILES_REFCOUNTED_PROFILE_KEYED_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile_keyed_base.h"

// Base class for refcounted objects that hang off the Profile.
//
// The two pass shutdown described in ProfileKeyedService works a bit
// differently because there could be outstanding references on other
// threads. ShutdownOnUIThread() will be called on the UI thread, and then the
// destructor will run when the last reference is dropped, which may or may not
// be after the corresponding Profile has been destroyed.
class RefcountedProfileKeyedService
    : public ProfileKeyedBase,
      public base::RefCountedThreadSafe<RefcountedProfileKeyedService> {
 public:
  // Unlike ProfileKeyedService, ShutdownOnUI is not optional. You must do
  // something to drop references during the first pass Shutdown() because this
  // is the only point where you are guaranteed that something is running on
  // the UI thread. The PKSF framework will ensure that this is only called on
  // the UI thread; you do not need to check for that yourself.
  virtual void ShutdownOnUIThread() = 0;

  // This second pass can happen anywhere.
  virtual ~RefcountedProfileKeyedService();
};

#endif  // CHROME_BROWSER_PROFILES_REFCOUNTED_PROFILE_KEYED_SERVICE_H_
