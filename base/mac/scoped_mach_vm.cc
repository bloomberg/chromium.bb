// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_mach_vm.h"

#if defined(OS_IOS)
#define mach_vm_address_t vm_address_t
#define mach_vm_size_t vm_size_t
#define mach_vm_deallocate vm_deallocate
#endif  // OS_IOS

namespace base {
namespace mac {

void ScopedMachVM::reset(mach_vm_address_t address, mach_vm_size_t size) {
  DCHECK(address % PAGE_SIZE == 0);
  DCHECK(size % PAGE_SIZE == 0);

  if (size_) {
    if (address_ < address) {
      mach_vm_deallocate(mach_task_self(),
                         address_,
                         std::min(size_, address - address_));
    }
    if (address_ + size_ > address + size) {
      mach_vm_address_t deallocate_start = std::max(address_, address + size);
      mach_vm_deallocate(mach_task_self(),
                         deallocate_start,
                         address_ + size_ - deallocate_start);
    }
  }

  address_ = address;
  size_ = size;
}

}  // namespace mac
}  // namespace base
