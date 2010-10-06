// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <limits>

#include "app/surface/transport_dib.h"
#include "base/logging.h"
#include "base/scoped_handle_win.h"
#include "base/scoped_ptr.h"
#include "base/sys_info.h"
#include "skia/ext/platform_canvas.h"

TransportDIB::Handle TransportDIB::Handle::DupForProcess(
    base::ProcessHandle new_owner_process) const {
  base::ProcessId new_owner_id = ::GetProcessId(new_owner_process);
  if (!new_owner_id) {
    LOG(WARNING) << "Could not get process id from handle"
                 << " handle:" << new_owner_process
                 << " error:" << ::GetLastError();
    return Handle();
  }
  HANDLE owner_process = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, owner_id_);
  if (!owner_process) {
    LOG(WARNING) << "Could not open process"
                 << " id:" << owner_id_
                 << " error:" << ::GetLastError();
    return Handle();
  }
  ScopedHandle scoped_owner_process(owner_process);
  HANDLE new_section = NULL;
  ::DuplicateHandle(owner_process, section_,
                    new_owner_process, &new_section,
                    STANDARD_RIGHTS_REQUIRED | FILE_MAP_ALL_ACCESS,
                    FALSE, 0);
  if (!new_section) {
    LOG(WARNING) << "Could not duplicate handle for process"
                 << " error:" << ::GetLastError();
    return Handle();
  }
  return TransportDIB::Handle(new_section, new_owner_id, false);
}

TransportDIB::TransportDIB() {
}

TransportDIB::~TransportDIB() {
}

TransportDIB::TransportDIB(HANDLE handle)
    : shared_memory_(handle, false /* read write */) {
}

// static
TransportDIB* TransportDIB::Create(size_t size, uint32 sequence_num) {
  size_t allocation_granularity = base::SysInfo::VMAllocationGranularity();
  size = size / allocation_granularity + 1;
  size = size * allocation_granularity;

  TransportDIB* dib = new TransportDIB;

  if (!dib->shared_memory_.Create("", false /* read write */,
                                  true /* open existing */, size)) {
    delete dib;
    return NULL;
  }

  dib->size_ = size;
  dib->sequence_num_ = sequence_num;

  return dib;
}

// static
TransportDIB* TransportDIB::Map(Handle handle) {
  scoped_ptr<TransportDIB> dib(CreateWithHandle(handle));
  if (!dib->Map())
    return NULL;
  return dib.release();
}

// static
TransportDIB* TransportDIB::CreateWithHandle(Handle handle) {
  // It is not sufficient to compare the current process ID and the ID in the
  // handle here to see if a duplication is required because they will always
  // be the same in single process mode.
  if (handle.should_dup_on_map())
    handle = handle.DupForProcess(::GetCurrentProcess());
  return new TransportDIB(handle.section());
}

// static
bool TransportDIB::is_valid(Handle dib) {
  return dib.section() != NULL && dib.owner_id() != NULL;
}

skia::PlatformCanvas* TransportDIB::GetPlatformCanvas(int w, int h) {
  // This DIB already mapped the file into this process, but PlatformCanvas
  // will map it again.
  DCHECK(!memory()) << "Mapped file twice in the same process.";

  scoped_ptr<skia::PlatformCanvas> canvas(new skia::PlatformCanvas);
  if (!canvas->initialize(w, h, true, shared_memory_.handle()))
    return NULL;
  return canvas.release();
}

bool TransportDIB::Map() {
  if (memory())
    return true;

  if (!shared_memory_.Map(0 /* map whole shared memory segment */)) {
    LOG(ERROR) << "Failed to map transport DIB"
               << " handle:" << shared_memory_.handle()
               << " error:" << ::GetLastError();
    return false;
  }

  // There doesn't seem to be any way to find the size of the shared memory
  // region! GetFileSize indicates that the handle is invalid. Thus, we
  // conservatively set the size to the maximum and hope that the renderer
  // isn't about to ask us to read off the end of the array.
  size_ = std::numeric_limits<size_t>::max();
  return true;
}

TransportDIB::Handle TransportDIB::GetHandleForProcess(
    base::ProcessHandle process_handle) const {
  return handle().DupForProcess(process_handle);
}

void* TransportDIB::memory() const {
  return shared_memory_.memory();
}

TransportDIB::Handle TransportDIB::handle() const {
  return TransferrableSectionHandle(shared_memory_.handle(),
                                    ::GetCurrentProcessId(),
                                    true);
}

TransportDIB::Id TransportDIB::id() const {
  return sequence_num_;
}
