// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include <zircon/process.h>
#include <zircon/rights.h>
#include <zircon/syscalls.h>

#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/numerics/checked_math.h"
#include "base/process/process_metrics.h"

namespace base {
namespace subtle {

static constexpr int kNoWriteOrExec =
    ZX_DEFAULT_VMO_RIGHTS &
    ~(ZX_RIGHT_WRITE | ZX_RIGHT_EXECUTE | ZX_RIGHT_SET_PROPERTY);

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Take(
    ScopedZxHandle handle,
    Mode mode,
    size_t size,
    const UnguessableToken& guid) {
  if (!handle.is_valid())
    return {};

  if (size == 0)
    return {};

  if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return {};

  CHECK(
      CheckPlatformHandlePermissionsCorrespondToMode(handle.get(), mode, size));

  return PlatformSharedMemoryRegion(std::move(handle), mode, size, guid);
}

zx_handle_t PlatformSharedMemoryRegion::GetPlatformHandle() const {
  return handle_.get();
}

bool PlatformSharedMemoryRegion::IsValid() const {
  return handle_.is_valid();
}

PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Duplicate() const {
  if (!IsValid())
    return {};

  CHECK_NE(mode_, Mode::kWritable)
      << "Duplicating a writable shared memory region is prohibited";

  ScopedZxHandle duped_handle;
  zx_status_t status = zx_handle_duplicate(handle_.get(), ZX_RIGHT_SAME_RIGHTS,
                                           duped_handle.receive());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
    return {};
  }

  return PlatformSharedMemoryRegion(std::move(duped_handle), mode_, size_,
                                    guid_);
}

bool PlatformSharedMemoryRegion::ConvertToReadOnly() {
  if (!IsValid())
    return false;

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to read-only";

  ScopedZxHandle old_handle(handle_.release());
  ScopedZxHandle new_handle;
  zx_status_t status =
      zx_handle_replace(old_handle.get(), kNoWriteOrExec, new_handle.receive());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_handle_replace";
    return false;
  }
  ignore_result(old_handle.release());

  handle_ = std::move(new_handle);
  mode_ = Mode::kReadOnly;
  return true;
}

bool PlatformSharedMemoryRegion::MapAt(off_t offset,
                                       size_t size,
                                       void** memory,
                                       size_t* mapped_size) const {
  if (!IsValid())
    return false;

  size_t end_byte;
  if (!CheckAdd(offset, size).AssignIfValid(&end_byte) || end_byte > size_) {
    return false;
  }

  bool write_allowed = mode_ != Mode::kReadOnly;
  uintptr_t addr;
  zx_status_t status = zx_vmar_map(
      zx_vmar_root_self(), 0, handle_.get(), offset, size,
      ZX_VM_FLAG_PERM_READ | (write_allowed ? ZX_VM_FLAG_PERM_WRITE : 0),
      &addr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmar_map";
    return false;
  }

  *memory = reinterpret_cast<void*>(addr);
  *mapped_size = size;
  DCHECK_EQ(0U,
            reinterpret_cast<uintptr_t>(*memory) & (kMapMinimumAlignment - 1));
  return true;
}

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Create(Mode mode,
                                                              size_t size) {
  if (size == 0)
    return {};

  size_t rounded_size = bits::Align(size, GetPageSize());
  if (rounded_size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return {};

  CHECK_NE(mode, Mode::kReadOnly) << "Creating a region in read-only mode will "
                                     "lead to this region being non-modifiable";

  ScopedZxHandle vmo;
  zx_status_t status = zx_vmo_create(rounded_size, 0, vmo.receive());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmo_create";
    return {};
  }

  const int kNoExecFlags = ZX_DEFAULT_VMO_RIGHTS & ~ZX_RIGHT_EXECUTE;
  ScopedZxHandle old_vmo(std::move(vmo));
  status = zx_handle_replace(old_vmo.get(), kNoExecFlags, vmo.receive());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_handle_replace";
    return {};
  }
  ignore_result(old_vmo.release());

  return PlatformSharedMemoryRegion(std::move(vmo), mode, size,
                                    UnguessableToken::Create());
}

// static
bool PlatformSharedMemoryRegion::CheckPlatformHandlePermissionsCorrespondToMode(
    PlatformHandle handle,
    Mode mode,
    size_t size) {
  zx_info_handle_basic_t basic = {};
  zx_status_t status = zx_object_get_info(handle, ZX_INFO_HANDLE_BASIC, &basic,
                                          sizeof(basic), nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_object_get_info";
    return false;
  }

  bool is_read_only = (basic.rights & kNoWriteOrExec) == basic.rights;
  bool expected_read_only = mode == Mode::kReadOnly;

  if (is_read_only != expected_read_only) {
    DLOG(ERROR) << "VMO object has wrong access rights: it is"
                << (is_read_only ? " " : " not ") << "read-only but it should"
                << (expected_read_only ? " " : " not ") << "be";
    return false;
  }

  return true;
}

PlatformSharedMemoryRegion::PlatformSharedMemoryRegion(
    ScopedZxHandle handle,
    Mode mode,
    size_t size,
    const UnguessableToken& guid)
    : handle_(std::move(handle)), mode_(mode), size_(size), guid_(guid) {}

}  // namespace subtle
}  // namespace base
