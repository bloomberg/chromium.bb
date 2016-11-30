// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_STRONG_BINDING_SET_H_
#define MEDIA_MOJO_SERVICES_STRONG_BINDING_SET_H_

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace media {

// This class manages a set of bindings. When the pipe a binding is bound to is
// disconnected, the binding is automatically destroyed and removed from the
// set, and the interface implementation is deleted. When the StrongBindingSet
// is destructed, all outstanding bindings in the set are destroyed and all the
// bound interface implementations are automatically deleted.
template <typename Interface>
class StrongBindingSet {
 public:
  using BindingId = size_t;
  using RequestType = mojo::InterfaceRequest<Interface>;

  StrongBindingSet() {}
  ~StrongBindingSet() {}

  // Adds a new binding to the set which binds |request| to |impl|.
  BindingId AddBinding(std::unique_ptr<Interface> impl, RequestType request) {
    BindingId binding_id = next_binding_id_++;
    std::unique_ptr<Entry> entry = base::MakeUnique<Entry>(
        std::move(impl), std::move(request), this, binding_id);
    bindings_.insert(std::make_pair(binding_id, std::move(entry)));
    return binding_id;
  }

  // Removes a binding from the set. Note that this is safe to call even if the
  // binding corresponding to |binding_id| has already been removed.
  // Returns |true| if the binding was removed and |false| if it didn't exist.
  bool RemoveBinding(BindingId binding_id) {
    auto it = bindings_.find(binding_id);
    if (it == bindings_.end())
      return false;
    bindings_.erase(it);
    return true;
  }

 private:
  class Entry {
   public:
    Entry(std::unique_ptr<Interface> impl,
          RequestType request,
          StrongBindingSet* binding_set,
          BindingId binding_id)
        : impl_(std::move(impl)),
          binding_(impl_.get(), std::move(request)),
          binding_set_(binding_set),
          binding_id_(binding_id) {
      // base::Unretained() is safe because |this| is owned by the
      // |binding_set|.
      binding_.set_connection_error_with_reason_handler(
          base::Bind(&StrongBindingSet::OnConnectionError,
                     base::Unretained(binding_set), binding_id));
    }

    // Holds ownership of the interface implementation.
    std::unique_ptr<Interface> impl_;

    mojo::Binding<Interface> binding_;
    StrongBindingSet* const binding_set_;
    const BindingId binding_id_;

    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  void OnConnectionError(BindingId binding_id,
                         uint32_t custom_reason,
                         const std::string& description) {
    bool success = RemoveBinding(binding_id);
    DCHECK(success) << "Binding for ID " << binding_id << " does not exist.";
  }

  std::map<BindingId, std::unique_ptr<Entry>> bindings_;
  BindingId next_binding_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(StrongBindingSet);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STRONG_BINDING_SET_H_
