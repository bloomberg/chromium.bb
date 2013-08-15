// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_PEPPER_INTERFACE_H_
#define LIBRARIES_NACL_IO_PEPPER_INTERFACE_H_

#include <ppapi/c/pp_bool.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_file_info.h>
#include <ppapi/c/pp_instance.h>
#include <ppapi/c/pp_resource.h>
#include <ppapi/c/pp_var.h>
#include <ppapi/c/ppb_console.h>
#include <ppapi/c/ppb_core.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/ppb_file_ref.h>
#include <ppapi/c/ppb_file_system.h>
#include <ppapi/c/ppb_host_resolver.h>
#include <ppapi/c/ppb_messaging.h>
#include <ppapi/c/ppb_messaging.h>
#include <ppapi/c/ppb_net_address.h>
#include <ppapi/c/ppb_tcp_socket.h>
#include <ppapi/c/ppb_url_loader.h>
#include <ppapi/c/ppb_url_request_info.h>
#include <ppapi/c/ppb_url_response_info.h>
#include <ppapi/c/ppb_udp_socket.h>
#include <ppapi/c/ppb_var.h>

#include <sdk_util/macros.h>

namespace nacl_io {

// This class is the base interface for Pepper used by nacl_io.
//
// We use #include and macro magic to simplify adding new interfaces. The
// resulting PepperInterface basically looks like this:
//
// class PepperInterface {
//  public:
//   virtual ~PepperInterface() {}
//   virtual PP_Instance GetInstance() = 0;
//   ...
//
//   // Interface getters.
//   ConsoleInterface* GetConsoleInterface() = 0;
//   CoreInterface* GetCoreInterface() = 0;
//   FileIoInterface* GetFileIoInterface() = 0;
//   ... etc.
// };
//
//
// Note: To add a new interface:
//
// 1. Using one of the other interfaces as a template, add your interface to
//    all_interfaces.h.
// 2. Add the necessary pepper header to the top of this file.
// 3. Compile and cross your fingers!


// Forward declare interface classes.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    class BaseClass;
#include "nacl_io/pepper/all_interfaces.h"

int PPErrorToErrno(int32_t err);

class PepperInterface {
 public:
  virtual ~PepperInterface() {}
  virtual PP_Instance GetInstance() = 0;

  // Convenience functions. These forward to
  // GetCoreInterface()->{AddRef,Release}Resource.
  void AddRefResource(PP_Resource resource);
  void ReleaseResource(PP_Resource resource);

// Interface getters.
//
// These macros expand to definitions like:
//
//   CoreInterface* GetCoreInterface() = 0;
//
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    virtual BaseClass* Get##BaseClass() = 0;
#include "nacl_io/pepper/all_interfaces.h"
};

// Interface class definitions.
//
// Each class will be defined with all pure virtual methods, e.g:
//
//   class CoreInterface {
//    public:
//     virtual ~CoreInterface() {}
//     virtual void AddRefResource() = 0;
//     virtual void ReleaseResource() = 0;
//     virtual PP_Bool IsMainThread() = 0;
//   };
//
#include "nacl_io/pepper/undef_macros.h"
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    class BaseClass { \
     public: \
      virtual ~BaseClass() {}
#define END_INTERFACE(BaseClass, PPInterface) \
    };
#define METHOD0(Class, ReturnType, MethodName) \
    virtual ReturnType MethodName() = 0;
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
#include "nacl_io/pepper/all_interfaces.h"


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

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_PEPPER_INTERFACE_H_
