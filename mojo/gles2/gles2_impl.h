// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GLES2_GLES2_IMPL_H_
#define MOJO_GLES2_GLES2_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "mojo/gles2/export.h"
#include "mojo/public/gles2/gles2_private.h"

namespace mojo {
namespace gles2 {

class MOJO_GLES2_IMPL_EXPORT GLES2Impl : public GLES2Private {
 public:
  GLES2Impl();
  virtual ~GLES2Impl();

  static void Init();

  virtual void Initialize() OVERRIDE;
  virtual void Terminate() OVERRIDE;
  virtual void MakeCurrent(uint64_t encoded) OVERRIDE;
  virtual void SwapBuffers() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Impl);
};

}  // namespace gles2
}  // namespace mojo

#endif  // MOJO_GLES2_GLES2_IMPL_H_
