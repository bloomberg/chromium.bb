// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/plugin/plugin_npobject_identity_test.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace {

class NPThingy : public NPObject {
 public:
  NPThingy() : NPObject() {}

  static NPObject* Allocate(NPP npp, NPClass* npclass) {
    return new NPThingy();
  }

  static void Deallocate(NPObject* npobject) {
    delete static_cast<NPThingy*>(npobject);
  }
};

NPClass* GetNPThingyClass() {
  static NPClass plugin_class = {
    NP_CLASS_STRUCT_VERSION,
    NPThingy::Allocate,
    NPThingy::Deallocate,
    NULL, // Invalidate
    NULL, // HasMethod
    NULL, // Invoke
    NULL, // InvokeDefault
    NULL, // HasProperty
    NULL, // GetProperty
    NULL, // SetProperty
    NULL, // RemoveProperty
  };
  return &plugin_class;
}


}  // namespace

namespace NPAPIClient {

NPObjectIdentityTest::NPObjectIdentityTest(NPP id,
                                           NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions) {
}

NPError NPObjectIdentityTest::SetWindow(NPWindow* pNPWindow) {
#if !defined(OS_MACOSX)
  if (pNPWindow->window == NULL)
    return NPERR_NO_ERROR;
#endif

  NPIdentifier are_these_the_same_id = HostFunctions()->getstringidentifier(
      "areTheseTheSame");

  // Get a function from window.areTheseTheSame.
  NPObject* window;
  HostFunctions()->getvalue(id(), NPNVWindowNPObject, &window);
  NPVariant func_var;
  HostFunctions()->getproperty(id(), window, are_these_the_same_id, &func_var);
  NPObject* func = NPVARIANT_TO_OBJECT(func_var);

  // Create a custom NPObject and pass it in both arguments to areTheseTheSame.
  NPObject* thingy = HostFunctions()->createobject(id(), GetNPThingyClass());
  NPVariant func_args[2];
  OBJECT_TO_NPVARIANT(thingy, func_args[0]);
  OBJECT_TO_NPVARIANT(thingy, func_args[1]);
  NPVariant were_the_same_var;
  HostFunctions()->invokeDefault(id(), func, (const NPVariant*)&func_args, 2,
                                 &were_the_same_var);

  // Confirm that JavaScript could see that the objects were the same.
  bool were_the_same = NPVARIANT_TO_BOOLEAN(were_the_same_var);
  if (!were_the_same)
    SetError("Identity was lost in passing from NPAPI into JavaScript.");

  HostFunctions()->releaseobject(thingy);

  // If this test failed, then we'd have crashed by now.
  SignalTestCompleted();

  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient
