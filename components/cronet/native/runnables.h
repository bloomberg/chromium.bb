// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_RUNNABLES_H_
#define COMPONENTS_CRONET_NATIVE_RUNNABLES_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/cronet/native/generated/cronet.idl_impl_interface.h"

namespace cronet {

// Base implementation of CronetRunnable that keeps Cronet_RunnableContext but
// doesn't implement Run() method.
class BaseCronet_Runnable : public Cronet_Runnable {
 public:
  BaseCronet_Runnable() = default;
  ~BaseCronet_Runnable() override = default;

  void SetContext(Cronet_RunnableContext context) override;

  Cronet_RunnableContext GetContext() override;

 private:
  // Runnable context.
  Cronet_RunnableContext context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BaseCronet_Runnable);
};

// Implementation of CronetRunnable that runs arbitrary base::OnceClosure.
class OnceClosureRunnable : public BaseCronet_Runnable {
 public:
  explicit OnceClosureRunnable(base::OnceClosure task);
  ~OnceClosureRunnable() override;

  void Run() override;

 private:
  // Closure to run.
  base::OnceClosure task_;

  DISALLOW_COPY_AND_ASSIGN(OnceClosureRunnable);
};

};  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_RUNNABLES_H_
