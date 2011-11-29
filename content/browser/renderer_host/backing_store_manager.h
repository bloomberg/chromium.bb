// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_MANAGER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/process.h"
#include "content/common/content_export.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/surface/transport_dib.h"

class BackingStore;
class RenderWidgetHost;

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
  static BackingStore* GetBackingStore(RenderWidgetHost* host,
                                       const gfx::Size& desired_size);

  // Makes a backing store which is fully ready for consumption, i.e. the
  // bitmap from the renderer has been copied into the backing store.
  //
  // backing_store_size
  //   The desired backing store dimensions.
  // bitmap_section
  //   The bitmap section from the renderer.
  // bitmap_rect
  //   The rect to be painted into the backing store
  // needs_full_paint
  //   Set if we need to send out a request to paint the view
  //   to the renderer.
  static void PrepareBackingStore(
      RenderWidgetHost* host,
      const gfx::Size& backing_store_size,
      TransportDIB::Id bitmap,
      const gfx::Rect& bitmap_rect,
      const std::vector<gfx::Rect>& copy_rects,
      const base::Closure& completion_callback,
      bool* needs_full_paint,
      bool* scheduled_completion_callback);

  // Returns a matching backing store for the host.
  // Returns NULL if we fail to find one.
  static BackingStore* Lookup(RenderWidgetHost* host);

  // Removes the backing store for the host.
  static void RemoveBackingStore(RenderWidgetHost* host);

  // Removes all backing stores.
  CONTENT_EXPORT static void RemoveAllBackingStores();

  // Expires the given backing store. This emulates something getting evicted
  // from the cache for the purpose of testing. Returns true if the host was
  // removed, false if it wasn't found.
  CONTENT_EXPORT static bool ExpireBackingStoreForTest(RenderWidgetHost* host);

  // Current size in bytes of the backing store cache.
  CONTENT_EXPORT static size_t MemorySize();

 private:
  // Not intended for instantiation.
  BackingStoreManager() {}

  DISALLOW_COPY_AND_ASSIGN(BackingStoreManager);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACKING_STORE_MANAGER_H_
