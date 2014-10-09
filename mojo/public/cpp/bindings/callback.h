// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_

#include "mojo/public/cpp/bindings/lib/callback_internal.h"
#include "mojo/public/cpp/bindings/lib/shared_ptr.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {

template <typename Sig>
class Callback;

template <typename... Args>
class Callback<void(Args...)> {
 public:
  struct Runnable {
    virtual ~Runnable() {}
    virtual void Run(
        typename internal::Callback_ParamTraits<Args>::ForwardType...)
        const = 0;
  };

  Callback() {}

  // The Callback assumes ownership of |runnable|.
  explicit Callback(Runnable* runnable) : sink_(runnable) {}

  // Any class that is copy-constructable and has a compatible Run method may
  // be adapted to a Callback using this constructor.
  template <typename Sink>
  Callback(const Sink& sink)
      : sink_(new Adapter<Sink>(sink)) {}

  void Run(typename internal::Callback_ParamTraits<Args>::ForwardType... args)
      const {
    if (sink_.get())
      sink_->Run(internal::Forward(args)...);
  }

  bool is_null() const { return !sink_.get(); }

  void reset() { sink_.reset(); }

 private:
  template <typename Sink>
  struct Adapter : public Runnable {
    explicit Adapter(const Sink& sink) : sink(sink) {}
    virtual void Run(
        typename internal::Callback_ParamTraits<Args>::ForwardType... args)
        const override {
      sink.Run(internal::Forward(args)...);
    }
    Sink sink;
  };

  internal::SharedPtr<Runnable> sink_;
};

typedef Callback<void()> Closure;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_
