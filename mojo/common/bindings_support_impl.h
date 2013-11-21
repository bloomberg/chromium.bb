// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_BINDINGS_SUPPORT_IMPL_H_
#define MOJO_COMMON_BINDINGS_SUPPORT_IMPL_H_

#include "mojo/public/bindings/lib/bindings_support.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "mojo/common/mojo_common_export.h"

namespace mojo {
namespace common {

class MOJO_COMMON_EXPORT BindingsSupportImpl
    : NON_EXPORTED_BASE(public BindingsSupport) {
 public:
  BindingsSupportImpl();
  virtual ~BindingsSupportImpl();

  // BindingsSupport methods:
  virtual AsyncWaitID AsyncWait(const Handle& handle, MojoWaitFlags flags,
                                AsyncWaitCallback* callback) OVERRIDE;
  virtual void CancelWait(AsyncWaitID async_wait_id) OVERRIDE;

 private:
  class Context;
  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(BindingsSupportImpl);
};

}  // namespace common
}  // namespace mojo

#endif  // MOJO_COMMON_BINDINGS_SUPPORT_IMPL_H_
