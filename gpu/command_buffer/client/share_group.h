// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_SHARE_GROUP_H_
#define GPU_COMMAND_BUFFER_CLIENT_SHARE_GROUP_H_

#include "../client/ref_counted.h"
#include "gles2_impl_export.h"

namespace gpu {
namespace gles2 {

// ShareGroup manages shared resources for contexts that are sharing resources.
class GLES2_IMPL_EXPORT ShareGroup
    : public gpu::RefCountedThreadSafe<ShareGroup> {
 public:
  typedef scoped_refptr<ShareGroup> Ref;

  ShareGroup();
  ~ShareGroup();

  bool Initialize();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShareGroup);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_SHARE_GROUP_H_
