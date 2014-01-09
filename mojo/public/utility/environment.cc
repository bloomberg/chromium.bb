// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/utility/environment.h"

#include "mojo/public/utility/bindings_support_impl.h"
#include "mojo/public/utility/run_loop.h"

namespace mojo {

Environment::Environment() : bindings_support_(NULL) {
  RunLoop::SetUp();

  internal::BindingsSupportImpl::SetUp();
  bindings_support_ = new internal::BindingsSupportImpl;
  BindingsSupport::Set(bindings_support_);
}

Environment::~Environment() {
  // Allow for someone to have replaced BindingsSupport.
  if (BindingsSupport::Get() == bindings_support_)
    BindingsSupport::Set(NULL);
  delete bindings_support_;
  internal::BindingsSupportImpl::TearDown();

  RunLoop::TearDown();
}

}  // namespace mojo
