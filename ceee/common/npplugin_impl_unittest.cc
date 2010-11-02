// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests for NPAPI Plugin Wrapper classes.

#include "ceee/common/npplugin_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace {

// A bare bones implementation class which really just uses the default
// implementations of NpPluginImpl/NpPluginBase so we can test those
// wrapper classes for correct default NPAPI functionality.
class BareNpPlugin : public NpPluginImpl<BareNpPlugin> {
 public:
  explicit BareNpPlugin(NPP instance) : NpPluginImpl<BareNpPlugin>(instance) {
  }
  ~BareNpPlugin() {
  }

  // TODO(stevet@google.com): Note that we re implement this to avoid
  // the DCHECK in the original base class. If we can find a non
  // crashing method of reminding developers to implement
  // DestroyStream, we can remove this implementation.
  virtual NPError DestroyStream(NPStream* stream,
    NPReason reason) {
    return NPERR_GENERIC_ERROR;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BareNpPlugin);
};

// A mock class which allows us to verify that corresponding NPPlugin functions
// are called after they are setup by a call to GetEntryPoints.
class MockNpPlugin : public NpPluginImpl<MockNpPlugin> {
 public:
  explicit MockNpPlugin(NPP instance) : NpPluginImpl<MockNpPlugin>(instance) {
  }
  ~MockNpPlugin() {
  }

  NPError Initialize() {
    return NPERR_NO_ERROR;
  }

  MOCK_METHOD1(SetWindow, NPError(NPWindow* window));
  MOCK_METHOD4(NewStream, NPError(NPMIMEType type, NPStream* stream,
      NPBool seekable, uint16* stype));
  MOCK_METHOD2(DestroyStream, NPError(NPStream* stream, NPReason reason));
  MOCK_METHOD1(WriteReady, int32(NPStream* stream));
  MOCK_METHOD4(Write, int32(NPStream* stream, int32 offset,
      int32 len, void* buffer));
  MOCK_METHOD2(StreamAsFile, void(NPStream* stream, const char* fname));
  MOCK_METHOD1(Print, void(NPPrint* platform_print));
  MOCK_METHOD1(HandleEvent, int16(void* event));
  MOCK_METHOD3(URLNotify, void(const char* url, NPReason reason,
      void* notify_data));
  MOCK_METHOD2(GetValue, NPError(NPPVariable variable, void* value));
  MOCK_METHOD2(SetValue, NPError(NPNVariable variable, void* value));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNpPlugin);
};

// Test to ensure that the NPPlugin helpers' defaults are correct. Note that
// we use http://goo.gl/u0VT as a rough guide to what values are defaults.
TEST(NpApiWrappers, NpPluginDefaults) {
  NPP instance = new NPP_t;
  BareNpPlugin np_plugin(instance);
  instance->pdata = &np_plugin;

  NPPluginFuncs plugin_funcs;
  EXPECT_EQ(NPERR_NO_ERROR, MockNpPlugin::GetEntryPoints(&plugin_funcs));

  EXPECT_EQ(NPERR_NO_ERROR, plugin_funcs.setwindow(instance, NULL));
  EXPECT_EQ(NPERR_GENERIC_ERROR,
      plugin_funcs.newstream(instance, NULL, NULL, '0', NULL));
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_funcs.destroystream(instance, NULL, 0));
  EXPECT_EQ(0, plugin_funcs.writeready(instance, NULL));
  EXPECT_EQ(0, plugin_funcs.write(instance, NULL, 0, 0, NULL));
  EXPECT_EQ(0, plugin_funcs.event(instance, NULL));
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_funcs.getvalue(instance,
    (NPPVariable)1, NULL));
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_funcs.setvalue(instance,
    (NPNVariable)1, NULL));
}

// Initialize a NPPluginFuncs struct with a call to NpPluginImpl<ImplClass>
// ::GetEntryPoints and ensure that the appropriate functions are attached
// to the structure.
TEST(NpApiWrappers, NpPluginCalls) {
  // Create a faux NPP instance since function calls need it's pdata field to
  // determine what plugin object should be making the call.
  NPP instance = new NPP_t;
  MockNpPlugin np_plugin(instance);
  instance->pdata = &np_plugin;

  NPPluginFuncs plugin_funcs;
  EXPECT_EQ(NPERR_NO_ERROR, MockNpPlugin::GetEntryPoints(&plugin_funcs));

  EXPECT_CALL(np_plugin, SetWindow(NULL))
      .Times(1)
      .WillOnce(Return(NPERR_NO_ERROR));
  EXPECT_EQ(NPERR_NO_ERROR, plugin_funcs.setwindow(instance, NULL));

  EXPECT_CALL(np_plugin, NewStream(NULL, NULL, '0', NULL))
      .Times(1)
      .WillOnce(Return(NPERR_GENERIC_ERROR));
  EXPECT_EQ(NPERR_GENERIC_ERROR,
      plugin_funcs.newstream(instance, NULL, NULL, '0', NULL));

  EXPECT_CALL(np_plugin, DestroyStream(NULL, 0))
      .Times(1)
      .WillOnce(Return(NPERR_GENERIC_ERROR));
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_funcs.destroystream(instance, NULL, 0));

  EXPECT_CALL(np_plugin, WriteReady(NULL))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_EQ(0, plugin_funcs.writeready(instance, NULL));

  EXPECT_CALL(np_plugin, Write(NULL, 0, 0, NULL))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_EQ(0, plugin_funcs.write(instance, NULL, 0, 0, NULL));

  EXPECT_CALL(np_plugin, StreamAsFile(NULL, NULL))
      .Times(1);
  plugin_funcs.asfile(instance, NULL, NULL);

  EXPECT_CALL(np_plugin, Print(NULL))
      .Times(1);
  plugin_funcs.print(instance, NULL);

  EXPECT_CALL(np_plugin, HandleEvent(NULL))
      .Times(1)
      .WillOnce(Return(0));
  EXPECT_EQ(0, plugin_funcs.event(instance, NULL));

  EXPECT_CALL(np_plugin, URLNotify(NULL, 0, NULL))
      .Times(1);
  plugin_funcs.urlnotify(instance, NULL, 0, NULL);

  EXPECT_CALL(np_plugin, GetValue((NPPVariable)1, NULL))
      .Times(1)
      .WillOnce(Return(NPERR_GENERIC_ERROR));
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_funcs.getvalue(instance,
      (NPPVariable)1, NULL));

  EXPECT_CALL(np_plugin, SetValue((NPNVariable)1, NULL))
      .Times(1)
      .WillOnce(Return(NPERR_GENERIC_ERROR));
  EXPECT_EQ(NPERR_GENERIC_ERROR, plugin_funcs.setvalue(instance,
      (NPNVariable)1, NULL));

  delete instance;
  instance = NULL;
}

}  // namespace
