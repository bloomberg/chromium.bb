// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryPurger provides static APIs to purge as much memory as possible from
// all processes.  These can be hooked to various signals to try and balance
// memory consumption, speed, page swapping, etc.

#ifndef CHROME_BROWSER_MEMORY_PURGER_H_
#define CHROME_BROWSER_MEMORY_PURGER_H_
#pragma once

#include "base/basictypes.h"

class RenderProcessHost;

class MemoryPurger {
 public:
  // Call any of these on the UI thread to purge memory from the named places.
  static void PurgeAll();
  static void PurgeBrowser();
  static void PurgeRenderers();
  static void PurgeRendererForHost(RenderProcessHost* host);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryPurger);
};

#endif  // CHROME_BROWSER_MEMORY_PURGER_H_
