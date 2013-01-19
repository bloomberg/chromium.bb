/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_MOUNTS_PEPPER_INTERFACE_H_
#define LIBRARIES_NACL_MOUNTS_PEPPER_INTERFACE_H_

#include <ppapi/c/dev/ppb_directory_reader_dev.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_file_info.h>
#include <ppapi/c/pp_instance.h>
#include <ppapi/c/pp_resource.h>
#include <ppapi/c/pp_var.h>
#include <ppapi/c/ppb_console.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/ppb_file_ref.h>
#include <ppapi/c/ppb_file_system.h>
#include <ppapi/c/ppb_messaging.h>
#include <ppapi/c/ppb_messaging.h>
#include <ppapi/c/ppb_url_loader.h>
#include <ppapi/c/ppb_url_request_info.h>
#include <ppapi/c/ppb_url_response_info.h>
#include <ppapi/c/ppb_var.h>

#include <utils/macros.h>

// Note: To add a new interface:
//
// 1. Using one of the other interfaces as a template, add your interface to
//    all_interfaces.h.
// 2. Add the necessary pepper header to the top of this file.
// 3. Compile and cross your fingers!


// Forward declare interface classes.
#include "nacl_mounts/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    class BaseClass;
#include "nacl_mounts/pepper/all_interfaces.h"
#include "nacl_mounts/pepper/undef_macros.h"

int PPErrorToErrno(int32_t err);

class PepperInterface {
 public:
  virtual ~PepperInterface() {}
  virtual PP_Instance GetInstance() = 0;
  virtual void AddRefResource(PP_Resource) = 0;
  virtual void ReleaseResource(PP_Resource) = 0;

// Interface getters.
#include "nacl_mounts/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    virtual BaseClass* Get##BaseClass() = 0;
#include "nacl_mounts/pepper/all_interfaces.h"
#include "nacl_mounts/pepper/undef_macros.h"
};

// Interface class definitions.
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    class BaseClass { \
     public: \
      virtual ~BaseClass() {}
#define END_INTERFACE(BaseClass, PPInterface) \
    };
#define METHOD1(Class, ReturnType, MethodName, Type0) \
    virtual ReturnType MethodName(Type0) = 0;
#define METHOD2(Class, ReturnType, MethodName, Type0, Type1) \
    virtual ReturnType MethodName(Type0, Type1) = 0;
#define METHOD3(Class, ReturnType, MethodName, Type0, Type1, Type2) \
    virtual ReturnType MethodName(Type0, Type1, Type2) = 0;
#define METHOD4(Class, ReturnType, MethodName, Type0, Type1, Type2, Type3) \
    virtual ReturnType MethodName(Type0, Type1, Type2, Type3) = 0;
#define METHOD5(Class, ReturnType, MethodName, Type0, Type1, Type2, Type3, \
                Type4) \
    virtual ReturnType MethodName(Type0, Type1, Type2, Type3, Type4) = 0;
#include "nacl_mounts/pepper/all_interfaces.h"
#include "nacl_mounts/pepper/undef_macros.h"


class ScopedResource {
 public:
  // Does not AddRef by default.
  ScopedResource(PepperInterface* ppapi, PP_Resource resource);
  ~ScopedResource();

  PP_Resource pp_resource() { return resource_; }

  // Return the resource without decrementing its refcount.
  PP_Resource Release();

 private:
  PepperInterface* ppapi_;
  PP_Resource resource_;

  DISALLOW_COPY_AND_ASSIGN(ScopedResource);
};

#endif  // LIBRARIES_NACL_MOUNTS_PEPPER_INTERFACE_H_
