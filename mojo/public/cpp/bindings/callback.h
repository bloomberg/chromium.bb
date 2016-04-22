// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/lib/callback_internal.h"
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
        typename internal::Callback_ParamTraits<Args>::ForwardType...) = 0;
  };

  // Constructs a "null" callback that does nothing.
  Callback() {}

  // Constructs a callback that will run |runnable|. The callback takes
  // ownership of |runnable|.
  explicit Callback(Runnable* runnable) : sink_(new RunnableHolder(runnable)) {}

  // As above, but can take an object that isn't derived from Runnable, so long
  // as it has a compatible operator() or Run() method. operator() will be
  // preferred if the type has both.
  //
  // The std::enable_if is used to disable this constructor if the argument is
  // derived from Runnable. This is needed because the compiler will pick this
  // constructor instead of the Runnable* one above when the argument is of the
  // type of the derived class instead of down casted to a Runnable. i.e:
  // class Foo : public Callback::Runnable {
  //   ...
  // };
  // Callback cb(new Foo);
  //
  // The call site can fix this by using a static_cast to down cast to a
  // Runnable*, but that shouldn't be necessary.
  template <
      typename Sink,
      typename std::enable_if<!std::is_base_of<
          Runnable,
          typename std::remove_pointer<Sink>::type>::value>::type* = nullptr>
  Callback(const Sink& sink) {
    using sink_type = typename internal::Conditional<
        internal::HasCompatibleCallOperator<Sink, Args...>::value,
        FunctorAdapter<Sink>, RunnableAdapter<Sink>>::type;
    sink_ = new RunnableHolder(new sink_type(sink));
  }

  // As above, but can take a compatible function pointer.
  Callback(void (*function_ptr)(
      typename internal::Callback_ParamTraits<Args>::ForwardType...))
      : sink_(new RunnableHolder(new FunctionPtrAdapter(function_ptr))) {}

  // Executes the callback function.
  void Run(typename internal::Callback_ParamTraits<Args>::ForwardType... args)
      const {
    if (sink_)
      sink_->runnable->Run(std::forward<
                 typename internal::Callback_ParamTraits<Args>::ForwardType>(
          args)...);
  }

  bool is_null() const { return !sink_.get(); }

  // Resets the callback to the "null" state.
  void reset() { sink_ = nullptr; }

 private:
  // Adapts a class that has a Run() method but is not derived from Runnable to
  // be callable by Callback.
  template <typename Sink>
  struct RunnableAdapter : public Runnable {
    explicit RunnableAdapter(const Sink& sink) : sink(sink) {}
    virtual void Run(
        typename internal::Callback_ParamTraits<Args>::ForwardType... args)
        override {
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
        override {
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
        override {
      (*function_ptr)(
          std::forward<
              typename internal::Callback_ParamTraits<Args>::ForwardType>(
              args)...);
    }
    FunctionPtr function_ptr;
  };

  struct RunnableHolder : public base::RefCountedThreadSafe<RunnableHolder> {
    explicit RunnableHolder(Runnable* runnable) : runnable(runnable) {}

    std::unique_ptr<Runnable> runnable;

   private:
    friend class base::RefCountedThreadSafe<RunnableHolder>;
    ~RunnableHolder() {}
  };

  scoped_refptr<RunnableHolder> sink_;
};

// A specialization of Callback which takes no parameters.
typedef Callback<void()> Closure;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_
