// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

template <typename BindingType>
struct BindingSetTraits;

template <typename Interface>
struct BindingSetTraits<Binding<Interface>> {
  using ProxyType = InterfacePtr<Interface>;
  using RequestType = InterfaceRequest<Interface>;

  static RequestType GetProxy(ProxyType* proxy) {
    return mojo::GetProxy(proxy);
  }
};

enum class BindingSetDispatchMode {
  WITHOUT_CONTEXT,
  WITH_CONTEXT,
};

// Use this class to manage a set of bindings, which are automatically destroyed
// and removed from the set when the pipe they are bound to is disconnected.
template <typename Interface, typename BindingType = Binding<Interface>>
class BindingSet {
 public:
  using Traits = BindingSetTraits<BindingType>;
  using ProxyType = typename Traits::ProxyType;
  using RequestType = typename Traits::RequestType;

  BindingSet() : BindingSet(BindingSetDispatchMode::WITHOUT_CONTEXT) {}

  // Constructs a new BindingSet operating in |dispatch_mode|. If |WITH_CONTEXT|
  // is used, AddBinding() supports a |context| argument, and dispatch_context()
  // may be called during message or error dispatch to identify which specific
  // binding received the message or error.
  explicit BindingSet(BindingSetDispatchMode dispatch_mode)
      : dispatch_mode_(dispatch_mode) {}

  void set_connection_error_handler(const base::Closure& error_handler) {
    error_handler_ = error_handler;
  }

  // Adds a new binding to the set which binds |request| to |impl|. If |context|
  // is non-null, dispatch_context() will reflect this value during the extent
  // of any message or error dispatch targeting this specific binding. Note that
  // |context| may only be non-null if the BindingSet was constructed with
  // |BindingSetDispatchMode::WITH_CONTEXT|.
  void AddBinding(Interface* impl,
                  RequestType request,
                  void* context = nullptr) {
    DCHECK(!context || SupportsContext());
    std::unique_ptr<Entry> entry =
        base::MakeUnique<Entry>(impl, std::move(request), this, context);
    bindings_.insert(std::make_pair(entry.get(), std::move(entry)));
  }

  // Returns a proxy bound to one end of a pipe whose other end is bound to
  // |this|.
  ProxyType CreateInterfacePtrAndBind(Interface* impl) {
    ProxyType proxy;
    AddBinding(impl, Traits::GetProxy(&proxy));
    return proxy;
  }

  void CloseAllBindings() { bindings_.clear(); }

  bool empty() const { return bindings_.empty(); }

  // Implementations may call this when processing a dispatched message or
  // error. During the extent of message or error dispatch, this will return the
  // context associated with the specific binding which received the message or
  // error. Use AddBinding() to associated a context with a specific binding.
  //
  // Note that this may ONLY be called if the BindingSet was constructed with
  // |BindingSetDispatchMode::WITH_CONTEXT|.
  void* dispatch_context() const {
    DCHECK(SupportsContext());
    return dispatch_context_;
  }

  void FlushForTesting() {
    for (auto& binding : bindings_) {
      binding.first->FlushForTesting();
    }
  }

 private:
  friend class Entry;

  class Entry {
   public:
    Entry(Interface* impl,
          RequestType request,
          BindingSet* binding_set,
          void* context)
        : binding_(impl, std::move(request)),
          binding_set_(binding_set),
          context_(context) {
      if (binding_set->SupportsContext())
        binding_.AddFilter(base::MakeUnique<DispatchFilter>(this));
      binding_.set_connection_error_handler(base::Bind(
          &Entry::OnConnectionError, base::Unretained(this)));
    }

    void FlushForTesting() { binding_.FlushForTesting(); }

   private:
    class DispatchFilter : public MessageReceiver {
     public:
      explicit DispatchFilter(Entry* entry) : entry_(entry) {}
      ~DispatchFilter() override {}

     private:
      // MessageReceiver:
      bool Accept(Message* message) override {
        entry_->WillDispatch();
        return true;
      }

      Entry* entry_;

      DISALLOW_COPY_AND_ASSIGN(DispatchFilter);
    };

    void WillDispatch() {
      DCHECK(binding_set_->SupportsContext());
      binding_set_->SetDispatchContext(context_);
    }

    void OnConnectionError() {
      if (binding_set_->SupportsContext())
        WillDispatch();
      binding_set_->OnConnectionError(this);
    }

    BindingType binding_;
    BindingSet* const binding_set_;
    void* const context_;

    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  void SetDispatchContext(void* context) {
    DCHECK(SupportsContext());
    dispatch_context_ = context;
  }

  bool SupportsContext() const {
    return dispatch_mode_ == BindingSetDispatchMode::WITH_CONTEXT;
  }

  void OnConnectionError(Entry* entry) {
    auto it = bindings_.find(entry);
    DCHECK(it != bindings_.end());
    bindings_.erase(it);

    if (!error_handler_.is_null())
      error_handler_.Run();
  }

  BindingSetDispatchMode dispatch_mode_;
  base::Closure error_handler_;
  std::map<Entry*, std::unique_ptr<Entry>> bindings_;
  void* dispatch_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BindingSet);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_
