// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SYSTEM_GLES2_PRIVATE_H_
#define MOJO_PUBLIC_SYSTEM_GLES2_PRIVATE_H_

#include "mojo/public/gles2/gles2.h"

namespace mojo {

// Implementors of the gles2 APIs can use this interface to install their
// implementation into the mojo_gles2 dynamic library. Mojo clients should not
// call these functions directly.
class MOJO_GLES2_EXPORT GLES2Private {
 public:
  virtual ~GLES2Private();

  static void Init(GLES2Private* priv);

  virtual void Initialize() = 0;
  virtual void Terminate() = 0;
  virtual void MakeCurrent(uint64_t encoded) = 0;
  virtual void SwapBuffers() = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_SYSTEM_GLES2_PRIVATE_H_
