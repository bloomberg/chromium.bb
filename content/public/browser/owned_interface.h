// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_OWNED_INTERFACE_H_
#define CONTENT_BROWSER_OWNED_INTERFACE_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"

namespace content {

// Common base class for interface impls, allowing them to be stored in a
// type-erased container for ownership management.
class OwnedInterface {
 public:
  OwnedInterface() = default;
  virtual ~OwnedInterface() = default;
};

template <typename InterfaceImpl>
class DeleteOnTaskRunner {
 public:
  DeleteOnTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner = nullptr)
      : task_runner_(task_runner) {}
  void operator()(const InterfaceImpl* impl) {
    if (task_runner_) {
      if (!task_runner_->DeleteSoon(FROM_HERE, impl)) {
#if defined(UNIT_TEST)
        // Only logged under unit testing because leaks at shutdown
        // are acceptable under normal circumstances.
        LOG(ERROR) << "DeleteSoon failed on thread";
#endif  // UNIT_TEST
      }
    } else {
      delete impl;
    }
  }
  ~DeleteOnTaskRunner() = default;

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

template <typename InterfaceImpl>
class OwnedInterfaceImpl : public OwnedInterface {
 public:
  OwnedInterfaceImpl(
      std::unique_ptr<InterfaceImpl> impl,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner = nullptr)
      : impl_(impl.release(), DeleteOnTaskRunner<InterfaceImpl>(task_runner)) {}

  ~OwnedInterfaceImpl() = default;

  // The returned pointer is owned by this object, which must outlive it.
  InterfaceImpl* get() { return impl_.get(); }

 private:
  std::unique_ptr<InterfaceImpl, DeleteOnTaskRunner<InterfaceImpl>> impl_;

  DISALLOW_COPY_AND_ASSIGN(OwnedInterfaceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_OWNED_INTERFACE_H_
