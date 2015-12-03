// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_

#include <utility>

#include "mojo/public/cpp/bindings/lib/callback_internal.h"
#include "mojo/public/cpp/bindings/lib/shared_ptr.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {

template <typename Sig>
class Callback;

// Represents a callback with any number of parameters and no return value. The
// callback is executed by calling its Run() method. The callback may be "null",
// meaning it does nothing.
template <typename... Args>
class Callback<void(Args...)> {
 public:
  // An interface that may be implemented to define the Run() method.
  struct Runnable {
    virtual ~Runnable() {}
    virtual void Run(
        // ForwardType ensures String is passed as a const reference. Can't use
        // universal refs (Args&&) here since this method itself isn't templated
        // because it is a virtual interface. So we have to take the arguments
        // all by value (except String which we take as a const reference due to
        // ForwardType).
        typename internal::Callback_ParamTraits<Args>::ForwardType...)
        const = 0;
  };

  // Constructs a "null" callback that does nothing.
  Callback() {}

  // Constructs a callback that will run |runnable|. The callback takes
  // ownership of |runnable|.
  explicit Callback(Runnable* runnable) : sink_(runnable) {}

  // As above, but can take an object that isn't derived from Runnable, so long
  // as it has a compatible operator() or Run() method. operator() will be
  // preferred if the type has both.
  template <typename Sink>
  Callback(const Sink& sink) {
    using sink_type = typename internal::Conditional<
        internal::HasCompatibleCallOperator<Sink, Args...>::value,
        FunctorAdapter<Sink>, RunnableAdapter<Sink>>::type;
    sink_ = internal::SharedPtr<Runnable>(new sink_type(sink));
  }

  // As above, but can take a compatible function pointer.
  Callback(void (*function_ptr)(
      typename internal::Callback_ParamTraits<Args>::ForwardType...))
      : sink_(new FunctionPtrAdapter(function_ptr)) {}

  // Executes the callback function.
  void Run(typename internal::Callback_ParamTraits<Args>::ForwardType... args)
      const {
    if (sink_.get())
      sink_->Run(std::forward<
                 typename internal::Callback_ParamTraits<Args>::ForwardType>(
          args)...);
  }

  bool is_null() const { return !sink_.get(); }

  // Resets the callback to the "null" state.
  void reset() { sink_.reset(); }

 private:
  // Adapts a class that has a Run() method but is not derived from Runnable to
  // be callable by Callback.
  template <typename Sink>
  struct RunnableAdapter : public Runnable {
    explicit RunnableAdapter(const Sink& sink) : sink(sink) {}
    virtual void Run(
        typename internal::Callback_ParamTraits<Args>::ForwardType... args)
        const override {
      sink.Run(std::forward<
               typename internal::Callback_ParamTraits<Args>::ForwardType>(
          args)...);
    }
    Sink sink;
  };

  // Adapts a class that has a compatible operator() to be callable by Callback.
  template <typename Sink>
  struct FunctorAdapter : public Runnable {
    explicit FunctorAdapter(const Sink& sink) : sink(sink) {}
    virtual void Run(
        typename internal::Callback_ParamTraits<Args>::ForwardType... args)
        const override {
      sink.operator()(
          std::forward<
              typename internal::Callback_ParamTraits<Args>::ForwardType>(
              args)...);
    }
    Sink sink;
  };

  // Adapts a function pointer.
  struct FunctionPtrAdapter : public Runnable {
   private:
    using FunctionPtr =
        void (*)(typename internal::Callback_ParamTraits<Args>::ForwardType...);

   public:
    explicit FunctionPtrAdapter(FunctionPtr function_ptr)
        : function_ptr(function_ptr) {}
    virtual void Run(
        typename internal::Callback_ParamTraits<Args>::ForwardType... args)
        const override {
      (*function_ptr)(
          std::forward<
              typename internal::Callback_ParamTraits<Args>::ForwardType>(
              args)...);
    }
    FunctionPtr function_ptr;
  };

  internal::SharedPtr<Runnable> sink_;
};

// A specialization of Callback which takes no parameters.
typedef Callback<void()> Closure;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_
