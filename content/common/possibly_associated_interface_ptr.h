// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_POSSIBLY_ASSOCIATED_INTERFACE_PTR_H_
#define CONTENT_COMMON_POSSIBLY_ASSOCIATED_INTERFACE_PTR_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"

namespace content {

// PossiblyAssociatedInterfacePtr<T> contains mojo::InterfacePtr<T> or
// mojo::AssociatedInterfacePtr<T>, but not both. Mojo-related functions in
// mojo::InterfacePtr<T> and mojo::AssociatedInterfacePtr<T> are not accessible,
// but a user can access the raw pointer to the interface.
template <typename T>
class PossiblyAssociatedInterfacePtr final {
 public:
  PossiblyAssociatedInterfacePtr() {}
  PossiblyAssociatedInterfacePtr(std::nullptr_t) {}
  PossiblyAssociatedInterfacePtr(mojo::InterfacePtr<T> independent_ptr)
      : independent_ptr_(std::move(independent_ptr)) {}
  PossiblyAssociatedInterfacePtr(mojo::AssociatedInterfacePtr<T> associated_ptr)
      : associated_ptr_(std::move(associated_ptr)) {}

  PossiblyAssociatedInterfacePtr(PossiblyAssociatedInterfacePtr&& other) {
    independent_ptr_ = std::move(other.independent_ptr_);
    associated_ptr_ = std::move(other.associated_ptr_);
  }
  ~PossiblyAssociatedInterfacePtr() {}

  PossiblyAssociatedInterfacePtr& operator=(
      PossiblyAssociatedInterfacePtr&& other) {
    independent_ptr_ = std::move(other.independent_ptr_);
    associated_ptr_ = std::move(other.associated_ptr_);
    return *this;
  }

  T* get() const {
    return independent_ptr_ ? independent_ptr_.get() : associated_ptr_.get();
  }
  T* operator->() const { return get(); }
  T& operator*() const { return *get(); }
  explicit operator bool() const { return get(); }

 private:
  mojo::InterfacePtr<T> independent_ptr_;
  mojo::AssociatedInterfacePtr<T> associated_ptr_;

  DISALLOW_COPY_AND_ASSIGN(PossiblyAssociatedInterfacePtr);
};

}  // namespace content

#endif  // CONTENT_COMMON_POSSIBLY_ASSOCIATED_INTERFACE_PTR_H_
