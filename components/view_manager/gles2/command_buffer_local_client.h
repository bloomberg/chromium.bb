// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_LOCAL_CLIENT_H_
#define COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_LOCAL_CLIENT_H_

namespace gles2 {

class CommandBufferLocalClient {
 public:
  virtual void UpdateVSyncParameters(int64_t timebase, int64_t interval) = 0;
  virtual void DidLoseContext() = 0;

 protected:
  virtual ~CommandBufferLocalClient() {}
};

}  // namespace gles2

#endif  // COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_LOCAL_CLIENT_H_
