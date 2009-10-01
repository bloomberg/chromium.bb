// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_purger.h"

#include "base/singleton.h"

// static
MemoryPurger* MemoryPurger::GetSingleton() {
  return Singleton<MemoryPurger>::get();
}

void MemoryPurger::OnSuspend() {
  // TODO:
  //
  // * For the browser process:
  //   * Commit all the sqlite databases and unload them from memory; at the
  // very least, do this with the history system, then use the prefetch trick to
  // reload it on wake.
  //   * Repeatedly call the V8 idle notification until it returns true
  // ("nothing more to free").  Note that it makes more sense to do this than to
  // implement a new "delete everything" pass because object references make it
  // difficult to free everything possible in just one pass.
  //   * Dump the backing stores.
  //   * Destroy the spellcheck object.
  //   * Tell tcmalloc to free everything it can.  (This is vague since it's not
  // clear to me what this means; Jim, Mike, and Anton will know more here.
  // Also, do we need to do this in each renderer process too?)
  //
  // * For each renderer process:
  //   * Repeatedly call the V8 idle notification until it returns true.
  //   * Tell the WebCore cache manager to set its global limit to 0, which
  // should flush the image/script/css/etc. caches.
  //   * Destroy the WebCore glyph caches.
  //   * Tell tcmalloc to free everything it can.
  //
  // Concern: If we tell a bunch of renderer processes to destroy their data,
  // they may have to page everything in to do it, which could end up
  // overflowing the amount of time we have to work with.
}

void MemoryPurger::OnResume() {
  // TODO:
  //
  // * For the browser process:
  //   * Reload the history system and any other needed sqlite databases.
  //
  // * For each renderer process:
  //   * Reset the WebCore cache manager global limit to the default.
}

MemoryPurger::MemoryPurger() {
  base::SystemMonitor::Get()->AddObserver(this);
}

MemoryPurger::~MemoryPurger() {
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->RemoveObserver(this);
}
