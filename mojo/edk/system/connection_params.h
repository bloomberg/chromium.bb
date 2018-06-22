// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CONNECTION_PARAMS_H_
#define MOJO_EDK_SYSTEM_CONNECTION_PARAMS_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "mojo/edk/system/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

// A set of parameters used when establishing a connection to another process.
class MOJO_SYSTEM_IMPL_EXPORT ConnectionParams {
 public:
  explicit ConnectionParams(ScopedInternalPlatformHandle channel);

  ConnectionParams(ConnectionParams&& params);
  ConnectionParams& operator=(ConnectionParams&& params);

  ScopedInternalPlatformHandle TakeChannelHandle();

 private:
  ScopedInternalPlatformHandle channel_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionParams);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CONNECTION_PARAMS_H_
