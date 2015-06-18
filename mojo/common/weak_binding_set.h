// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_WEAK_BINDING_SET_H_
#define MOJO_COMMON_WEAK_BINDING_SET_H_

#include <algorithm>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mojo {

template <typename Interface>
class WeakBinding;

// Use this class to manage a set of weak pointers to bindings each of which is
// owned by the pipe they are bound to.
template <typename Interface>
class WeakBindingSet : public ErrorHandler {
 public:
  WeakBindingSet() : error_handler_(nullptr) {}
  ~WeakBindingSet() { CloseAllBindings(); }

  void set_error_handler(ErrorHandler* error_handler) {
    error_handler_ = error_handler;
  }

  void AddBinding(Interface* impl, InterfaceRequest<Interface> request) {
    auto binding = new WeakBinding<Interface>(impl, request.Pass());
    binding->set_error_handler(this);
    bindings_.push_back(binding->GetWeakPtr());
  }

  void CloseAllBindings() {
    for (const auto& it : bindings_) {
      if (it)
        it->Close();
    }
    bindings_.clear();
  }

  bool empty() const { return bindings_.empty(); }

 private:
  // ErrorHandler implementation.
  void OnConnectionError() override {
    // Clear any deleted bindings.
    bindings_.erase(
        std::remove_if(bindings_.begin(), bindings_.end(),
                       [](const base::WeakPtr<WeakBinding<Interface>>& p) {
          return p.get() == nullptr;
        }),
        bindings_.end());

    if (error_handler_)
      error_handler_->OnConnectionError();
  }

  ErrorHandler* error_handler_;
  std::vector<base::WeakPtr<WeakBinding<Interface>>> bindings_;

  DISALLOW_COPY_AND_ASSIGN(WeakBindingSet);
};

template <typename Interface>
class WeakBinding : public ErrorHandler {
 public:
  WeakBinding(Interface* impl, InterfaceRequest<Interface> request)
      : binding_(impl, request.Pass()),
        error_handler_(nullptr),
        weak_ptr_factory_(this) {
    binding_.set_error_handler(this);
  }

  ~WeakBinding() override {}

  void set_error_handler(ErrorHandler* error_handler) {
    error_handler_ = error_handler;
  }

  base::WeakPtr<WeakBinding> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void Close() { binding_.Close(); }

  // ErrorHandler implementation.
  void OnConnectionError() override {
    ErrorHandler* error_handler = error_handler_;
    delete this;
    if (error_handler)
      error_handler->OnConnectionError();
  }

 private:
  mojo::Binding<Interface> binding_;
  ErrorHandler* error_handler_;
  base::WeakPtrFactory<WeakBinding> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WeakBinding);
};

}  // namespace mojo

#endif  // MOJO_COMMON_WEAK_BINDING_SET_H_
