// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Tuple is a generic templatized container, similar in concept to std::pair.
// There are classes Tuple0 to Tuple6, cooresponding to the number of elements
// it contains.  The convenient MakeTuple() function takes 0 to 6 arguments,
// and will construct and return the appropriate Tuple object.  The functions
// DispatchToMethod and DispatchToFunction take a function pointer or instance
// and method pointer, and unpack a tuple into arguments to the call.
//
// Tuple elements are copied by value, and stored in the tuple.  See the unit
// tests for more details of how/when the values are copied.
//
// Example usage:
//   // These two methods of creating a Tuple are identical.
//   Tuple2<int, const char*> tuple_a(1, "wee");
//   Tuple2<int, const char*> tuple_b = MakeTuple(1, "wee");
//
//   void SomeFunc(int a, const char* b) { }
//   DispatchToFunction(&SomeFunc, tuple_a);  // SomeFunc(1, "wee")
//   DispatchToFunction(
//       &SomeFunc, MakeTuple(10, "foo"));    // SomeFunc(10, "foo")
//
//   struct { void SomeMeth(int a, int b, int c) { } } foo;
//   DispatchToMethod(&foo, &Foo::SomeMeth, MakeTuple(1, 2, 3));
//   // foo->SomeMeth(1, 2, 3);

#ifndef BASE_TUPLE_H__
#define BASE_TUPLE_H__

#include "base/bind_helpers.h"

// Index sequences
//
// Minimal clone of the similarly-named C++14 functionality.

template <size_t...>
struct IndexSequence {};

template <size_t... Ns>
struct MakeIndexSequenceImpl;

template <size_t... Ns>
struct MakeIndexSequenceImpl<0, Ns...> {
  using Type = IndexSequence<Ns...>;
};

template <size_t N, size_t... Ns>
struct MakeIndexSequenceImpl<N, Ns...> {
  using Type = typename MakeIndexSequenceImpl<N - 1, N - 1, Ns...>::Type;
};

template <size_t N>
using MakeIndexSequence = typename MakeIndexSequenceImpl<N>::Type;

// Traits ----------------------------------------------------------------------
//
// A simple traits class for tuple arguments.
//
// ValueType: the bare, nonref version of a type (same as the type for nonrefs).
// RefType: the ref version of a type (same as the type for refs).
// ParamType: what type to pass to functions (refs should not be constified).

template <class P>
struct TupleTraits {
  typedef P ValueType;
  typedef P& RefType;
  typedef const P& ParamType;
};

template <class P>
struct TupleTraits<P&> {
  typedef P ValueType;
  typedef P& RefType;
  typedef P& ParamType;
};

// Tuple -----------------------------------------------------------------------
//
// This set of classes is useful for bundling 0 or more heterogeneous data types
// into a single variable.  The advantage of this is that it greatly simplifies
// function objects that need to take an arbitrary number of parameters; see
// RunnableMethod and IPC::MessageWithTuple.
//
// Tuple0 is supplied to act as a 'void' type.  It can be used, for example,
// when dispatching to a function that accepts no arguments (see the
// Dispatchers below).
// Tuple1<A> is rarely useful.  One such use is when A is non-const ref that you
// want filled by the dispatchee, and the tuple is merely a container for that
// output (a "tier").  See MakeRefTuple and its usages.

template <typename... Ts>
struct Tuple;

template <>
struct Tuple<> {};

template <typename A>
struct Tuple<A> {
 public:
  typedef A TypeA;

  Tuple() {}
  explicit Tuple(typename TupleTraits<A>::ParamType a) : a(a) {}

  A a;
};

template <typename A, typename B>
struct Tuple<A, B> {
 public:
  typedef A TypeA;
  typedef B TypeB;

  Tuple() {}
  Tuple(typename TupleTraits<A>::ParamType a,
        typename TupleTraits<B>::ParamType b)
      : a(a), b(b) {}

  A a;
  B b;
};

template <typename A, typename B, typename C>
struct Tuple<A, B, C> {
 public:
  typedef A TypeA;
  typedef B TypeB;
  typedef C TypeC;

  Tuple() {}
  Tuple(typename TupleTraits<A>::ParamType a,
        typename TupleTraits<B>::ParamType b,
        typename TupleTraits<C>::ParamType c)
      : a(a), b(b), c(c) {}

  A a;
  B b;
  C c;
};

template <typename A, typename B, typename C, typename D>
struct Tuple<A, B, C, D> {
 public:
  typedef A TypeA;
  typedef B TypeB;
  typedef C TypeC;
  typedef D TypeD;

  Tuple() {}
  Tuple(typename TupleTraits<A>::ParamType a,
        typename TupleTraits<B>::ParamType b,
        typename TupleTraits<C>::ParamType c,
        typename TupleTraits<D>::ParamType d)
      : a(a), b(b), c(c), d(d) {}

  A a;
  B b;
  C c;
  D d;
};

template <typename A, typename B, typename C, typename D, typename E>
struct Tuple<A, B, C, D, E> {
 public:
  typedef A TypeA;
  typedef B TypeB;
  typedef C TypeC;
  typedef D TypeD;
  typedef E TypeE;

  Tuple() {}
  Tuple(typename TupleTraits<A>::ParamType a,
        typename TupleTraits<B>::ParamType b,
        typename TupleTraits<C>::ParamType c,
        typename TupleTraits<D>::ParamType d,
        typename TupleTraits<E>::ParamType e)
      : a(a), b(b), c(c), d(d), e(e) {}

  A a;
  B b;
  C c;
  D d;
  E e;
};

template <typename A,
          typename B,
          typename C,
          typename D,
          typename E,
          typename F>
struct Tuple<A, B, C, D, E, F> {
 public:
  typedef A TypeA;
  typedef B TypeB;
  typedef C TypeC;
  typedef D TypeD;
  typedef E TypeE;
  typedef F TypeF;

  Tuple() {}
  Tuple(typename TupleTraits<A>::ParamType a,
        typename TupleTraits<B>::ParamType b,
        typename TupleTraits<C>::ParamType c,
        typename TupleTraits<D>::ParamType d,
        typename TupleTraits<E>::ParamType e,
        typename TupleTraits<F>::ParamType f)
      : a(a), b(b), c(c), d(d), e(e), f(f) {}

  A a;
  B b;
  C c;
  D d;
  E e;
  F f;
};

template <typename A,
          typename B,
          typename C,
          typename D,
          typename E,
          typename F,
          typename G>
struct Tuple<A, B, C, D, E, F, G> {
 public:
  typedef A TypeA;
  typedef B TypeB;
  typedef C TypeC;
  typedef D TypeD;
  typedef E TypeE;
  typedef F TypeF;
  typedef G TypeG;

  Tuple() {}
  Tuple(typename TupleTraits<A>::ParamType a,
        typename TupleTraits<B>::ParamType b,
        typename TupleTraits<C>::ParamType c,
        typename TupleTraits<D>::ParamType d,
        typename TupleTraits<E>::ParamType e,
        typename TupleTraits<F>::ParamType f,
        typename TupleTraits<G>::ParamType g)
      : a(a), b(b), c(c), d(d), e(e), f(f), g(g) {}

  A a;
  B b;
  C c;
  D d;
  E e;
  F f;
  G g;
};

template <typename A,
          typename B,
          typename C,
          typename D,
          typename E,
          typename F,
          typename G,
          typename H>
struct Tuple<A, B, C, D, E, F, G, H> {
 public:
  typedef A TypeA;
  typedef B TypeB;
  typedef C TypeC;
  typedef D TypeD;
  typedef E TypeE;
  typedef F TypeF;
  typedef G TypeG;
  typedef H TypeH;

  Tuple() {}
  Tuple(typename TupleTraits<A>::ParamType a,
        typename TupleTraits<B>::ParamType b,
        typename TupleTraits<C>::ParamType c,
        typename TupleTraits<D>::ParamType d,
        typename TupleTraits<E>::ParamType e,
        typename TupleTraits<F>::ParamType f,
        typename TupleTraits<G>::ParamType g,
        typename TupleTraits<H>::ParamType h)
      : a(a), b(b), c(c), d(d), e(e), f(f), g(g), h(h) {}

  A a;
  B b;
  C c;
  D d;
  E e;
  F f;
  G g;
  H h;
};

// Deprecated compat aliases

using Tuple0 = Tuple<>;
template <typename A>
using Tuple1 = Tuple<A>;
template <typename A, typename B>
using Tuple2 = Tuple<A, B>;
template <typename A, typename B, typename C>
using Tuple3 = Tuple<A, B, C>;
template <typename A, typename B, typename C, typename D>
using Tuple4 = Tuple<A, B, C, D>;
template <typename A, typename B, typename C, typename D, typename E>
using Tuple5 = Tuple<A, B, C, D, E>;
template <typename A,
          typename B,
          typename C,
          typename D,
          typename E,
          typename F>
using Tuple6 = Tuple<A, B, C, D, E, F>;
template <typename A,
          typename B,
          typename C,
          typename D,
          typename E,
          typename F,
          typename G>
using Tuple7 = Tuple<A, B, C, D, E, F, G>;
template <typename A,
          typename B,
          typename C,
          typename D,
          typename E,
          typename F,
          typename G,
          typename H>
using Tuple8 = Tuple<A, B, C, D, E, F, G, H>;

// Tuple element --------------------------------------------------------------

template <size_t N, typename T>
struct TupleElement;

template <typename T, typename... Ts>
struct TupleElement<0, Tuple<T, Ts...>> {
  using Type = T;
};

template <size_t N, typename T, typename... Ts>
struct TupleElement<N, Tuple<T, Ts...>> {
  using Type = typename TupleElement<N - 1, Tuple<Ts...>>::Type;
};

// Tuple getters --------------------------------------------------------------

template <size_t, typename T>
struct TupleGetter;

template <typename... Ts>
struct TupleGetter<0, Tuple<Ts...>> {
  using Elem = typename TupleElement<0, Tuple<Ts...>>::Type;
  static Elem& Get(Tuple<Ts...>& t) { return t.a; }
  static const Elem& Get(const Tuple<Ts...>& t) { return t.a; }
};

template <typename... Ts>
struct TupleGetter<1, Tuple<Ts...>> {
  using Elem = typename TupleElement<1, Tuple<Ts...>>::Type;
  static Elem& Get(Tuple<Ts...>& t) { return t.b; }
  static const Elem& Get(const Tuple<Ts...>& t) { return t.b; }
};

template <typename... Ts>
struct TupleGetter<2, Tuple<Ts...>> {
  using Elem = typename TupleElement<2, Tuple<Ts...>>::Type;
  static Elem& Get(Tuple<Ts...>& t) { return t.c; }
  static const Elem& Get(const Tuple<Ts...>& t) { return t.c; }
};

template <typename... Ts>
struct TupleGetter<3, Tuple<Ts...>> {
  using Elem = typename TupleElement<3, Tuple<Ts...>>::Type;
  static Elem& Get(Tuple<Ts...>& t) { return t.d; }
  static const Elem& Get(const Tuple<Ts...>& t) { return t.d; }
};

template <typename... Ts>
struct TupleGetter<4, Tuple<Ts...>> {
  using Elem = typename TupleElement<4, Tuple<Ts...>>::Type;
  static Elem& Get(Tuple<Ts...>& t) { return t.e; }
  static const Elem& Get(const Tuple<Ts...>& t) { return t.e; }
};

template <typename... Ts>
struct TupleGetter<5, Tuple<Ts...>> {
  using Elem = typename TupleElement<5, Tuple<Ts...>>::Type;
  static Elem& Get(Tuple<Ts...>& t) { return t.f; }
  static const Elem& Get(const Tuple<Ts...>& t) { return t.f; }
};

template <typename... Ts>
struct TupleGetter<6, Tuple<Ts...>> {
  using Elem = typename TupleElement<6, Tuple<Ts...>>::Type;
  static Elem& Get(Tuple<Ts...>& t) { return t.g; }
  static const Elem& Get(const Tuple<Ts...>& t) { return t.g; }
};

template <typename... Ts>
struct TupleGetter<7, Tuple<Ts...>> {
  using Elem = typename TupleElement<7, Tuple<Ts...>>::Type;
  static Elem& Get(Tuple<Ts...>& t) { return t.h; }
  static const Elem& Get(const Tuple<Ts...>& t) { return t.h; }
};

template <size_t I, typename... Ts>
typename TupleElement<I, Tuple<Ts...>>::Type& get(Tuple<Ts...>& tuple) {
  return TupleGetter<I, Tuple<Ts...>>::Get(tuple);
}

template <size_t I, typename... Ts>
const typename TupleElement<I, Tuple<Ts...>>::Type& get(
    const Tuple<Ts...>& tuple) {
  return TupleGetter<I, Tuple<Ts...>>::Get(tuple);
}

// Tuple types ----------------------------------------------------------------
//
// Allows for selection of ValueTuple/RefTuple/ParamTuple without needing the
// definitions of class types the tuple takes as parameters.

template <typename T>
struct TupleTypes;

template <typename... Ts>
struct TupleTypes<Tuple<Ts...>> {
  using ValueTuple = Tuple<typename TupleTraits<Ts>::ValueType...>;
  using RefTuple = Tuple<typename TupleTraits<Ts>::RefType...>;
  using ParamTuple = Tuple<typename TupleTraits<Ts>::ParamType...>;
};

// Tuple creators -------------------------------------------------------------
//
// Helper functions for constructing tuples while inferring the template
// argument types.

template <typename... Ts>
inline Tuple<Ts...> MakeTuple(const Ts&... arg) {
  return Tuple<Ts...>(arg...);
}

// The following set of helpers make what Boost refers to as "Tiers" - a tuple
// of references.

template <typename... Ts>
inline Tuple<Ts&...> MakeRefTuple(Ts&... arg) {
  return Tuple<Ts&...>(arg...);
}

// Dispatchers ----------------------------------------------------------------
//
// Helper functions that call the given method on an object, with the unpacked
// tuple arguments.  Notice that they all have the same number of arguments,
// so you need only write:
//   DispatchToMethod(object, &Object::method, args);
// This is very useful for templated dispatchers, since they don't need to know
// what type |args| is.

// Non-Static Dispatchers with no out params.

template <typename ObjT, typename Method, typename A>
inline void DispatchToMethod(ObjT* obj, Method method, const A& arg) {
  (obj->*method)(base::internal::UnwrapTraits<A>::Unwrap(arg));
}

template <typename ObjT, typename Method, typename... Ts, size_t... Ns>
inline void DispatchToMethodImpl(ObjT* obj,
                                 Method method,
                                 const Tuple<Ts...>& arg,
                                 IndexSequence<Ns...>) {
  (obj->*method)(base::internal::UnwrapTraits<Ts>::Unwrap(get<Ns>(arg))...);
}

template <typename ObjT, typename Method, typename... Ts>
inline void DispatchToMethod(ObjT* obj,
                             Method method,
                             const Tuple<Ts...>& arg) {
  DispatchToMethodImpl(obj, method, arg, MakeIndexSequence<sizeof...(Ts)>());
}

// Static Dispatchers with no out params.

template <typename Function, typename A>
inline void DispatchToMethod(Function function, const A& arg) {
  (*function)(base::internal::UnwrapTraits<A>::Unwrap(arg));
}

template <typename Function, typename... Ts, size_t... Ns>
inline void DispatchToFunctionImpl(Function function,
                                   const Tuple<Ts...>& arg,
                                   IndexSequence<Ns...>) {
  (*function)(base::internal::UnwrapTraits<Ts>::Unwrap(get<Ns>(arg))...);
}

template <typename Function, typename... Ts>
inline void DispatchToFunction(Function function, const Tuple<Ts...>& arg) {
  DispatchToFunctionImpl(function, arg, MakeIndexSequence<sizeof...(Ts)>());
}

// Dispatchers with out parameters.

template <typename ObjT,
          typename Method,
          typename In,
          typename... OutTs,
          size_t... OutNs>
inline void DispatchToMethodImpl(ObjT* obj,
                                 Method method,
                                 const In& in,
                                 Tuple<OutTs...>* out,
                                 IndexSequence<OutNs...>) {
  (obj->*method)(base::internal::UnwrapTraits<In>::Unwrap(in),
                 &get<OutNs>(*out)...);
}

template <typename ObjT, typename Method, typename In, typename... OutTs>
inline void DispatchToMethod(ObjT* obj,
                             Method method,
                             const In& in,
                             Tuple<OutTs...>* out) {
  DispatchToMethodImpl(obj, method, in, out,
                       MakeIndexSequence<sizeof...(OutTs)>());
}

template <typename ObjT,
          typename Method,
          typename... InTs,
          typename... OutTs,
          size_t... InNs,
          size_t... OutNs>
inline void DispatchToMethodImpl(ObjT* obj,
                                 Method method,
                                 const Tuple<InTs...>& in,
                                 Tuple<OutTs...>* out,
                                 IndexSequence<InNs...>,
                                 IndexSequence<OutNs...>) {
  (obj->*method)(base::internal::UnwrapTraits<InTs>::Unwrap(get<InNs>(in))...,
                 &get<OutNs>(*out)...);
}

template <typename ObjT, typename Method, typename... InTs, typename... OutTs>
inline void DispatchToMethod(ObjT* obj,
                             Method method,
                             const Tuple<InTs...>& in,
                             Tuple<OutTs...>* out) {
  DispatchToMethodImpl(obj, method, in, out,
                       MakeIndexSequence<sizeof...(InTs)>(),
                       MakeIndexSequence<sizeof...(OutTs)>());
}

#endif  // BASE_TUPLE_H__
