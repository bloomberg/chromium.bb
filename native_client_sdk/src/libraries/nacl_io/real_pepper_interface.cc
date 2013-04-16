/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "nacl_io/real_pepper_interface.h"
#include <assert.h>
#include <stdio.h>

#include <ppapi/c/pp_errors.h>


#include "nacl_io/pepper/undef_macros.h"
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    class Real##BaseClass : public BaseClass { \
     public: \
      explicit Real##BaseClass(const PPInterface* interface);
#define END_INTERFACE(BaseClass, PPInterface) \
     private: \
      const PPInterface* interface_; \
    };
#define METHOD1(Class, ReturnType, MethodName, Type0) \
    virtual ReturnType MethodName(Type0);
#define METHOD2(Class, ReturnType, MethodName, Type0, Type1) \
    virtual ReturnType MethodName(Type0, Type1);
#define METHOD3(Class, ReturnType, MethodName, Type0, Type1, Type2) \
    virtual ReturnType MethodName(Type0, Type1, Type2);
#define METHOD4(Class, ReturnType, MethodName, Type0, Type1, Type2, Type3) \
    virtual ReturnType MethodName(Type0, Type1, Type2, Type3);
#define METHOD5(Class, ReturnType, MethodName, Type0, Type1, Type2, Type3, \
                Type4) \
    virtual ReturnType MethodName(Type0, Type1, Type2, Type3, Type4);
#include "nacl_io/pepper/all_interfaces.h"


#include "nacl_io/pepper/undef_macros.h"
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    Real##BaseClass::Real##BaseClass(const PPInterface* interface) \
        : interface_(interface) {}

#define END_INTERFACE(BaseClass, PPInterface)

#define METHOD1(BaseClass, ReturnType, MethodName, Type0) \
    ReturnType Real##BaseClass::MethodName(Type0 arg0) { \
      return interface_->MethodName(arg0); \
    }
#define METHOD2(BaseClass, ReturnType, MethodName, Type0, Type1) \
    ReturnType Real##BaseClass::MethodName(Type0 arg0, Type1 arg1) { \
      return interface_->MethodName(arg0, arg1); \
    }
#define METHOD3(BaseClass, ReturnType, MethodName, Type0, Type1, Type2) \
    ReturnType Real##BaseClass::MethodName(Type0 arg0, Type1 arg1, \
                                           Type2 arg2) { \
      return interface_->MethodName(arg0, arg1, arg2); \
    }
#define METHOD4(BaseClass, ReturnType, MethodName, Type0, Type1, Type2, Type3) \
    ReturnType Real##BaseClass::MethodName(Type0 arg0, Type1 arg1, Type2 arg2, \
                                           Type3 arg3) { \
      return interface_->MethodName(arg0, arg1, arg2, arg3); \
    }
#define METHOD5(BaseClass, ReturnType, MethodName, Type0, Type1, Type2, Type3, \
                Type4) \
    ReturnType Real##BaseClass::MethodName(Type0 arg0, Type1 arg1, Type2 arg2, \
                                           Type3 arg3, Type4 arg4) { \
      return interface_->MethodName(arg0, arg1, arg2, arg3, arg4); \
    }
#include "nacl_io/pepper/all_interfaces.h"


RealPepperInterface::RealPepperInterface(PP_Instance instance,
                                         PPB_GetInterface get_browser_interface)
    : instance_(instance),
      core_interface_(NULL),
      message_loop_interface_(NULL) {

  core_interface_ = static_cast<const PPB_Core*>(
      get_browser_interface(PPB_CORE_INTERFACE));
  message_loop_interface_ = static_cast<const PPB_MessageLoop*>(
      get_browser_interface(PPB_MESSAGELOOP_INTERFACE));
  assert(core_interface_);
  assert(message_loop_interface_);

#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    BaseClass##interface_ = new Real##BaseClass( \
        static_cast<const PPInterface*>( \
            get_browser_interface(InterfaceString)));
#include "nacl_io/pepper/all_interfaces.h"
}

PP_Instance RealPepperInterface::GetInstance() {
  return instance_;
}

void RealPepperInterface::AddRefResource(PP_Resource resource) {
  if (resource)
    core_interface_->AddRefResource(resource);
}

void RealPepperInterface::ReleaseResource(PP_Resource resource) {
  if (resource)
    core_interface_->ReleaseResource(resource);
}

bool RealPepperInterface::IsMainThread() {
  return core_interface_->IsMainThread();
}

// Define getter function.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    BaseClass* RealPepperInterface::Get##BaseClass() { \
      return BaseClass##interface_; \
    }
#include "nacl_io/pepper/all_interfaces.h"


int32_t RealPepperInterface::InitializeMessageLoop() {
  int32_t result;
  PP_Resource message_loop = 0;
  if (core_interface_->IsMainThread()) {
    // TODO(binji): Spin up the main thread's ppapi work thread.
    assert(0);
  } else {
    message_loop = message_loop_interface_->GetCurrent();
    if (!message_loop) {
      message_loop = message_loop_interface_->Create(instance_);
      result = message_loop_interface_->AttachToCurrentThread(message_loop);
      assert(result == PP_OK);
    }
  }

  return PP_OK;
}
