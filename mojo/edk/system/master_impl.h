// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_MASTER_IMPL_H_
#define MOJO_EDK_SYSTEM_MASTER_IMPL_H_

#include "base/process/process.h"
#include "mojo/edk/system/master.mojom.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace mojo {
namespace edk {

// An instance of this class exists in the maste process for each slave process.
class MasterImpl : public Master {
 public:
  explicit MasterImpl(base::ProcessId slave_pid);
  ~MasterImpl() override;

  // Master implementation:
  void HandleToToken(ScopedHandle platform_handle,
                     const HandleToTokenCallback& callback) override;
  void TokenToHandle(uint64_t token,
                     const TokenToHandleCallback& callback) override;

 private:
#if defined(OS_WIN)
  base::Process slave_process_;
#endif

  MOJO_DISALLOW_COPY_AND_ASSIGN(MasterImpl);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_MASTER_IMPL_H_
