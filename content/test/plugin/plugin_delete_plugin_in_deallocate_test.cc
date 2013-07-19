// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/plugin/plugin_delete_plugin_in_deallocate_test.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace {

NPAPIClient::DeletePluginInDeallocateTest* g_signalling_instance_ = NULL;

class DeletePluginInDeallocateTestNPObject : public NPObject {
 public:
  DeletePluginInDeallocateTestNPObject()
      : NPObject(), id_(NULL), host_functions_(NULL), deallocate_count_(0) {}

  static NPObject* Allocate(NPP npp, NPClass* npclass) {
    return new DeletePluginInDeallocateTestNPObject();
  }

  static void Deallocate(NPObject* npobject) {
    DeletePluginInDeallocateTestNPObject* object =
        reinterpret_cast<DeletePluginInDeallocateTestNPObject*>(npobject);
    ++object->deallocate_count_;

    // Call window.deletePlugin to tear-down our plugin from inside deallocate.
    if (object->deallocate_count_ == 1) {
      NPIdentifier delete_id =
          object->host_functions_->getstringidentifier("deletePlugin");
      NPObject* window_obj = NULL;
      object->host_functions_->getvalue(object->id_, NPNVWindowNPObject,
                                        &window_obj);
      NPVariant rv;
      object->host_functions_->invoke(object->id_, window_obj, delete_id, NULL,
                                      0, &rv);
    }
  }

  NPP id_;
  NPNetscapeFuncs* host_functions_;
  int deallocate_count_;
};

NPClass* GetDeletePluginInDeallocateTestClass() {
  static NPClass plugin_class = {
    NP_CLASS_STRUCT_VERSION,
    DeletePluginInDeallocateTestNPObject::Allocate,
    DeletePluginInDeallocateTestNPObject::Deallocate,
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

DeletePluginInDeallocateTest::DeletePluginInDeallocateTest(
    NPP id, NPNetscapeFuncs* host_functions)
    : PluginTest(id, host_functions), npobject_(NULL), test_started_(false) {
}

NPError DeletePluginInDeallocateTest::SetWindow(NPWindow* pNPWindow) {
#if !defined(OS_MACOSX)
  if (pNPWindow->window == NULL)
    return NPERR_NO_ERROR;
#endif

  // Ensure that we run the test once, even if SetWindow is called again.
  if (test_started_)
    return NPERR_NO_ERROR;
  test_started_ = true;

  // Because of http://crbug.com/94829, we have to have a second plugin
  // instance that we can use to signal completion.
  if (test_id() == "signaller") {
    g_signalling_instance_ = this;
    return NPERR_NO_ERROR;
  }

  // Create a custom NPObject and give our Id and the Netscape function table.
  npobject_ = HostFunctions()->createobject(
      id(), GetDeletePluginInDeallocateTestClass());
  DeletePluginInDeallocateTestNPObject* test_object =
      reinterpret_cast<DeletePluginInDeallocateTestNPObject*>(npobject_);
  test_object->id_ = id();
  test_object->host_functions_ = HostFunctions();

  // Fetch the window script object for our page.
  NPObject* window = NULL;
  HostFunctions()->getvalue(id(), NPNVWindowNPObject, &window);

  // Pass it to the window.setTestObject function, which will later release it.
  NPIdentifier set_test_object_id =
      HostFunctions()->getstringidentifier("setTestObject");
  NPVariant func_var;
  NULL_TO_NPVARIANT(func_var);
  HostFunctions()->getproperty(id(), window, set_test_object_id, &func_var);

  NPObject* func = NPVARIANT_TO_OBJECT(func_var);
  NPVariant func_arg;
  OBJECT_TO_NPVARIANT(npobject_, func_arg);
  NPVariant func_result;
  HostFunctions()->invokeDefault(id(), func, &func_arg, 1,
                                 &func_result);

  // Release the object - the page's reference should keep it alive.
  HostFunctions()->releaseobject(npobject_);

  return NPERR_NO_ERROR;
}

NPError DeletePluginInDeallocateTest::Destroy() {
  // Because of http://crbug.com/94829, we can't signal completion from within
  // the NPP_Destroy of the plugin that is being destroyed.  We work-around
  // that by testing using a second instance, and signalling though the main
  // test instance.

  // There should always be a signalling instance by the time we reach here.
  if (!g_signalling_instance_)
    return NPERR_NO_ERROR;

  // If we're the signalling instance, do nothing.
  if (g_signalling_instance_ == this)
    return NPERR_NO_ERROR;

  if (!npobject_) {
    g_signalling_instance_->SetError("SetWindow was not invoked.");
  } else {
    // Verify that our object was deallocated exactly once.
    DeletePluginInDeallocateTestNPObject* test_object =
        reinterpret_cast<DeletePluginInDeallocateTestNPObject*>(npobject_);
    if (test_object->deallocate_count_ != 1)
      g_signalling_instance_->SetError(
          "Object was not deallocated exactly once.");
    delete test_object;
  }

  g_signalling_instance_->SignalTestCompleted();

  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient
