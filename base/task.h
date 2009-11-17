// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_H_
#define BASE_TASK_H_

#include "base/non_thread_safe.h"
#include "base/tracked.h"
#include "base/tuple.h"
#include "base/weak_ptr.h"

// Task ------------------------------------------------------------------------
//
// A task is a generic runnable thingy, usually used for running code on a
// different thread or for scheduling future tasks off of the message loop.

class Task : public tracked_objects::Tracked {
 public:
  Task() {}
  virtual ~Task() {}

  // Tasks are automatically deleted after Run is called.
  virtual void Run() = 0;
};

class CancelableTask : public Task {
 public:
  // Not all tasks support cancellation.
  virtual void Cancel() = 0;
};

// Scoped Factories ------------------------------------------------------------
//
// These scoped factory objects can be used by non-refcounted objects to safely
// place tasks in a message loop.  Each factory guarantees that the tasks it
// produces will not run after the factory is destroyed.  Commonly, factories
// are declared as class members, so the class' tasks will automatically cancel
// when the class instance is destroyed.
//
// Exampe Usage:
//
// class MyClass {
//  private:
//   // This factory will be used to schedule invocations of SomeMethod.
//   ScopedRunnableMethodFactory<MyClass> some_method_factory_;
//
//  public:
//   // It is safe to suppress warning 4355 here.
//   MyClass() : some_method_factory_(this) { }
//
//   void SomeMethod() {
//     // If this function might be called directly, you might want to revoke
//     // any outstanding runnable methods scheduled to call it.  If it's not
//     // referenced other than by the factory, this is unnecessary.
//     some_method_factory_.RevokeAll();
//     ...
//   }
//
//   void ScheduleSomeMethod() {
//     // If you'd like to only only have one pending task at a time, test for
//     // |empty| before manufacturing another task.
//     if (!some_method_factory_.empty())
//       return;
//
//     // The factories are not thread safe, so always invoke on
//     // |MessageLoop::current()|.
//     MessageLoop::current()->PostDelayedTask(FROM_HERE,
//         some_method_factory_.NewRunnableMethod(&MyClass::SomeMethod),
//         kSomeMethodDelayMS);
//   }
// };

// A ScopedRunnableMethodFactory creates runnable methods for a specified
// object.  This is particularly useful for generating callbacks for
// non-reference counted objects when the factory is a member of the object.
template<class T>
class ScopedRunnableMethodFactory {
 public:
  explicit ScopedRunnableMethodFactory(T* object) : weak_factory_(object) {
  }

  template <class Method>
  inline Task* NewRunnableMethod(Method method) {
    return new RunnableMethod<Method, Tuple0>(
        weak_factory_.GetWeakPtr(), method, MakeTuple());
  }

  template <class Method, class A>
  inline Task* NewRunnableMethod(Method method, const A& a) {
    return new RunnableMethod<Method, Tuple1<A> >(
        weak_factory_.GetWeakPtr(), method, MakeTuple(a));
  }

  template <class Method, class A, class B>
  inline Task* NewRunnableMethod(Method method, const A& a, const B& b) {
    return new RunnableMethod<Method, Tuple2<A, B> >(
        weak_factory_.GetWeakPtr(), method, MakeTuple(a, b));
  }

  template <class Method, class A, class B, class C>
  inline Task* NewRunnableMethod(Method method,
                                 const A& a,
                                 const B& b,
                                 const C& c) {
    return new RunnableMethod<Method, Tuple3<A, B, C> >(
        weak_factory_.GetWeakPtr(), method, MakeTuple(a, b, c));
  }

  template <class Method, class A, class B, class C, class D>
  inline Task* NewRunnableMethod(Method method,
                                 const A& a,
                                 const B& b,
                                 const C& c,
                                 const D& d) {
    return new RunnableMethod<Method, Tuple4<A, B, C, D> >(
        weak_factory_.GetWeakPtr(), method, MakeTuple(a, b, c, d));
  }

  template <class Method, class A, class B, class C, class D, class E>
  inline Task* NewRunnableMethod(Method method,
                                 const A& a,
                                 const B& b,
                                 const C& c,
                                 const D& d,
                                 const E& e) {
    return new RunnableMethod<Method, Tuple5<A, B, C, D, E> >(
        weak_factory_.GetWeakPtr(), method, MakeTuple(a, b, c, d, e));
  }

  void RevokeAll() { weak_factory_.InvalidateWeakPtrs(); }

  bool empty() const { return !weak_factory_.HasWeakPtrs(); }

 protected:
  template <class Method, class Params>
  class RunnableMethod : public Task {
   public:
    RunnableMethod(const base::WeakPtr<T>& obj, Method meth, const Params& params)
        : obj_(obj),
          meth_(meth),
          params_(params) {
    }

    virtual void Run() {
      if (obj_)
        DispatchToMethod(obj_.get(), meth_, params_);
    }

   private:
    base::WeakPtr<T> obj_;
    Method meth_;
    Params params_;

    DISALLOW_COPY_AND_ASSIGN(RunnableMethod);
  };

 private:
  base::WeakPtrFactory<T> weak_factory_;
};

// General task implementations ------------------------------------------------

// Task to delete an object
template<class T>
class DeleteTask : public CancelableTask {
 public:
  explicit DeleteTask(T* obj) : obj_(obj) {
  }
  virtual void Run() {
    delete obj_;
  }
  virtual void Cancel() {
    obj_ = NULL;
  }
 private:
  T* obj_;
};

// Task to Release() an object
template<class T>
class ReleaseTask : public CancelableTask {
 public:
  explicit ReleaseTask(T* obj) : obj_(obj) {
  }
  virtual void Run() {
    if (obj_)
      obj_->Release();
  }
  virtual void Cancel() {
    obj_ = NULL;
  }
 private:
  T* obj_;
};

// RunnableMethodTraits --------------------------------------------------------
//
// This traits-class is used by RunnableMethod to manage the lifetime of the
// callee object.  By default, it is assumed that the callee supports AddRef
// and Release methods.  A particular class can specialize this template to
// define other lifetime management.  For example, if the callee is known to
// live longer than the RunnableMethod object, then a RunnableMethodTraits
// struct could be defined with empty RetainCallee and ReleaseCallee methods.

template <class T>
struct RunnableMethodTraits {
  RunnableMethodTraits() {
#ifndef NDEBUG
    origin_thread_id_ = PlatformThread::CurrentId();
#endif
  }

  ~RunnableMethodTraits() {
#ifndef NDEBUG
    // If destroyed on a separate thread, then we had better have been using
    // thread-safe reference counting!
    if (origin_thread_id_ != PlatformThread::CurrentId())
      DCHECK(T::ImplementsThreadSafeReferenceCounting());
#endif
  }

  void RetainCallee(T* obj) {
#ifndef NDEBUG
    // Catch NewRunnableMethod being called in an object's constructor.  This
    // isn't safe since the method can be invoked before the constructor
    // completes, causing the object to be deleted.
    obj->AddRef();
    obj->Release();
#endif
    obj->AddRef();
  }

  void ReleaseCallee(T* obj) {
    obj->Release();
  }

 private:
#ifndef NDEBUG
  PlatformThreadId origin_thread_id_;
#endif
};

// RunnableMethod and RunnableFunction -----------------------------------------
//
// Runnable methods are a type of task that call a function on an object when
// they are run. We implement both an object and a set of NewRunnableMethod and
// NewRunnableFunction functions for convenience. These functions are
// overloaded and will infer the template types, simplifying calling code.
//
// The template definitions all use the following names:
// T                - the class type of the object you're supplying
//                    this is not needed for the Static version of the call
// Method/Function  - the signature of a pointer to the method or function you
//                    want to call
// Param            - the parameter(s) to the method, possibly packed as a Tuple
// A                - the first parameter (if any) to the method
// B                - the second parameter (if any) to the mathod
//
// Put these all together and you get an object that can call a method whose
// signature is:
//   R T::MyFunction([A[, B]])
//
// Usage:
// PostTask(FROM_HERE, NewRunnableMethod(object, &Object::method[, a[, b]])
// PostTask(FROM_HERE, NewRunnableFunction(&function[, a[, b]])

// RunnableMethod and NewRunnableMethod implementation -------------------------

template <class T, class Method, class Params>
class RunnableMethod : public CancelableTask {
 public:
  RunnableMethod(T* obj, Method meth, const Params& params)
      : obj_(obj), meth_(meth), params_(params) {
    traits_.RetainCallee(obj_);
  }

  ~RunnableMethod() {
    ReleaseCallee();
  }

  virtual void Run() {
    if (obj_)
      DispatchToMethod(obj_, meth_, params_);
  }

  virtual void Cancel() {
    ReleaseCallee();
  }

 private:
  void ReleaseCallee() {
    if (obj_) {
      traits_.ReleaseCallee(obj_);
      obj_ = NULL;
    }
  }

  T* obj_;
  Method meth_;
  Params params_;
  RunnableMethodTraits<T> traits_;
};

template <class T, class Method>
inline CancelableTask* NewRunnableMethod(T* object, Method method) {
  return new RunnableMethod<T, Method, Tuple0>(object, method, MakeTuple());
}

template <class T, class Method, class A>
inline CancelableTask* NewRunnableMethod(T* object, Method method, const A& a) {
  return new RunnableMethod<T, Method, Tuple1<A> >(object,
                                                   method,
                                                   MakeTuple(a));
}

template <class T, class Method, class A, class B>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
const A& a, const B& b) {
  return new RunnableMethod<T, Method, Tuple2<A, B> >(object, method,
                                                      MakeTuple(a, b));
}

template <class T, class Method, class A, class B, class C>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                          const A& a, const B& b, const C& c) {
  return new RunnableMethod<T, Method, Tuple3<A, B, C> >(object, method,
                                                         MakeTuple(a, b, c));
}

template <class T, class Method, class A, class B, class C, class D>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                          const A& a, const B& b,
                                          const C& c, const D& d) {
  return new RunnableMethod<T, Method, Tuple4<A, B, C, D> >(object, method,
                                                            MakeTuple(a, b,
                                                                      c, d));
}

template <class T, class Method, class A, class B, class C, class D, class E>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                          const A& a, const B& b,
                                          const C& c, const D& d, const E& e) {
  return new RunnableMethod<T,
                            Method,
                            Tuple5<A, B, C, D, E> >(object,
                                                    method,
                                                    MakeTuple(a, b, c, d, e));
}

template <class T, class Method, class A, class B, class C, class D, class E,
          class F>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                          const A& a, const B& b,
                                          const C& c, const D& d, const E& e,
                                          const F& f) {
  return new RunnableMethod<T,
                            Method,
                            Tuple6<A, B, C, D, E, F> >(object,
                                                       method,
                                                       MakeTuple(a, b, c, d, e,
                                                                 f));
}

template <class T, class Method, class A, class B, class C, class D, class E,
          class F, class G>
inline CancelableTask* NewRunnableMethod(T* object, Method method,
                                         const A& a, const B& b,
                                         const C& c, const D& d, const E& e,
                                         const F& f, const G& g) {
  return new RunnableMethod<T,
                            Method,
                            Tuple7<A, B, C, D, E, F, G> >(object,
                                                          method,
                                                          MakeTuple(a, b, c, d,
                                                                    e, f, g));
}

// RunnableFunction and NewRunnableFunction implementation ---------------------

template <class Function, class Params>
class RunnableFunction : public CancelableTask {
 public:
  RunnableFunction(Function function, const Params& params)
      : function_(function), params_(params) {
  }

  ~RunnableFunction() {
  }

  virtual void Run() {
    if (function_)
      DispatchToFunction(function_, params_);
  }

  virtual void Cancel() {
  }

 private:
  Function function_;
  Params params_;
};

template <class Function>
inline CancelableTask* NewRunnableFunction(Function function) {
  return new RunnableFunction<Function, Tuple0>(function, MakeTuple());
}

template <class Function, class A>
inline CancelableTask* NewRunnableFunction(Function function, const A& a) {
  return new RunnableFunction<Function, Tuple1<A> >(function, MakeTuple(a));
}

template <class Function, class A, class B>
inline CancelableTask* NewRunnableFunction(Function function,
                                           const A& a, const B& b) {
  return new RunnableFunction<Function, Tuple2<A, B> >(function,
                                                       MakeTuple(a, b));
}

template <class Function, class A, class B, class C>
inline CancelableTask* NewRunnableFunction(Function function,
                                           const A& a, const B& b,
                                           const C& c) {
  return new RunnableFunction<Function, Tuple3<A, B, C> >(function,
                                                          MakeTuple(a, b, c));
}

template <class Function, class A, class B, class C, class D>
inline CancelableTask* NewRunnableFunction(Function function,
                                           const A& a, const B& b,
                                           const C& c, const D& d) {
  return new RunnableFunction<Function, Tuple4<A, B, C, D> >(function,
                                                             MakeTuple(a, b,
                                                                       c, d));
}

template <class Function, class A, class B, class C, class D, class E>
inline CancelableTask* NewRunnableFunction(Function function,
                                           const A& a, const B& b,
                                           const C& c, const D& d,
                                           const E& e) {
  return new RunnableFunction<Function, Tuple5<A, B, C, D, E> >(function,
                                                                MakeTuple(a, b,
                                                                          c, d,
                                                                          e));
}

// Callback --------------------------------------------------------------------
//
// A Callback is like a Task but with unbound parameters. It is basically an
// object-oriented function pointer.
//
// Callbacks are designed to work with Tuples.  A set of helper functions and
// classes is provided to hide the Tuple details from the consumer.  Client
// code will generally work with the CallbackRunner base class, which merely
// provides a Run method and is returned by the New* functions. This allows
// users to not care which type of class implements the callback, only that it
// has a certain number and type of arguments.
//
// The implementation of this is done by CallbackImpl, which inherits
// CallbackStorage to store the data. This allows the storage of the data
// (requiring the class type T) to be hidden from users, who will want to call
// this regardless of the implementor's type T.
//
// Note that callbacks currently have no facility for cancelling or abandoning
// them. We currently handle this at a higher level for cases where this is
// necessary. The pointer in a callback must remain valid until the callback
// is made.
//
// Like Task, the callback executor is responsible for deleting the callback
// pointer once the callback has executed.
//
// Example client usage:
//   void Object::DoStuff(int, string);
//   Callback2<int, string>::Type* callback =
//       NewCallback(obj, &Object::DoStuff);
//   callback->Run(5, string("hello"));
//   delete callback;
// or, equivalently, using tuples directly:
//   CallbackRunner<Tuple2<int, string> >* callback =
//       NewCallback(obj, &Object::DoStuff);
//   callback->RunWithParams(MakeTuple(5, string("hello")));
//
// There is also a 0-args version that returns a value.  Example:
//   int Object::GetNextInt();
//   CallbackWithReturnValue<int>::Type* callback =
//       NewCallbackWithReturnValue(obj, &Object::GetNextInt);
//   int next_int = callback->Run();
//   delete callback;

// Base for all Callbacks that handles storage of the pointers.
template <class T, typename Method>
class CallbackStorage {
 public:
  CallbackStorage(T* obj, Method meth) : obj_(obj), meth_(meth) {
  }

 protected:
  T* obj_;
  Method meth_;
};

// Interface that is exposed to the consumer, that does the actual calling
// of the method.
template <typename Params>
class CallbackRunner {
 public:
  typedef Params TupleType;

  virtual ~CallbackRunner() {}
  virtual void RunWithParams(const Params& params) = 0;

  // Convenience functions so callers don't have to deal with Tuples.
  inline void Run() {
    RunWithParams(Tuple0());
  }

  template <typename Arg1>
  inline void Run(const Arg1& a) {
    RunWithParams(Params(a));
  }

  template <typename Arg1, typename Arg2>
  inline void Run(const Arg1& a, const Arg2& b) {
    RunWithParams(Params(a, b));
  }

  template <typename Arg1, typename Arg2, typename Arg3>
  inline void Run(const Arg1& a, const Arg2& b, const Arg3& c) {
    RunWithParams(Params(a, b, c));
  }

  template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
  inline void Run(const Arg1& a, const Arg2& b, const Arg3& c, const Arg4& d) {
    RunWithParams(Params(a, b, c, d));
  }

  template <typename Arg1, typename Arg2, typename Arg3,
            typename Arg4, typename Arg5>
  inline void Run(const Arg1& a, const Arg2& b, const Arg3& c,
                  const Arg4& d, const Arg5& e) {
    RunWithParams(Params(a, b, c, d, e));
  }
};

template <class T, typename Method, typename Params>
class CallbackImpl : public CallbackStorage<T, Method>,
                     public CallbackRunner<Params> {
 public:
  CallbackImpl(T* obj, Method meth) : CallbackStorage<T, Method>(obj, meth) {
  }
  virtual void RunWithParams(const Params& params) {
    // use "this->" to force C++ to look inside our templatized base class; see
    // Effective C++, 3rd Ed, item 43, p210 for details.
    DispatchToMethod(this->obj_, this->meth_, params);
  }
};

// 0-arg implementation
struct Callback0 {
  typedef CallbackRunner<Tuple0> Type;
};

template <class T>
typename Callback0::Type* NewCallback(T* object, void (T::*method)()) {
  return new CallbackImpl<T, void (T::*)(), Tuple0 >(object, method);
}

// 1-arg implementation
template <typename Arg1>
struct Callback1 {
  typedef CallbackRunner<Tuple1<Arg1> > Type;
};

template <class T, typename Arg1>
typename Callback1<Arg1>::Type* NewCallback(T* object,
                                            void (T::*method)(Arg1)) {
  return new CallbackImpl<T, void (T::*)(Arg1), Tuple1<Arg1> >(object, method);
}

// 2-arg implementation
template <typename Arg1, typename Arg2>
struct Callback2 {
  typedef CallbackRunner<Tuple2<Arg1, Arg2> > Type;
};

template <class T, typename Arg1, typename Arg2>
typename Callback2<Arg1, Arg2>::Type* NewCallback(
    T* object,
    void (T::*method)(Arg1, Arg2)) {
  return new CallbackImpl<T, void (T::*)(Arg1, Arg2),
      Tuple2<Arg1, Arg2> >(object, method);
}

// 3-arg implementation
template <typename Arg1, typename Arg2, typename Arg3>
struct Callback3 {
  typedef CallbackRunner<Tuple3<Arg1, Arg2, Arg3> > Type;
};

template <class T, typename Arg1, typename Arg2, typename Arg3>
typename Callback3<Arg1, Arg2, Arg3>::Type* NewCallback(
    T* object,
    void (T::*method)(Arg1, Arg2, Arg3)) {
  return new CallbackImpl<T,  void (T::*)(Arg1, Arg2, Arg3),
      Tuple3<Arg1, Arg2, Arg3> >(object, method);
}

// 4-arg implementation
template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct Callback4 {
  typedef CallbackRunner<Tuple4<Arg1, Arg2, Arg3, Arg4> > Type;
};

template <class T, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
typename Callback4<Arg1, Arg2, Arg3, Arg4>::Type* NewCallback(
    T* object,
    void (T::*method)(Arg1, Arg2, Arg3, Arg4)) {
  return new CallbackImpl<T, void (T::*)(Arg1, Arg2, Arg3, Arg4),
      Tuple4<Arg1, Arg2, Arg3, Arg4> >(object, method);
}

// 5-arg implementation
template <typename Arg1, typename Arg2, typename Arg3,
          typename Arg4, typename Arg5>
struct Callback5 {
  typedef CallbackRunner<Tuple5<Arg1, Arg2, Arg3, Arg4, Arg5> > Type;
};

template <class T, typename Arg1, typename Arg2,
          typename Arg3, typename Arg4, typename Arg5>
typename Callback5<Arg1, Arg2, Arg3, Arg4, Arg5>::Type* NewCallback(
    T* object,
    void (T::*method)(Arg1, Arg2, Arg3, Arg4, Arg5)) {
  return new CallbackImpl<T, void (T::*)(Arg1, Arg2, Arg3, Arg4, Arg5),
      Tuple5<Arg1, Arg2, Arg3, Arg4, Arg5> >(object, method);
}

// An UnboundMethod is a wrapper for a method where the actual object is
// provided at Run dispatch time.
template <class T, class Method, class Params>
class UnboundMethod {
 public:
  UnboundMethod(Method m, Params p) : m_(m), p_(p) {}
  void Run(T* obj) const {
    DispatchToMethod(obj, m_, p_);
  }
 private:
  Method m_;
  Params p_;
};

// Return value implementation with no args.
template <typename ReturnValue>
struct CallbackWithReturnValue {
  class Type {
   public:
    virtual ReturnValue Run() = 0;
  };
};

template <class T, typename Method, typename ReturnValue>
class CallbackWithReturnValueImpl
    : public CallbackStorage<T, Method>,
      public CallbackWithReturnValue<ReturnValue>::Type {
 public:
  CallbackWithReturnValueImpl(T* obj, Method meth)
      : CallbackStorage<T, Method>(obj, meth) {}

  virtual ReturnValue Run() {
    return (this->obj_->*(this->meth_))();
  }
};

template <class T, typename ReturnValue>
typename CallbackWithReturnValue<ReturnValue>::Type*
NewCallbackWithReturnValue(T* object, ReturnValue (T::*method)()) {
  return new CallbackWithReturnValueImpl<T, ReturnValue (T::*)(), ReturnValue>(
      object, method);
}


#endif  // BASE_TASK_H_
