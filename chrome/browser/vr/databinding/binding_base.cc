// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/databinding/binding_base.h"

namespace vr {

BindingBase::BindingBase() : weak_ptr_factory_(this) {}
BindingBase::~BindingBase() {}

base::WeakPtr<BindingBase> BindingBase::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace vr
