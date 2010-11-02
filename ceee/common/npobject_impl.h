// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Base classes to unclutter the writing of NPAPI objects.
#ifndef CEEE_COMMON_NPOBJECT_IMPL_H_
#define CEEE_COMMON_NPOBJECT_IMPL_H_

#include "base/basictypes.h"
#include "third_party/npapi/bindings/nphostapi.h"

// Base class for NPAPI objects.
class NpObjectBase: public NPObject {
 public:
  explicit NpObjectBase(NPP npp) : npp_(npp) {
  }
  virtual ~NpObjectBase() {
  }

 protected:
  // @name NP Object interface.
  // Override these to implement your object's functionality.
  // For documentation on these methods, see the corresponding callback
  // function description in http://developer.mozilla.org/en/NPObject
  // @{
  virtual void Invalidate();
  virtual bool HasMethod(NPIdentifier name);
  virtual bool Invoke(NPIdentifier name,
                      const NPVariant* args,
                      uint32_t argCount,
                      NPVariant* result);
  virtual bool InvokeDefault(const NPVariant* args,
                             uint32_t argCount,
                             NPVariant* result);
  virtual bool HasProperty(NPIdentifier name);
  virtual bool GetProperty(NPIdentifier name, NPVariant* result);
  virtual bool SetProperty(NPIdentifier name, const NPVariant* value);
  virtual bool RemoveProperty(NPIdentifier name);
  virtual bool Enumeration(NPIdentifier** value, uint32_t* count);
  virtual bool Construct(const NPVariant* args, uint32_t argCount,
                         NPVariant* result);
  // @}

  // @name NP callback functions.
  // @{
  static void NPDeallocate(NPObject* obj);
  static void NPInvalidate(NPObject* obj);
  static bool NPHasMethod(NPObject* obj, NPIdentifier name);
  static bool NPInvoke(NPObject* npobj,
                       NPIdentifier name,
                       const NPVariant* args,
                       uint32_t argCount,
                       NPVariant* result);
  static bool NPInvokeDefault(NPObject* npobj,
                              const NPVariant* args,
                              uint32_t argCount,
                              NPVariant* result);
  static bool NPHasProperty(NPObject* npobj, NPIdentifier name);
  static bool NPGetProperty(NPObject* npobj,
                            NPIdentifier name,
                            NPVariant* result);
  static bool NPSetProperty(NPObject* npobj,
                            NPIdentifier name,
                            const NPVariant* value);
  static bool NPRemoveProperty(NPObject* npobj,
                               NPIdentifier name);
  static bool NPEnumeration(NPObject* npobj, NPIdentifier** value,
                            uint32_t* count);
  static bool NPConstruct(NPObject* npobj, const NPVariant* args,
                          uint32_t arg_count, NPVariant* result);
  // @}

  NPP npp() const { return npp_; }
 private:
  // The NPP instance we belong to.
  NPP npp_;

  DISALLOW_COPY_AND_ASSIGN(NpObjectBase);
};


// Derive your NPAPI object class from NpObjectImpl<MyClass>.
template <class ImplClass> class NpObjectImpl: public NpObjectBase {
 public:
  explicit NpObjectImpl(NPP npp) : NpObjectBase(npp) {
  }

  static NPObject* NPAllocate(NPP npp, NPClass* np_class) {
    NPObject* object = new ImplClass(npp);
    object->_class = np_class;
    object->referenceCount = 1;
    return object;
  }

  static NPClass* object_class() { return &object_class_; }

 private:
  static NPClass object_class_;

  DISALLOW_COPY_AND_ASSIGN(NpObjectImpl);
};

template <class ImplClass> NPClass NpObjectImpl<ImplClass>::object_class_ = {
  NP_CLASS_STRUCT_VERSION,
  ImplClass::NPAllocate,  // allocate;
  ImplClass::NPDeallocate,  // deallocate;
  ImplClass::NPInvalidate,  // invalidate;
  ImplClass::NPHasMethod,  // hasMethod;
  ImplClass::NPInvoke,  // invoke;
  ImplClass::NPInvokeDefault,  // invokeDefault;
  ImplClass::NPHasProperty,  // hasProperty;
  ImplClass::NPGetProperty,  // getProperty;
  ImplClass::NPSetProperty,  // setProperty;
  ImplClass::NPRemoveProperty,  // removeProperty;
  ImplClass::NPEnumeration,  // enumerate
  ImplClass::NPConstruct,  // construct
};

#endif  // CEEE_COMMON_NPOBJECT_IMPL_H_
