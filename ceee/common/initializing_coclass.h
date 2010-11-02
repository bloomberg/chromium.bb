// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
//
// Declares a convenience coclass implementation that makes it easy to create
// initialized object instances.
#ifndef CEEE_COMMON_INITIALIZING_COCLASS_H_
#define CEEE_COMMON_INITIALIZING_COCLASS_H_

#include <atlbase.h>
#include <atlcom.h>

using ATL::CComObject;

// A convenience mixin coclass, to allow creating and initializing
// CComObject<T>-derived object instances, and optionally query them for
// a given interface, all in a single CreateInstance invocation.
//
// Usage:
// <code>
// class MyObjectImpl: public CComObjectRoot<...>,
//         public InitializingCoClass<MyObjectImpl>, ... {
// public:
//   // This function will be invoked when MyObjectImpl::CreateInstance with N
//   // arguments of the appropriate type is invoked.
//   // @note if this function returns error, the object will be destroyed
//   // @note reference args must be const
//   HRESULT Initialize(SomeType1 arg1, ..., SomeTypeN argN);
// };
// ...
// CComPtr<IFoo> foo;
// HRESULT hr = MyObjectImp::CreateInstance(arg1, ..., argN, &foo)
// </code>
//
// @param T the CComObjectRootEx derivative class you want to create and
//          initalize.
template <class T>
class InitializingCoClass {
 public:
  typedef CComObject<T> TImpl;

  // Creates an instance of CComObject<T> and initializes it.
  // @param new_instance on success returns the new, initialized instance.
  // @note this method does not return a reference to the new object.
  static HRESULT CreateInstance(T** new_instance) {
    TImpl* instance;
    HRESULT hr = TImpl::CreateInstance(&instance);
    if (FAILED(hr))
      return hr;

    instance->InternalFinalConstructAddRef();
    hr = instance->Initialize();
    instance->InternalFinalConstructRelease();
    if (FAILED(hr)) {
      delete instance;
      instance = NULL;
    }
    *new_instance = instance;

    return hr;
  }

  // Creates an instance of CComObject<T>, initializes it and queries it
  // for interface I.
  // @param instance on success returns the new, initialized instance.
  template <class I>
  static HRESULT CreateInitialized(I** instance) {
    T* new_instance;
    HRESULT hr = CreateInstance(&new_instance);
    if (FAILED(hr))
      return hr;

    hr = new_instance->QueryInterface(__uuidof(I),
                                      reinterpret_cast<void**>(instance));
    if (FAILED(hr))
      delete new_instance;

    return hr;
  }

  // Creates an instance of CComObject<T>, initializes it and queries it
  // for an interface.
  // @param iid the interface to query for. Must be congruent to the type I.
  // @param instance on success returns the new, initialized instance.
  template <class I>
  static HRESULT CreateInitializedIID(REFIID iid, I** instance) {
    T* new_instance;
    HRESULT hr = CreateInstance(&new_instance);
    if (FAILED(hr))
      return hr;

    hr = new_instance->QueryInterface(iid, reinterpret_cast<void**>(instance));
    if (FAILED(hr))
      delete new_instance;

    return hr;
  }


// This file is chock full of repetitions of the same code, and because we
// don't yet stock infinite monkeys (nor infinite developers to review their
// code) we substitute with the python script initializing_coclass.py
//
// The functions therein are of this general make:

// Create a new, initialized T* instance. Note that zero references will
// be held to *new_instance at return.
//
// template <class A1, ..., class An>
// HRESULT CreateInstance(A1 a1, ..., An an, T **new_instance);
//
// Create a new, initialized T* instance, and QI it for I
// template <class I, class A1, ..., class An>
// HRESULT CreateInstance(A1 a1, ..., An an, I **new_instance);
#include "initializing_coclass_gen.inl"  // NOLINT
};

#ifndef CEEE_COMMON_INITIALIZING_COCLASS_GEN_INL_
#error initializing_coclass_gen.inl wasn't generated correctly.
#endif

#endif  // CEEE_COMMON_INITIALIZING_COCLASS_H_
