// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_CONNECTION_PARAMS_H_
#define MOJO_EDK_EMBEDDER_CONNECTION_PARAMS_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

class MOJO_SYSTEM_IMPL_EXPORT ConnectionParams {
 public:
  explicit ConnectionParams(ScopedPlatformHandle channel);

  ConnectionParams(ConnectionParams&& param);
  ConnectionParams& operator=(ConnectionParams&& param);

  ScopedPlatformHandle TakeChannelHandle();

 private:
  ScopedPlatformHandle channel_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionParams);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_CONNECTION_PARAMS_H_
