// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DISPLAY_H_
#define COMPONENTS_EXO_DISPLAY_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory_handle.h"

namespace exo {
class SharedMemory;
class ShellSurface;
class Surface;

// The core display class. This class provides functions for creating surfaces
// and is in charge of combining the contents of multiple surfaces into one
// displayable output.
class Display {
 public:
  Display();
  ~Display();

  // Creates a new surface.
  scoped_ptr<Surface> CreateSurface();

  // Creates a shared memory segment from |handle| of |size| with the
  // given |id|. This function takes ownership of |handle|.
  scoped_ptr<SharedMemory> CreateSharedMemory(
      const base::SharedMemoryHandle& handle,
      size_t size);

  // Creates a shell surface for an existing surface.
  scoped_ptr<ShellSurface> CreateShellSurface(Surface* surface);

 private:
  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DISPLAY_H_
