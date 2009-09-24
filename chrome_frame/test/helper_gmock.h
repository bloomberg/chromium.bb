// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_HELPER_GMOCK_H_
#define CHROME_FRAME_TEST_HELPER_GMOCK_H_
// This intention of this file is to make possible gmock WithArgs<> in
// Chromium code base.
// MutantImpl is like CallbackImpl, but also has prebound arguments (like Task)
// There is also functor wrapper around it that should be used with
// testing::Invoke, for example:
// testing::WithArgs<0, 2>(
//    testing::Invoke(CBF(&mock_object, &MockObject::Something, &tmp_obj, 12)));
// This will invoke MockObject::Something(tmp_obj, 12, arg_0, arg_2)

// DispatchToMethod supporting two sets of arguments -
// prebound (P) and calltime (C)
// 1 - 1
template <class ObjT, class Method, class P1, class C1>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple1<P1>& p,
                             const Tuple1<C1>& c) {
  (obj->*method)(p.a, c.a);
}
// 2 - 1
template <class ObjT, class Method, class P1, class P2, class C1>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple2<P1, P2>& p,
                             const Tuple1<C1>& c) {
  (obj->*method)(p.a, p.b, c.a);
}
// 3 - 1
template <class ObjT, class Method, class P1, class P2, class P3, class C1>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple3<P1, P2, P3>& p,
                             const Tuple1<C1>& c) {
  (obj->*method)(p.a, p.b, p.c, c.a);
}
// 4 - 1
template <class ObjT, class Method, class P1, class P2, class P3,
          class P4, class C1>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple4<P1, P2, P3, P4>& p,
                             const Tuple1<C1>& c) {
  (obj->*method)(p.a, p.b, p.c, p.d, c.a);
}

// 1 - 2
template <class ObjT, class Method, class P1, class C1, class C2>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple1<P1>& p,
                             const Tuple2<C1, C2>& c) {
  (obj->*method)(p.a, c.a, c.b);
}

// 2 - 2
template <class ObjT, class Method, class P1, class P2, class C1, class C2>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple2<P1, P2>& p,
                             const Tuple2<C1, C2>& c) {
  (obj->*method)(p.a, p.b, c.a, c.b);
}

// 3 - 2
template <class ObjT, class Method, class P1, class P2, class P3, class C1,
          class C2>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple3<P1, P2, P3>& p,
                             const Tuple2<C1, C2>& c) {
  (obj->*method)(p.a, p.b, p.c, c.a, c.b);
}

// 4 - 2
template <class ObjT, class Method, class P1, class P2, class P3, class P4,
          class C1, class C2>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple4<P1, P2, P3, P4>& p,
                             const Tuple2<C1, C2>& c) {
  (obj->*method)(p.a, p.b, p.c, p.d, c.a, c.b);
}

// 1 - 3
template <class ObjT, class Method, class P1, class C1, class C2, class C3>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple1<P1>& p,
                             const Tuple3<C1, C2, C3>& c) {
  (obj->*method)(p.a, c.a, c.b, c.c);
}

// 2 - 3
template <class ObjT, class Method, class P1, class P2, class C1, class C2,
          class C3>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple2<P1, P2>& p,
                             const Tuple3<C1, C2, C3>& c) {
  (obj->*method)(p.a, p.b, c.a, c.b, c.c);
}

// 3 - 3
template <class ObjT, class Method, class P1, class P2, class P3, class C1,
          class C2, class C3>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple3<P1, P2, P3>& p,
                             const Tuple3<C1, C2, C3>& c) {
  (obj->*method)(p.a, p.b, p.c, c.a, c.b, c.c);
}

// 4 - 3
template <class ObjT, class Method, class P1, class P2, class P3, class P4,
          class C1, class C2, class C3>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple4<P1, P2, P3, P4>& p,
                             const Tuple3<C1, C2, C3>& c) {
  (obj->*method)(p.a, p.b, p.c, p.d, c.a, c.b, c.c);
}

// 1 - 4
template <class ObjT, class Method, class P1, class C1, class C2, class C3,
          class C4>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple1<P1>& p,
                             const Tuple4<C1, C2, C3, C4>& c) {
  (obj->*method)(p.a, c.a, c.b, c.c, c.d);
}

// 2 - 4
template <class ObjT, class Method, class P1, class P2, class C1, class C2,
          class C3, class C4>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple2<P1, P2>& p,
                             const Tuple4<C1, C2, C3, C4>& c) {
  (obj->*method)(p.a, p.b, c.a, c.b, c.c, c.d);
}

// 3 - 4
template <class ObjT, class Method, class P1, class P2, class P3,
          class C1, class C2, class C3, class C4>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple3<P1, P2, P3>& p,
                             const Tuple4<C1, C2, C3, C4>& c) {
  (obj->*method)(p.a, p.b, p.c, c.a, c.b, c.c, c.d);
}

// 4 - 4
template <class ObjT, class Method, class P1, class P2, class P3, class P4,
          class C1, class C2, class C3, class C4>
inline void DispatchToMethod(ObjT* obj, Method method,
                             const Tuple4<P1, P2, P3, P4>& p,
                             const Tuple4<C1, C2, C3, C4>& c) {
  (obj->*method)(p.a, p.b, p.c, p.d, c.a, c.b, c.c, c.d);
}


////////////////////////////////////////////////////////////////////////////////

// Like CallbackImpl but has prebound arguments (like Task)
template <class T, typename Method, typename PreBound, typename Params>
class MutantImpl : public CallbackStorage<T, Method>,
  public CallbackRunner<Params> {
 public:
  MutantImpl(T* obj, Method meth, const PreBound& pb)
    : CallbackStorage<T, Method>(obj, meth),
      pb_(pb) {
  }

  virtual void RunWithParams(const Params& params) {
    // use "this->" to force C++ to look inside our templatized base class; see
    // Effective C++, 3rd Ed, item 43, p210 for details.
    DispatchToMethod(this->obj_, this->meth_, pb_, params);
  }

  PreBound pb_;
};

////////////////////////////////////////////////////////////////////////////////
// Mutant creation simplification
// 1 - 1
template <class T, typename P1, typename A1>
inline typename Callback1<A1>::Type* NewMutant(T* obj,
                                               void (T::*method)(P1, A1),
                                               P1 p1) {
  return new MutantImpl<T, void (T::*)(P1, A1), P1, A1>(obj, method,
      MakeTuple(p1));
}

// 1 - 2
template <class T, typename P1, typename A1, typename A2>
inline typename Callback2<A1, A2>::Type* NewMutant(T* obj,
                                                  void (T::*method)(P1, A1, A2),
                                                  P1 p1) {
  return new MutantImpl<T, void (T::*)(P1, A1, A2), Tuple1<P1>, Tuple2<A1, A2> >
      (obj, method, MakeTuple(p1));
}

// 1 - 3
template <class T, typename P1, typename A1, typename A2, typename A3>
inline typename Callback3<A1, A2, A3>::Type*
NewMutant(T* obj, void (T::*method)(P1, A1, A2, A3), P1 p1) {
  return new MutantImpl<T, void (T::*)(P1, A1, A2, A3), Tuple1<P1>,
      Tuple3<A1, A2, A3> >(obj, method, MakeTuple(p1));
}

// 1 - 4
template <class T, typename P1, typename A1, typename A2, typename A3,
          typename A4>
inline typename Callback4<A1, A2, A3, A4>::Type*
NewMutant(T* obj, void (T::*method)(P1, A1, A2, A3, A4), P1 p1) {
  return new MutantImpl<T, void (T::*)(P1, A1, A2, A3, A4), Tuple1<P1>,
      Tuple4<A1, A2, A3, A4> >(obj, method, MakeTuple(p1));
}


// 2 - 1
template <class T, typename P1, typename P2, typename A1>
inline typename Callback1<A1>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, A1), P1 p1, P2 p2) {
  return new MutantImpl<T, void (T::*)(P1, P2, A1), Tuple2<P1, P2>,
      Tuple1<A1> >(obj, method, MakeTuple(p1, p2));
}

// 2 - 2
template <class T, typename P1, typename P2, typename A1, typename A2>
inline typename Callback2<A1, A2>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, A1, A2), P1 p1, P2 p2) {
  return new MutantImpl<T, void (T::*)(P1, P2, A1, A2), Tuple2<P1, P2>,
      Tuple2<A1, A2> >(obj, method, MakeTuple(p1, p2));
}

// 2 - 3
template <class T, typename P1, typename P2, typename A1, typename A2,
          typename A3>
inline typename Callback3<A1, A2, A3>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, A1, A2, A3), P1 p1, P2 p2) {
  return new MutantImpl<T, void (T::*)(P1, P2, A1, A2, A3), Tuple2<P1, P2>,
      Tuple3<A1, A2, A3> >(obj, method, MakeTuple(p1, p2));
}

// 2 - 4
template <class T, typename P1, typename P2, typename A1, typename A2,
          typename A3, typename A4>
inline typename Callback4<A1, A2, A3, A4>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, A1, A2, A3, A4), P1 p1, P2 p2) {
  return new MutantImpl<T, void (T::*)(P1, P2, A1, A2, A3, A4), Tuple2<P1, P2>,
      Tuple3<A1, A2, A3, A4> >(obj, method, MakeTuple(p1, p2));
}

// 3 - 1
template <class T, typename P1, typename P2, typename P3, typename A1>
inline typename Callback1<A1>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, P3, A1), P1 p1, P2 p2, P3 p3) {
  return new MutantImpl<T, void (T::*)(P1, P2, P3, A1), Tuple3<P1, P2, P3>,
      Tuple1<A1> >(obj, method, MakeTuple(p1, p2, p3));
}

// 3 - 2
template <class T, typename P1, typename P2, typename P3, typename A1,
          typename A2>
inline typename Callback2<A1, A2>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, P3, A1, A2), P1 p1, P2 p2, P3 p3) {
  return new MutantImpl<T, void (T::*)(P1, P2, P3, A1, A2), Tuple3<P1, P2, P3>,
      Tuple2<A1, A2> >(obj, method, MakeTuple(p1, p2, p3));
}

// 3 - 3
template <class T, typename P1, typename P2, typename P3, typename A1,
          typename A2, typename A3>
inline typename Callback3<A1, A2, A3>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, P3, A1, A2, A3), P1 p1, P2 p2,
          P3 p3) {
  return new MutantImpl<T, void (T::*)(P1, P2, P3, A1, A2, A3),
      Tuple3<P1, P2, P3>, Tuple3<A1, A2, A3> >(obj, method,
      MakeTuple(p1, p2, p3));
}

// 3 - 4
template <class T, typename P1, typename P2, typename P3, typename A1,
          typename A2, typename A3, typename A4>
inline typename Callback4<A1, A2, A3, A4>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, P3, A1, A2, A3, A4), P1 p1, P2 p2,
          P3 p3) {
  return new MutantImpl<T, void (T::*)(P1, P2, P3, A1, A2, A3, A4),
      Tuple3<P1, P2, P3>, Tuple3<A1, A2, A3, A4> >(obj, method,
      MakeTuple(p1, p2, p3));
}

// 4 - 1
template <class T, typename P1, typename P2, typename P3, typename P4,
          typename A1>
inline typename Callback1<A1>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, P3, P4, A1), P1 p1, P2 p2, P3 p3,
          P4 p4) {
  return new MutantImpl<T, void (T::*)(P1, P2, P3, P4, A1),
      Tuple4<P1, P2, P3, P4>, Tuple1<A1> >(obj, method,
      MakeTuple(p1, p2, p3, p4));
}

// 4 - 2
template <class T, typename P1, typename P2, typename P3, typename P4,
          typename A1, typename A2>
inline typename Callback2<A1, A2>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, P3, P4, A1, A2), P1 p1, P2 p2,
          P3 p3, P4 p4) {
  return new MutantImpl<T, void (T::*)(P1, P2, P3, P4, A1, A2),
    Tuple4<P1, P2, P3, P4>, Tuple2<A1, A2> >(obj, method,
    MakeTuple(p1, p2, p3, p4));
}

// 4 - 3
template <class T, typename P1, typename P2, typename P3, typename P4,
          typename A1, typename A2, typename A3>
inline typename Callback3<A1, A2, A3>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, P3, P4, A1, A2, A3), P1 p1, P2 p2,
          P3 p3, P4 p4) {
  return new MutantImpl<T, void (T::*)(P1, P2, P3, P4, A1, A2, A3),
    Tuple4<P1, P2, P3, P4>, Tuple3<A1, A2, A3> >(obj, method,
    MakeTuple(p1, p2, p3, p4));
}

// 4 - 4
template <class T, typename P1, typename P2, typename P3, typename P4,
          typename A1, typename A2, typename A3, typename A4>
inline typename Callback4<A1, A2, A3, A4>::Type*
NewMutant(T* obj, void (T::*method)(P1, P2, P3, P4, A1, A2, A3, A4),
          P1 p1, P2 p2, P3 p3, P4 p4) {
  return new MutantImpl<T, void (T::*)(P1, P2, P3, P4, A1, A2, A3, A4),
      Tuple4<P1, P2, P3, P4>, Tuple3<A1, A2, A3, A4> >(obj, method,
      MakeTuple(p1, p2, p3, p4));
}

////////////////////////////////////////////////////////////////////////////////
// Simple callback wrapper acting as a functor.
// Redirects operator() to CallbackRunner<Params>::Run
// We cannot delete the inner impl_ in object's destructor because
// this object is copied few times inside from GMock machinery.
template <typename Params>
struct CallbackFunctor {
  explicit CallbackFunctor(CallbackRunner<Params>*  cb) : impl_(cb) {}

  template <typename Arg1>
  inline void operator()(const Arg1& a) {
    impl_->Run(a);
    delete impl_;
    impl_ = NULL;
  }

  template <typename Arg1, typename Arg2>
  inline void operator()(const Arg1& a, const Arg2& b) {
    impl_->Run(a, b);
    delete impl_;
    impl_ = NULL;
  }

  template <typename Arg1, typename Arg2, typename Arg3>
  inline void operator()(const Arg1& a, const Arg2& b, const Arg3& c) {
    impl_->Run(a, b, c);
    delete impl_;
    impl_ = NULL;
  }

  template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
  inline void operator()(const Arg1& a, const Arg2& b, const Arg3& c,
                         const Arg4& d) {
    impl_->Run(a, b, c, d);
    delete impl_;
    impl_ = NULL;
  }

 private:
  CallbackFunctor();
  CallbackRunner<Params>* impl_;
};


///////////////////////////////////////////////////////////////////////////////
// CallbackFunctors creation

// 0 - 1
template <class T, typename A1>
inline CallbackFunctor<Tuple1<A1> >
CBF(T* obj, void (T::*method)(A1)) {
  return CallbackFunctor<Tuple1<A1> >(NewCallback(obj, method));
}

// 0 - 2
template <class T, typename A1, typename A2>
inline CallbackFunctor<Tuple2<A1, A2> >
CBF(T* obj, void (T::*method)(A1, A2)) {
  return CallbackFunctor<Tuple2<A1, A2> >(NewCallback(obj, method));
}

// 0 - 3
template <class T, typename A1, typename A2, typename A3>
inline CallbackFunctor<Tuple3<A1, A2, A3> >
CBF(T* obj, void (T::*method)(A1, A2, A3)) {
  return CallbackFunctor<Tuple3<A1, A2, A3> >(NewCallback(obj, method));
}

// 0 - 4
template <class T, typename A1, typename A2, typename A3, typename A4>
inline CallbackFunctor<Tuple4<A1, A2, A3, A4> >
CBF(T* obj, void (T::*method)(A1, A2, A3, A4)) {
  return CallbackFunctor<Tuple4<A1, A2, A3, A4> >(NewCallback(obj, method));
}

// 1 - 1
template <class T, typename P1, typename A1>
inline CallbackFunctor<Tuple1<A1> >
CBF(T* obj, void (T::*method)(P1, A1), P1 p1) {
  Callback1<A1>::Type* t = new MutantImpl<T, void (T::*)(P1, A1), Tuple1<P1>,
      Tuple1<A1> >(obj, method, MakeTuple(p1));
  return CallbackFunctor<Tuple1<A1> >(t);
}

// 1 - 2
template <class T, typename P1, typename A1, typename A2>
inline CallbackFunctor<Tuple2<A1, A2> >
CBF(T* obj, void (T::*method)(P1, A1, A2), P1 p1) {
  Callback2<A1, A2>::Type* t = new MutantImpl<T, void (T::*)(P1, A1, A2),
      Tuple1<P1>, Tuple2<A1, A2> >(obj, method, MakeTuple(p1));
  return CallbackFunctor<Tuple2<A1, A2> >(t);
}

// 1 - 3
template <class T, typename P1, typename A1, typename A2, typename A3>
inline CallbackFunctor<Tuple3<A1, A2, A3> >
CBF(T* obj, void (T::*method)(P1, A1, A2, A3), P1 p1) {
  Callback3<A1, A2, A3>::Type* t =
      new MutantImpl<T, void (T::*)(P1, A1, A2, A3), Tuple1<P1>,
      Tuple3<A1, A2, A3> >(obj, method, MakeTuple(p1));
  return CallbackFunctor<Tuple3<A1, A2, A3> >(t);
}

// 1 - 4
template <class T, typename P1, typename A1, typename A2, typename A3,
          typename A4>
inline CallbackFunctor<Tuple4<A1, A2, A3, A4> >
CBF(T* obj, void (T::*method)(P1, A1, A2, A3, A4), P1 p1) {
  Callback4<A1, A2, A3>::Type* t =
      new MutantImpl<T, void (T::*)(P1, A1, A2, A3, A4), Tuple1<P1>,
      Tuple4<A1, A2, A3, A4> >(obj, method, MakeTuple(p1));
  return CallbackFunctor<Tuple4<A1, A2, A3, A4> >(t);
}

// 2 - 1
template <class T, typename P1, typename P2, typename A1>
inline CallbackFunctor<Tuple1<A1> >
CBF(T* obj, void (T::*method)(P1, P2, A1), P1 p1, P2 p2) {
  Callback1<A1>::Type* t = new MutantImpl<T, void (T::*)(P1, P2, A1),
      Tuple2<P1, P2>, Tuple1<A1> >(obj, method, MakeTuple(p1, p2));
  return CallbackFunctor<Tuple1<A1> >(t);
}

// 2 - 2
template <class T, typename P1, typename P2, typename A1, typename A2>
inline CallbackFunctor<Tuple2<A1, A2> >
CBF(T* obj, void (T::*method)(P1, P2, A1, A2), P1 p1, P2 p2) {
  Callback2<A1, A2>::Type* t = new MutantImpl<T, void (T::*)(P1, P2, A1, A2),
      Tuple2<P1, P2>, Tuple2<A1, A2> >(obj, method, MakeTuple(p1, p2));
  return CallbackFunctor<Tuple2<A1, A2> >(t);
}

// 2 - 3
template <class T, typename P1, typename P2, typename A1, typename A2,
          typename A3> inline CallbackFunctor<Tuple3<A1, A2, A3> >
CBF(T* obj, void (T::*method)(P1, P2, A1, A2, A3), P1 p1, P2 p2) {
  Callback3<A1, A2, A3>::Type* t =
      new MutantImpl<T, void (T::*)(P1, P2, A1, A2, A3), Tuple2<P1, P2>,
      Tuple3<A1, A2, A3> >(obj, method, MakeTuple(p1, p2));
  return CallbackFunctor<Tuple3<A1, A2, A3> >(t);
}

// 2 - 4
template <class T, typename P1, typename P2, typename A1, typename A2,
          typename A3, typename A4>
inline CallbackFunctor<Tuple4<A1, A2, A3, A4> >
CBF(T* obj, void (T::*method)(P1, P2, A1, A2, A3, A4), P1 p1, P2 p2) {
  Callback4<A1, A2, A3>::Type* t =
      new MutantImpl<T, void (T::*)(P1, P2, A1, A2, A3, A4), Tuple2<P1, P2>,
      Tuple4<A1, A2, A3, A4> >(obj, method, MakeTuple(p1, p2));
  return CallbackFunctor<Tuple4<A1, A2, A3, A4> >(t);
}


// 3 - 1
template <class T, typename P1, typename P2, typename P3, typename A1>
inline CallbackFunctor<Tuple1<A1> >
CBF(T* obj, void (T::*method)(P1, P2, P3, A1), P1 p1, P2 p2, P3 p3) {
  Callback1<A1>::Type* t = new MutantImpl<T, void (T::*)(P1, P2, P3, A1),
      Tuple3<P1, P2, P3>, Tuple1<A1> >(obj, method, MakeTuple(p1, p2, p3));
  return CallbackFunctor<Tuple1<A1> >(t);
}


// 3 - 2
template <class T, typename P1, typename P2, typename P3, typename A1,
          typename A2> inline CallbackFunctor<Tuple2<A1, A2> >
CBF(T* obj, void (T::*method)(P1, P2, P3, A1, A2), P1 p1, P2 p2, P3 p3) {
  Callback2<A1, A2>::Type* t =
      new MutantImpl<T, void (T::*)(P1, P2, P3, A1, A2), Tuple3<P1, P2, P3>,
      Tuple2<A1, A2> >(obj, method, MakeTuple(p1, p2, p3));
  return CallbackFunctor<Tuple2<A1, A2> >(t);
}

// 3 - 3
template <class T, typename P1, typename P2, typename P3, typename A1,
          typename A2, typename A3>
inline CallbackFunctor<Tuple3<A1, A2, A3> >
CBF(T* obj, void (T::*method)(P1, P2, P3, A1, A2, A3), P1 p1, P2 p2, P3 p3) {
  Callback3<A1, A2, A3>::Type* t =
      new MutantImpl<T, void (T::*)(P1, P2, P3, A1, A2, A3), Tuple3<P1, P2, P3>,
      Tuple3<A1, A2, A3> >(obj, method, MakeTuple(p1, p2, p3));
  return CallbackFunctor<Tuple3<A1, A2, A3> >(t);
}

// 3 - 4
template <class T, typename P1, typename P2, typename P3, typename A1,
          typename A2, typename A3, typename A4>
inline CallbackFunctor<Tuple4<A1, A2, A3, A4> >
CBF(T* obj, void (T::*method)(P1, P2, P3, A1, A2, A3, A4),
    P1 p1, P2 p2, P3 p3) {
  Callback4<A1, A2, A3>::Type* t =
      new MutantImpl<T, void (T::*)(P1, P2, P3, A1, A2, A3, A4),
      Tuple3<P1, P2, P3>, Tuple4<A1, A2, A3, A4> >(obj, method,
      MakeTuple(p1, p2, p3));
  return CallbackFunctor<Tuple4<A1, A2, A3, A4> >(t);
}



// 4 - 1
template <class T, typename P1, typename P2, typename P3, typename P4,
          typename A1>
inline CallbackFunctor<Tuple1<A1> >
CBF(T* obj, void (T::*method)(P1, P2, P3, P4, A1), P1 p1, P2 p2, P3 p3, P4 p4) {
  Callback1<A1>::Type* t = new MutantImpl<T, void (T::*)(P1, P2, P3, P4, A1),
      Tuple4<P1, P2, P3, P4>, Tuple1<A1> >
      (obj, method, MakeTuple(p1, p2, p3, p4));
  return CallbackFunctor<Tuple1<A1> >(t);
}


// 4 - 2
template <class T, typename P1, typename P2, typename P3, typename P4,
          typename A1, typename A2>
inline CallbackFunctor<Tuple2<A1, A2> >
CBF(T* obj, void (T::*method)(P1, P2, P3, P4, A1, A2),
            P1 p1, P2 p2, P3 p3, P4 p4) {
  Callback2<A1, A2>::Type* t =
      new MutantImpl<T, void (T::*)(P1, P2, P3, P4, A1, A2),
      Tuple4<P1, P2, P3, P4>,
      Tuple2<A1, A2> >(obj, method, MakeTuple(p1, p2, p3, p4));
  return CallbackFunctor<Tuple2<A1, A2> >(t);
}

// 4 - 3
template <class T, typename P1, typename P2, typename P3, typename P4,
          typename A1, typename A2, typename A3>
inline CallbackFunctor<Tuple3<A1, A2, A3> >
CBF(T* obj, void (T::*method)(P1, P2, P3, P4, A1, A2, A3),
    P1 p1, P2 p2, P3 p3, P4 p4) {
  Callback3<A1, A2, A3>::Type* t =
      new MutantImpl<T, void (T::*)(P1, P2, P3, P4, A1, A2, A3),
      Tuple4<P1, P2, P3, P4>, Tuple3<A1, A2, A3> >
      (obj, method, MakeTuple(p1, p2, p3, p4));
  return CallbackFunctor<Tuple3<A1, A2, A3> >(t);
}

// 4 - 4
template <class T, typename P1, typename P2, typename P3, typename P4,
          typename A1, typename A2, typename A3, typename A4>
inline CallbackFunctor<Tuple4<A1, A2, A3, A4> >
CBF(T* obj, void (T::*method)(P1, P2, P3, P4, A1, A2, A3, A4),
    P1 p1, P2 p2, P3 p3, P4 p4) {
  Callback4<A1, A2, A3>::Type* t =
      new MutantImpl<T, void (T::*)(P1, P2, P3, P4, A1, A2, A3, A4),
      Tuple4<P1, P2, P3, P4>, Tuple4<A1, A2, A3, A4> >(obj, method,
      MakeTuple(p1, p2, p3, p4));
  return CallbackFunctor<Tuple4<A1, A2, A3, A4> >(t);
}


// Simple task wrapper acting as a functor.
// Redirects operator() to Task::Run. We cannot delete the inner impl_ object
// in object's destructor because this object is copied few times inside
// from GMock machinery.
struct TaskHolder {
  explicit TaskHolder(Task* impl) : impl_(impl) {}
  void operator()() {
    impl_->Run();
    delete impl_;
    impl_ = NULL;
  }
 private:
  TaskHolder();
  Task* impl_;
};

#endif  // CHROME_FRAME_TEST_HELPER_GMOCK_H_
