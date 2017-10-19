// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_DATABINDING_BINDING_BASE_H_
#define CHROME_BROWSER_VR_DATABINDING_BINDING_BASE_H_

#include "base/memory/weak_ptr.h"

namespace vr {

// Bindings are used to tie models to views. You may, for example, want to bind
// the visibility of an error indicator to a boolean that signals that the
// application is exhibiting the error condition.
class BindingBase {
 public:
  BindingBase();
  virtual ~BindingBase();

  // This function updates the binding. The exact behavior depends on the
  // subclass. Please see comments on the overridden functions for details.
  // Returns true if the binding was updated.
  virtual bool Update() = 0;

  base::WeakPtr<BindingBase> GetWeakPtr();

 private:
  base::WeakPtrFactory<BindingBase> weak_ptr_factory_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_DATABINDING_BINDING_BASE_H_
