// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/display.h"

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// Display, public:

Display::Display() {}

Display::~Display() {}

scoped_ptr<Surface> Display::CreateSurface() {
  TRACE_EVENT0("exo", "Display::CreateSurface");

  return make_scoped_ptr(new Surface);
}

scoped_ptr<SharedMemory> Display::CreateSharedMemory(
    const base::SharedMemoryHandle& handle,
    size_t size) {
  TRACE_EVENT1("exo", "Display::CreateSharedMemory", "size", size);

  if (!base::SharedMemory::IsHandleValid(handle))
    return nullptr;

  return make_scoped_ptr(new SharedMemory(handle));
}

scoped_ptr<ShellSurface> Display::CreateShellSurface(Surface* surface) {
  TRACE_EVENT1("exo", "Display::CreateShellSurface", "surface",
               surface->AsTracedValue());

  return make_scoped_ptr(new ShellSurface(surface));
}

}  // namespace exo
