// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryPurger is designed to be used a singleton which listens for
// suspend/resume notifications and purges as much memory as possible before
// suspend.  The hope is that it will be faster to recalculate or manually
// reload this data on resume than to let the OS page everything out and then
// fault it back in.

#ifndef CHROME_BROWSER_MEMORY_PURGER_H_
#define CHROME_BROWSER_MEMORY_PURGER_H_

#include "base/system_monitor.h"

template<typename Type>
struct DefaultSingletonTraits;

class MemoryPurger : public base::SystemMonitor::PowerObserver {
 public:
  static MemoryPurger* GetSingleton();

  // PowerObserver
  virtual void OnSuspend();
  virtual void OnResume();

 private:
  MemoryPurger();
  virtual ~MemoryPurger();

  friend struct DefaultSingletonTraits<MemoryPurger>;

  DISALLOW_COPY_AND_ASSIGN(MemoryPurger);
};

#endif  // CHROME_BROWSER_MEMORY_PURGER_H_
