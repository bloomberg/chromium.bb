// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_TRY_CATCH_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_TRY_CATCH_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "v8/include/v8.h"

namespace content {

class PepperPluginInstanceImpl;

// Base class for scripting TryCatch helpers.
class PepperTryCatch {
 public:
  PepperTryCatch(PepperPluginInstanceImpl* instance,
                 bool convert_objects);
  virtual ~PepperTryCatch();

  virtual void SetException(const char* message) = 0;
  // Gets the plugin context. Virtual so it can be overriden for testing.
  virtual v8::Handle<v8::Context> GetContext();

  // Convenience functions for doing conversions to/from V8 values and sets an
  // exception if there is an error in the conversion.
  v8::Handle<v8::Value> ToV8(PP_Var var);
  ppapi::ScopedPPVar FromV8(v8::Handle<v8::Value> v8_value);

 protected:
  PepperPluginInstanceImpl* instance_;

  // Whether To/FromV8 should convert object vars. If set to false, an exception
  // should be set if they are encountered during conversion.
  bool convert_objects_;
};

// Catches var exceptions and emits a v8 exception.
class PepperTryCatchV8 : public PepperTryCatch {
 public:
  PepperTryCatchV8(PepperPluginInstanceImpl* instance,
                   bool convert_objects,
                   v8::Isolate* isolate);
  virtual ~PepperTryCatchV8();

  bool HasException();
  bool ThrowException();
  void ThrowException(const char* message);
  PP_Var* exception() { return &exception_; }

  // PepperTryCatch
  virtual void SetException(const char* message) OVERRIDE;

 private:
  PP_Var exception_;

  DISALLOW_COPY_AND_ASSIGN(PepperTryCatchV8);
};

// Catches v8 exceptions and emits a var exception.
class PepperTryCatchVar : public PepperTryCatch {
 public:
  // The PP_Var exception will be placed in |exception|. The user of this class
  // is responsible for managing the lifetime of the exception. It is valid to
  //  pass NULL for |exception| in which case no exception will be set.
  PepperTryCatchVar(PepperPluginInstanceImpl* instance,
                    bool convert_objects,
                    PP_Var* exception);
  virtual ~PepperTryCatchVar();

  bool HasException();

  // PepperTryCatch
  virtual void SetException(const char* message) OVERRIDE;

 private:
  // Code which uses PepperTryCatchVar doesn't typically have a HandleScope,
  // make one for them. Note that this class is always allocated on the stack.
  v8::HandleScope handle_scope_;

  v8::TryCatch try_catch_;

  PP_Var* exception_;
  bool exception_is_set_;

  DISALLOW_COPY_AND_ASSIGN(PepperTryCatchVar);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_TRY_CATCH_H_
