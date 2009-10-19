// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_HELPER_GMOCK_H_
#define CHROME_FRAME_TEST_HELPER_GMOCK_H_
// The intention of this file is to make possible using GMock actions in
// all of its syntactic beauty. Classes and helper functions could be used as
// more generic variants of Task and Callback classes (see base/task.h)
// Mutant supports both pre-bound arguments (like Task) and call-time arguments
// (like Callback) - hence the name. :-)
// DispatchToMethod supporting two sets of arguments -
// pre-bound (P) and call-time (C) as well as return value type is templatized
// It will also try to call the selected method even if provided pre-bound args
// does not match exactly with the function signature - hence the X1, X2
// parameters in CreateFunctor.

#include "base/linked_ptr.h"
#include "base/task.h"   // for CallBackStorage
#include "base/tuple.h"  // for Tuple


// 0 - 0
template <typename R, typename T, typename Method>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple0& p,
                          const Tuple0& c) {
  return (obj->*method)();
}

// 0 - 1
template <typename R, typename T, typename Method, typename C1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple0& p,
                          const Tuple1<C1>& c) {
  return (obj->*method)(c.a);
}

// 0 - 2
template <typename R, typename T, typename Method, typename C1, typename C2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple0& p,
                          const Tuple2<C1, C2>& c) {
  return (obj->*method)(c.a, c.b);
}

// 0 - 3
template <typename R, typename T, typename Method, typename C1, typename C2,
          typename C3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple0& p,
                          const Tuple3<C1, C2, C3>& c) {
  return (obj->*method)(c.a, c.b, c.c);
}

// 0 - 4
template <typename R, typename T, typename Method, typename C1, typename C2,
          typename C3, typename C4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple0& p,
                          const Tuple4<C1, C2, C3, C4>& c) {
  return (obj->*method)(c.a, c.b, c.c, c.d);
}

// 1 - 0
template <typename R, typename T, typename Method, typename P1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple1<P1>& p,
                          const Tuple0& c) {
  return (obj->*method)(p.a);
}

// 1 - 1
template <typename R, typename T, typename Method, typename P1, typename C1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple1<P1>& p,
                          const Tuple1<C1>& c) {
  return (obj->*method)(p.a, c.a);
}

// 1 - 2
template <typename R, typename T, typename Method, typename P1, typename C1,
          typename C2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple1<P1>& p,
                          const Tuple2<C1, C2>& c) {
  return (obj->*method)(p.a, c.a, c.b);
}

// 1 - 3
template <typename R, typename T, typename Method, typename P1, typename C1,
          typename C2, typename C3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple1<P1>& p,
                          const Tuple3<C1, C2, C3>& c) {
  return (obj->*method)(p.a, c.a, c.b, c.c);
}

// 1 - 4
template <typename R, typename T, typename Method, typename P1, typename C1,
          typename C2, typename C3, typename C4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple1<P1>& p,
                          const Tuple4<C1, C2, C3, C4>& c) {
  return (obj->*method)(p.a, c.a, c.b, c.c, c.d);
}

// 2 - 0
template <typename R, typename T, typename Method, typename P1, typename P2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple2<P1, P2>& p,
                          const Tuple0& c) {
  return (obj->*method)(p.a, p.b);
}

// 2 - 1
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename C1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple2<P1, P2>& p,
                          const Tuple1<C1>& c) {
  return (obj->*method)(p.a, p.b, c.a);
}

// 2 - 2
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename C1, typename C2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple2<P1, P2>& p,
                          const Tuple2<C1, C2>& c) {
  return (obj->*method)(p.a, p.b, c.a, c.b);
}

// 2 - 3
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename C1, typename C2, typename C3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple2<P1, P2>& p,
                          const Tuple3<C1, C2, C3>& c) {
  return (obj->*method)(p.a, p.b, c.a, c.b, c.c);
}

// 2 - 4
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename C1, typename C2, typename C3, typename C4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple2<P1, P2>& p,
                          const Tuple4<C1, C2, C3, C4>& c) {
  return (obj->*method)(p.a, p.b, c.a, c.b, c.c, c.d);
}

// 3 - 0
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple3<P1, P2, P3>& p,
                          const Tuple0& c) {
  return (obj->*method)(p.a, p.b, p.c);
}

// 3 - 1
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename C1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple3<P1, P2, P3>& p,
                          const Tuple1<C1>& c) {
  return (obj->*method)(p.a, p.b, p.c, c.a);
}

// 3 - 2
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename C1, typename C2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple3<P1, P2, P3>& p,
                          const Tuple2<C1, C2>& c) {
  return (obj->*method)(p.a, p.b, p.c, c.a, c.b);
}

// 3 - 3
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename C1, typename C2, typename C3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple3<P1, P2, P3>& p,
                          const Tuple3<C1, C2, C3>& c) {
  return (obj->*method)(p.a, p.b, p.c, c.a, c.b, c.c);
}

// 3 - 4
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename C1, typename C2, typename C3, typename C4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple3<P1, P2, P3>& p,
                          const Tuple4<C1, C2, C3, C4>& c) {
  return (obj->*method)(p.a, p.b, p.c, c.a, c.b, c.c, c.d);
}

// 4 - 0
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename P4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple4<P1, P2, P3, P4>& p,
                          const Tuple0& c) {
  return (obj->*method)(p.a, p.b, p.c, p.d);
}

// 4 - 1
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename P4, typename C1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple4<P1, P2, P3, P4>& p,
                          const Tuple1<C1>& c) {
  return (obj->*method)(p.a, p.b, p.c, p.d, c.a);
}

// 4 - 2
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename P4, typename C1, typename C2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple4<P1, P2, P3, P4>& p,
                          const Tuple2<C1, C2>& c) {
  return (obj->*method)(p.a, p.b, p.c, p.d, c.a, c.b);
}

// 4 - 3
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename P4, typename C1, typename C2, typename C3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple4<P1, P2, P3, P4>& p,
                          const Tuple3<C1, C2, C3>& c) {
  return (obj->*method)(p.a, p.b, p.c, p.d, c.a, c.b, c.c);
}

// 4 - 4
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename P4, typename C1, typename C2, typename C3,
          typename C4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple4<P1, P2, P3, P4>& p,
                          const Tuple4<C1, C2, C3, C4>& c) {
  return (obj->*method)(p.a, p.b, p.c, p.d, c.a, c.b, c.c, c.d);
}

// Interface that is exposed to the consumer, that does the actual calling
// of the method.
template <typename R, typename Params>
class MutantRunner {
 public:
  virtual R RunWithParams(const Params& params) = 0;
  virtual ~MutantRunner() {}
};

// MutantImpl holds pre-bound arguments (like Task) and like Callback
// allows call-time arguments.
template <typename R, typename T, typename Method,
                      typename PreBound, typename Params>
class MutantImpl : public CallbackStorage<T, Method>,
                   public MutantRunner<R, Params> {
 public:
  MutantImpl(T* obj, Method meth, const PreBound& pb)
    : CallbackStorage<T, Method>(obj, meth),
    pb_(pb) {
  }

  // MutantRunner implementation
  virtual R RunWithParams(const Params& params) {
    return DispatchToMethod<R>(this->obj_, this->meth_, pb_, params);
  }

  PreBound pb_;
};

// Simple MutantRunner<> wrapper acting as a functor.
// Redirects operator() to MutantRunner<Params>::Run()
template <typename R, typename Params>
struct MutantFunctor {
  explicit MutantFunctor(MutantRunner<R, Params>*  cb) : impl_(cb) {
  }

  ~MutantFunctor() {
  }

  inline R operator()() {
    return impl_->RunWithParams(Tuple0());
  }

  template <typename Arg1>
  inline R operator()(const Arg1& a) {
    return impl_->RunWithParams(Params(a));
  }

  template <typename Arg1, typename Arg2>
  inline R operator()(const Arg1& a, const Arg2& b) {
    return impl_->RunWithParams(Params(a, b));
  }

  template <typename Arg1, typename Arg2, typename Arg3>
  inline R operator()(const Arg1& a, const Arg2& b, const Arg3& c) {
    return impl_->RunWithParams(Params(a, b, c));
  }

  template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
  inline R operator()(const Arg1& a, const Arg2& b, const Arg3& c,
                         const Arg4& d) {
    return impl_->RunWithParams(Params(a, b, c, d));
  }

 private:
  // We need copy constructor since MutantFunctor is copied few times
  // inside GMock machinery, hence no DISALLOW_EVIL_CONTRUCTORS
  MutantFunctor();
  linked_ptr<MutantRunner<R, Params> > impl_;
};



// 0 - 0
template <typename R, typename T>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T* obj, R (T::*method)()) {
  MutantRunner<R, Tuple0> *t = new MutantImpl<R, T,
      R (T::*)(),
      Tuple0, Tuple0>
      (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple0>(t);
}

// 0 - 1
template <typename R, typename T, typename A1>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T* obj, R (T::*method)(A1)) {
  MutantRunner<R, Tuple1<A1> > *t = new MutantImpl<R, T,
      R (T::*)(A1),
      Tuple0, Tuple1<A1> >
      (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple1<A1> >(t);
}

// 0 - 2
template <typename R, typename T, typename A1, typename A2>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T* obj, R (T::*method)(A1, A2)) {
  MutantRunner<R, Tuple2<A1, A2> > *t = new MutantImpl<R, T,
      R (T::*)(A1, A2),
      Tuple0, Tuple2<A1, A2> >
      (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}

// 0 - 3
template <typename R, typename T, typename A1, typename A2, typename A3>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T* obj, R (T::*method)(A1, A2, A3)) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t = new MutantImpl<R, T,
      R (T::*)(A1, A2, A3),
      Tuple0, Tuple3<A1, A2, A3> >
      (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}

// 0 - 4
template <typename R, typename T, typename A1, typename A2, typename A3,
          typename A4>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T* obj, R (T::*method)(A1, A2, A3, A4)) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t = new MutantImpl<R, T,
      R (T::*)(A1, A2, A3, A4),
      Tuple0, Tuple4<A1, A2, A3, A4> >
      (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}

// 1 - 0
template <typename R, typename T, typename P1, typename X1>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T* obj, R (T::*method)(X1), const P1& p1) {
  MutantRunner<R, Tuple0> *t = new MutantImpl<R, T,
      R (T::*)(X1),
      Tuple1<P1>, Tuple0>
      (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple0>(t);
}

// 1 - 1
template <typename R, typename T, typename P1, typename A1, typename X1>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T* obj, R (T::*method)(X1, A1), const P1& p1) {
  MutantRunner<R, Tuple1<A1> > *t = new MutantImpl<R, T,
      R (T::*)(X1, A1),
      Tuple1<P1>, Tuple1<A1> >
      (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple1<A1> >(t);
}

// 1 - 2
template <typename R, typename T, typename P1, typename A1, typename A2,
          typename X1>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T* obj, R (T::*method)(X1, A1, A2), const P1& p1) {
  MutantRunner<R, Tuple2<A1, A2> > *t = new MutantImpl<R, T,
      R (T::*)(X1, A1, A2),
      Tuple1<P1>, Tuple2<A1, A2> >
      (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}

// 1 - 3
template <typename R, typename T, typename P1, typename A1, typename A2,
          typename A3, typename X1>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T* obj, R (T::*method)(X1, A1, A2, A3), const P1& p1) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t = new MutantImpl<R, T,
      R (T::*)(X1, A1, A2, A3),
      Tuple1<P1>, Tuple3<A1, A2, A3> >
      (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}

// 1 - 4
template <typename R, typename T, typename P1, typename A1, typename A2,
          typename A3, typename A4, typename X1>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T* obj, R (T::*method)(X1, A1, A2, A3, A4), const P1& p1) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t = new MutantImpl<R, T,
      R (T::*)(X1, A1, A2, A3, A4),
      Tuple1<P1>, Tuple4<A1, A2, A3, A4> >
      (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}

// 2 - 0
template <typename R, typename T, typename P1, typename P2, typename X1,
          typename X2>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T* obj, R (T::*method)(X1, X2), const P1& p1, const P2& p2) {
  MutantRunner<R, Tuple0> *t = new MutantImpl<R, T,
      R (T::*)(X1, X2),
      Tuple2<P1, P2>, Tuple0>
      (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple0>(t);
}

// 2 - 1
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename X1, typename X2>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, A1), const P1& p1, const P2& p2) {
  MutantRunner<R, Tuple1<A1> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, A1),
      Tuple2<P1, P2>, Tuple1<A1> >
      (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple1<A1> >(t);
}

// 2 - 2
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename A2, typename X1, typename X2>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, A1, A2), const P1& p1,
    const P2& p2) {
  MutantRunner<R, Tuple2<A1, A2> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, A1, A2),
      Tuple2<P1, P2>, Tuple2<A1, A2> >
      (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}

// 2 - 3
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename A2, typename A3, typename X1, typename X2>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, A1, A2, A3), const P1& p1,
    const P2& p2) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, A1, A2, A3),
      Tuple2<P1, P2>, Tuple3<A1, A2, A3> >
      (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}

// 2 - 4
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename A2, typename A3, typename A4, typename X1, typename X2>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, A1, A2, A3, A4), const P1& p1,
    const P2& p2) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, A1, A2, A3, A4),
      Tuple2<P1, P2>, Tuple4<A1, A2, A3, A4> >
      (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}

// 3 - 0
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename X1, typename X2, typename X3>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3), const P1& p1, const P2& p2,
    const P3& p3) {
  MutantRunner<R, Tuple0> *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, X3),
      Tuple3<P1, P2, P3>, Tuple0>
      (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple0>(t);
}

// 3 - 1
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename X1, typename X2, typename X3>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, A1), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple1<A1> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, X3, A1),
      Tuple3<P1, P2, P3>, Tuple1<A1> >
      (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple1<A1> >(t);
}

// 3 - 2
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename A2, typename X1, typename X2, typename X3>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, A1, A2), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple2<A1, A2> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, X3, A1, A2),
      Tuple3<P1, P2, P3>, Tuple2<A1, A2> >
      (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}

// 3 - 3
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename A2, typename A3, typename X1, typename X2,
          typename X3>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, A1, A2, A3), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, X3, A1, A2, A3),
      Tuple3<P1, P2, P3>, Tuple3<A1, A2, A3> >
      (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}

// 3 - 4
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename A2, typename A3, typename A4, typename X1,
          typename X2, typename X3>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, A1, A2, A3, A4), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, X3, A1, A2, A3, A4),
      Tuple3<P1, P2, P3>, Tuple4<A1, A2, A3, A4> >
      (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}

// 4 - 0
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename X1, typename X2, typename X3, typename X4>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, X4), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple0> *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, X3, X4),
      Tuple4<P1, P2, P3, P4>, Tuple0>
      (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple0>(t);
}

// 4 - 1
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename X1, typename X2, typename X3,
          typename X4>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, X4, A1), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple1<A1> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, X3, X4, A1),
      Tuple4<P1, P2, P3, P4>, Tuple1<A1> >
      (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple1<A1> >(t);
}

// 4 - 2
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename A2, typename X1, typename X2,
          typename X3, typename X4>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, X4, A1, A2), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple2<A1, A2> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, X3, X4, A1, A2),
      Tuple4<P1, P2, P3, P4>, Tuple2<A1, A2> >
      (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}

// 4 - 3
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename A2, typename A3, typename X1,
          typename X2, typename X3, typename X4>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, X4, A1, A2, A3), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, X3, X4, A1, A2, A3),
      Tuple4<P1, P2, P3, P4>, Tuple3<A1, A2, A3> >
      (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}

// 4 - 4
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename A2, typename A3, typename A4,
          typename X1, typename X2, typename X3, typename X4>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, X4, A1, A2, A3, A4),
    const P1& p1, const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t = new MutantImpl<R, T,
      R (T::*)(X1, X2, X3, X4, A1, A2, A3, A4),
      Tuple4<P1, P2, P3, P4>, Tuple4<A1, A2, A3, A4> >
      (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}
#endif  // CHROME_FRAME_TEST_HELPER_GMOCK_H_
