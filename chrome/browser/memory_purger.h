// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryPurger provides static APIs to purge as much memory as possible from
// all processes.  These can be hooked to various signals to try and balance
// memory consumption, speed, page swapping, etc.
//
// This was implemented in an attempt to speed up suspend-to-disk by throwing
// away any cached data that can be recomputed.  Memory use will rapidly
// re-expand after this purge is run.

#ifndef CHROME_BROWSER_MEMORY_PURGER_H_
#define CHROME_BROWSER_MEMORY_PURGER_H_
#pragma once

#include "base/basictypes.h"

namespace content {
class RenderProcessHost;
}

class MemoryPurger {
 public:
  // Call any of these on the UI thread to purge memory from the named places.
  static void PurgeAll();
  static void PurgeBrowser();
  static void PurgeRenderers();
  static void PurgeRendererForHost(content::RenderProcessHost* host);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryPurger);
};

#endif  // CHROME_BROWSER_MEMORY_PURGER_H_
