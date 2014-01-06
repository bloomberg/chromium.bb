// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NONSFI_ELF_LOADER_H_
#define COMPONENTS_NACL_LOADER_NONSFI_ELF_LOADER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

#if !defined(OS_LINUX)
#error Non SFI mode is currently supported only on linux.
#endif

struct NaClDesc;

namespace nacl {
namespace nonsfi {

class ElfImage {
 public:
  ElfImage();
  ~ElfImage();

  // Available only after Load() is called.
  uintptr_t entry_point() const;

  // Reads the ELF file from the descriptor and stores the header information
  // into this instance.
  NaClErrorCode Read(struct NaClDesc* descriptor);

  // Loads the ELF executable to the memory.
  NaClErrorCode Load(struct NaClDesc* descriptor);

 private:
  struct Data;
  ::scoped_ptr<Data> data_;

  DISALLOW_COPY_AND_ASSIGN(ElfImage);
};

}  // namespace nonsfi
}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_NONSFI_ELF_LOADER_H_
