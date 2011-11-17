// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_PROCESS_IMPL_H_
#define CONTENT_RENDERER_RENDER_PROCESS_IMPL_H_
#pragma once

#include "base/timer.h"
#include "content/renderer/render_process.h"
#include "native_client/src/shared/imc/nacl_imc.h"

namespace skia {
class PlatformCanvas;
}

// Implementation of the RenderProcess interface for the regular browser.
// See also MockRenderProcess which implements the active "RenderProcess" when
// running under certain unit tests.
class RenderProcessImpl : public RenderProcess {
 public:
  RenderProcessImpl();
  virtual ~RenderProcessImpl();

  // RenderProcess implementation.
  virtual skia::PlatformCanvas* GetDrawingCanvas(
      TransportDIB** memory,
      const gfx::Rect& rect) OVERRIDE;
  virtual void ReleaseTransportDIB(TransportDIB* memory) OVERRIDE;
  virtual bool UseInProcessPlugins() const OVERRIDE;

  // Like UseInProcessPlugins(), but called before RenderProcess is created
  // and does not allow overriding by tests. This just checks the command line
  // each time.
  static bool InProcessPlugins();

 private:
  // Look in the shared memory cache for a suitable object to reuse.
  //   result: (output) the memory found
  //   size: the resulting memory will be >= this size, in bytes
  //   returns: false if a suitable DIB memory could not be found
  bool GetTransportDIBFromCache(TransportDIB** result, size_t size);

  // Maybe put the given shared memory into the shared memory cache. Returns
  // true if the SharedMemory object was stored in the cache; otherwise, false
  // is returned.
  bool PutSharedMemInCache(TransportDIB* memory);

  void ClearTransportDIBCache();

  // Return the index of a free cache slot in which to install a transport DIB
  // of the given size. If all entries in the cache are larger than the given
  // size, this doesn't free any slots and returns -1.
  int FindFreeCacheSlot(size_t size);

  // Create a new transport DIB of, at least, the given size. Return NULL on
  // error.
  TransportDIB* CreateTransportDIB(size_t size);
  void FreeTransportDIB(TransportDIB*);

  // A very simplistic and small cache.  If an entry in this array is non-null,
  // then it points to a SharedMemory object that is available for reuse.
  TransportDIB* shared_mem_cache_[2];

  // This DelayTimer cleans up our cache 5 seconds after the last use.
  base::DelayTimer<RenderProcessImpl> shared_mem_cache_cleaner_;

  // TransportDIB sequence number
  uint32 transport_dib_next_sequence_number_;

  bool in_process_plugins_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessImpl);
};

#endif  // CONTENT_RENDERER_RENDER_PROCESS_IMPL_H_
