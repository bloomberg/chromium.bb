// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_MANAGER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/process.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/surface/transport_dib.h"

class BackingStore;

namespace content {
class RenderWidgetHost;
}

// This class manages backing stores in the browsr. Every RenderWidgetHost is
// associated with a backing store which it requests from this class.  The
// hosts don't maintain any references to the backing stores.  These backing
// stores are maintained in a cache which can be trimmed as needed.
class BackingStoreManager {
 public:
  // Returns a backing store which matches the desired dimensions.
  //
  // backing_store_rect
  //   The desired backing store dimensions.
  // Returns a pointer to the backing store on success, NULL on failure.
  static BackingStore* GetBackingStore(content::RenderWidgetHost* host,
                                       const gfx::Size& desired_size);

  // Makes a backing store which is fully ready for consumption, i.e. the
  // bitmap from the renderer has been copied into the backing store.
  //
  // backing_store_size
  //   The desired backing store dimensions, in DIPs.
  // bitmap_section
  //   The bitmap section from the renderer.
  // bitmap_rect
  //   The rect to be painted into the backing store, in DIPs.
  // scale_factor
  //   The device scale factor the backing store is expected to be at.
  //   If the backing store's device scale factor doesn't match, it will need
  //   to scale |bitmap| at paint time. This will only be out of sync with the
  //   backing store scale factor for a few frames, right after device scale
  //   changes.
  // needs_full_paint
  //   Set if we need to send out a request to paint the view
  //   to the renderer.
  static void PrepareBackingStore(
      content::RenderWidgetHost* host,
      const gfx::Size& backing_store_size,
      TransportDIB::Id bitmap,
      const gfx::Rect& bitmap_rect,
      const std::vector<gfx::Rect>& copy_rects,
      float scale_factor,
      const base::Closure& completion_callback,
      bool* needs_full_paint,
      bool* scheduled_completion_callback);

  // Returns a matching backing store for the host.
  // Returns NULL if we fail to find one.
  static BackingStore* Lookup(content::RenderWidgetHost* host);

  // Removes the backing store for the host.
  static void RemoveBackingStore(content::RenderWidgetHost* host);

  // Removes all backing stores.
  static void RemoveAllBackingStores();

  // Current size in bytes of the backing store cache.
  static size_t MemorySize();

 private:
  // Not intended for instantiation.
  BackingStoreManager() {}

  DISALLOW_COPY_AND_ASSIGN(BackingStoreManager);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_MANAGER_H_
