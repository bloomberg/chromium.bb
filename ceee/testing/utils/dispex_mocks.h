// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock class implementations for IDispatch/IDispatchEx.
#ifndef CEEE_TESTING_UTILS_DISPEX_MOCKS_H_
#define CEEE_TESTING_UTILS_DISPEX_MOCKS_H_

#include <dispex.h>
#include "gmock/gmock.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/instance_count_mixin.h"

namespace testing {

class IDispatchExMockImpl: public IDispatchEx {
 public:
  // The methods in this class are code generated using this command line:
  // [ tools\com_mock.py IDispatchEx "%WindowsSdkDir%\Include\dispex.h" ]
  // (and then massaged slightly by hand)
#include "ceee/testing/utils/mock_idispatchex.gen"
};

// A mock implementation of IDispatch(Ex) with a handy set of
// functions to set up an expectation for Invoke.
class MockDispatchEx
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockDispatchEx>,
      public InstanceCountMixin<MockDispatchEx>,
      public StrictMock<IDispatchExMockImpl> {
 public:
  BEGIN_COM_MAP(MockDispatchEx)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IDispatchEx)
  END_COM_MAP()

  HRESULT Initialize(MockDispatchEx** self) {
    *self = this;
    return S_OK;
  }

  // Expect a function invoke with 0 params.
  void ExpectInvoke(DISPID disp_id) {
    EXPECT_CALL(*this, Invoke(disp_id, _, _, DISPATCH_METHOD,
                              Field(&DISPPARAMS::cArgs, Eq(0)), _, _, _));
  }

  // Expect a function invoke with 1 param.
  template <class A0>
  void ExpectInvoke(DISPID disp_id, const A0& a0) {
    EXPECT_CALL(*this, Invoke(disp_id, _, _, DISPATCH_METHOD,
                              AllOf(
                                  Field(&DISPPARAMS::cArgs, Eq(1)),
                                  DispParamArgEq<0>(CComVariant(a0))),
                               _, _, _));
  }

  // Expect a function invoke with 2 params.
  template <class A0, class A1>
  void ExpectInvoke(DISPID disp_id,
                    const A0& a0,
                    const A1& a1) {
    // Note the reverse order of dispid params from args.
    EXPECT_CALL(*this, Invoke(disp_id, _, _, DISPATCH_METHOD,
                              AllOf(
                                  Field(&DISPPARAMS::cArgs, Eq(2)),
                                  DispParamArgEq<1>(CComVariant(a0)),
                                  DispParamArgEq<0>(CComVariant(a1))),
                               _, _, _));
  }

  // Expect a function invoke with 3 params.
  template <class A0, class A1, class A2>
  void ExpectInvoke(DISPID disp_id,
                    const A0& a0,
                    const A1& a1,
                    const A2& a2) {
    // Note the reverse order of dispid params from args.
    EXPECT_CALL(*this, Invoke(disp_id, _, _, DISPATCH_METHOD,
                              AllOf(
                                  Field(&DISPPARAMS::cArgs, Eq(3)),
                                  DispParamArgEq<2>(CComVariant(a0)),
                                  DispParamArgEq<1>(CComVariant(a1)),
                                  DispParamArgEq<0>(CComVariant(a2))),
                               _, _, _));
  }

  // Expect a function invoke with 4 params.
  template <class A0, class A1, class A2, class A3>
  void ExpectInvoke(DISPID disp_id,
                    const A0& a0,
                    const A1& a1,
                    const A2& a2,
                    const A3& a3) {
    // Note the reverse order of dispid params from args.
    EXPECT_CALL(*this, Invoke(disp_id, _, _, DISPATCH_METHOD,
                              AllOf(
                                  Field(&DISPPARAMS::cArgs, Eq(4)),
                                  DispParamArgEq<3>(CComVariant(a0)),
                                  DispParamArgEq<2>(CComVariant(a1)),
                                  DispParamArgEq<1>(CComVariant(a2)),
                                  DispParamArgEq<0>(CComVariant(a3))),
                               _, _, _));
  }

  // Expect a function invoke with 5 params.
  template <class A0, class A1, class A2, class A3, class A4>
  void ExpectInvoke(DISPID disp_id,
                    const A0& a0,
                    const A1& a1,
                    const A2& a2,
                    const A3& a3,
                    const A4& a4) {
    // Note the reverse order of dispid params from args.
    EXPECT_CALL(*this, Invoke(disp_id, _, _, DISPATCH_METHOD,
                              AllOf(Field(&DISPPARAMS::cArgs, Eq(5)),
                                  AllOf(
                                     DispParamArgEq<4>(CComVariant(a0)),
                                     DispParamArgEq<3>(CComVariant(a1)),
                                     DispParamArgEq<2>(CComVariant(a2)),
                                     DispParamArgEq<1>(CComVariant(a3)),
                                     DispParamArgEq<0>(CComVariant(a4)))),
                               _, _, _));
  }
};

}  // testing

#endif  // CEEE_TESTING_UTILS_DISPEX_MOCKS_H_
