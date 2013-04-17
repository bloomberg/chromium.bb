/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_TEST_PEPPER_INTERFACE_MOCK_H_
#define LIBRARIES_NACL_IO_TEST_PEPPER_INTERFACE_MOCK_H_

#include "gmock/gmock.h"
#include "nacl_io/pepper_interface.h"

// Mock interface class definitions.
#include "nacl_io/pepper/undef_macros.h"
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    class BaseClass##Mock : public BaseClass { \
     public: \
      BaseClass##Mock(); \
      virtual ~BaseClass##Mock();
#define END_INTERFACE(BaseClass, PPInterface) \
    };
#define METHOD1(Class, ReturnType, MethodName, Type0) \
    MOCK_METHOD1(MethodName, ReturnType(Type0));
#define METHOD2(Class, ReturnType, MethodName, Type0, Type1) \
    MOCK_METHOD2(MethodName, ReturnType(Type0, Type1));
#define METHOD3(Class, ReturnType, MethodName, Type0, Type1, Type2) \
    MOCK_METHOD3(MethodName, ReturnType(Type0, Type1, Type2));
#define METHOD4(Class, ReturnType, MethodName, Type0, Type1, Type2, Type3) \
    MOCK_METHOD4(MethodName, ReturnType(Type0, Type1, Type2, Type3));
#define METHOD5(Class, ReturnType, MethodName, Type0, Type1, Type2, Type3, \
                Type4) \
    MOCK_METHOD5(MethodName, ReturnType(Type0, Type1, Type2, Type3, Type4));
#include "nacl_io/pepper/all_interfaces.h"


class PepperInterfaceMock : public PepperInterface {
 public:
  explicit PepperInterfaceMock(PP_Instance instance);
  ~PepperInterfaceMock();

  virtual PP_Instance GetInstance();
  MOCK_METHOD1(AddRefResource, void(PP_Resource));
  MOCK_METHOD1(ReleaseResource, void(PP_Resource));
  MOCK_METHOD0(IsMainThread, bool());

// Interface getters.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    virtual BaseClass##Mock* Get##BaseClass();
#include "nacl_io/pepper/all_interfaces.h"

 private:
  PP_Instance instance_;

// Interface pointers.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    BaseClass##Mock* BaseClass##interface_;
#include "nacl_io/pepper/all_interfaces.h"

  int dummy_;
};


#endif  // LIBRARIES_NACL_IO_TEST_PEPPER_INTERFACE_MOCK_H_
