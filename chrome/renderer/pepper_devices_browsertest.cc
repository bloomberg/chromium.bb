// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/pepper_devices.h"
#include "chrome/renderer/webplugin_delegate_pepper.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPlugin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "webkit/plugins/npapi/plugin_instance.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_impl.h"

class PepperDeviceTest;

namespace {

const FilePath::CharType kTestPluginFileName[] =
    FILE_PATH_LITERAL("pepper-device-tester");
const char kTestPluginMimeType[] = "chrome-test/pepper-device-test";

// This maps the NPP instances to the test object so our C callbacks can easily
// get back to the object. There will normally be only one item in this map.
static std::map<NPP, PepperDeviceTest*> active_tests;

NPError NPP_New(NPMIMEType plugin_type, NPP instance,
                uint16 mode, int16 argc, char* argn[],
                char* argv[], NPSavedData* saved) {
  // Watch out: active_tests won't contain the NPP pointer until after this
  // call is complete, so don't use it.
  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** saved) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;
  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  return NPERR_NO_ERROR;
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  return 0;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;
  switch (variable) {
    case NPPVpluginNeedsXEmbed:
      *static_cast<NPBool*>(value) = 1;
      return NPERR_NO_ERROR;
    default:
      return NPERR_INVALID_PARAM;
  }
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void* value) {
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* plugin_funcs) {
  plugin_funcs->newp = NPP_New;
  plugin_funcs->destroy = NPP_Destroy;
  plugin_funcs->setwindow = NPP_SetWindow;
  plugin_funcs->event = NPP_HandleEvent;
  plugin_funcs->getvalue = NPP_GetValue;
  plugin_funcs->setvalue = NPP_SetValue;
  return NPERR_NO_ERROR;
}

#if defined(OS_MACOSX) || defined(OS_WIN)
NPError API_CALL NP_Initialize(NPNetscapeFuncs* browser_funcs) {
  return NPERR_NO_ERROR;
}
#else
NPError API_CALL NP_Initialize(NPNetscapeFuncs* browser_funcs,
                               NPPluginFuncs* plugin_funcs) {
  NP_GetEntryPoints(plugin_funcs);
  return NPERR_NO_ERROR;
}
#endif

NPError API_CALL NP_Shutdown() {
  return NPERR_NO_ERROR;
}

}  // namespace

// PepperDeviceTest ------------------------------------------------------------

class PepperDeviceTest : public RenderViewTest {
 public:
  WebPluginDelegatePepper* pepper_plugin() const { return pepper_plugin_; }

  NPP npp() const { return pepper_plugin_->instance()->npp(); }

 protected:
  // Logs that the given flush command was called in flush_calls.
  static void FlushCalled(NPP instance,
                          NPDeviceContext* context,
                          NPError err,
                          NPUserData* user_data);

  // Audio callback, currently empty.
  static void AudioCallback(NPDeviceContextAudio* context);

  // A log of flush commands we can use to check the async callbacks.
  struct FlushData {
    NPP instance;
    NPDeviceContext* context;
    NPError err;
    NPUserData* user_data;
  };
  std::vector<FlushData> flush_calls_;

 private:
  // testing::Test implementation.
  virtual void SetUp();
  virtual void TearDown();

  scoped_ptr<webkit::npapi::WebPluginImpl> plugin_;
  WebPluginDelegatePepper* pepper_plugin_;  // FIXME(brettw): check lifetime.
};

void PepperDeviceTest::SetUp() {
  RenderViewTest::SetUp();

  webkit::npapi::PluginEntryPoints entry_points = {
#if !defined(OS_POSIX) || defined(OS_MACOSX)
    NP_GetEntryPoints,
#endif
    NP_Initialize,
    NP_Shutdown
  };
  webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(
      FilePath(kTestPluginFileName),
      "Pepper device test plugin",
      "Pepper device test plugin",
      kTestPluginMimeType,
      entry_points);

  // Create the WebKit plugin with no delegates (this seems to work
  // sufficiently for the test).
  WebKit::WebPluginParams params;
  plugin_.reset(new webkit::npapi::WebPluginImpl(
      NULL, params, FilePath(), std::string(),
      base::WeakPtr<webkit::npapi::WebPluginPageDelegate>()));

  // Create a pepper plugin for the RenderView.
  pepper_plugin_ = WebPluginDelegatePepper::Create(
      FilePath(kTestPluginFileName), kTestPluginMimeType, view_->AsWeakPtr());
  ASSERT_TRUE(pepper_plugin_);
  ASSERT_TRUE(pepper_plugin_->Initialize(GURL(), std::vector<std::string>(),
                                         std::vector<std::string>(),
                                         plugin_.get(), false));

  // Normally the RenderView creates the pepper plugin and registers it with
  // its internal list. Since we're creating it manually, we have to reach in
  // and register it to prevent tear-down from asserting.
  view_->current_oldstyle_pepper_plugins_.insert(pepper_plugin_);

  active_tests[npp()] = this;

  // Need to specify a window size or graphics calls will fail on the 0x0
  // bitmap.
  gfx::Rect rect(0, 0, 100, 100);
  view_->OnResize(rect.size(), gfx::Rect());
  pepper_plugin_->UpdateGeometry(rect, rect);
}

void PepperDeviceTest::TearDown() {
  active_tests.erase(active_tests.find(npp()));

  plugin_.reset();
  if (pepper_plugin_)
    pepper_plugin_->PluginDestroyed();

  webkit::npapi::PluginList::Singleton()->UnregisterInternalPlugin(
      FilePath(kTestPluginFileName));

  RenderViewTest::TearDown();
}

// static
void PepperDeviceTest::FlushCalled(NPP instance,
                                   NPDeviceContext* context,
                                   NPError err,
                                   NPUserData* user_data) {
  if (active_tests.find(instance) == active_tests.end())
    return;
  PepperDeviceTest* that = active_tests[instance];

  FlushData flush_data;
  flush_data.instance = instance;
  flush_data.context = context;
  flush_data.err = err;
  flush_data.user_data = user_data;
  that->flush_calls_.push_back(flush_data);
}

void PepperDeviceTest::AudioCallback(NPDeviceContextAudio* context) {
}


// -----------------------------------------------------------------------------

// TODO(brettw) this crashes on Mac. Figure out why and enable.
#if !defined(OS_MACOSX)

TEST_F(PepperDeviceTest, Flush) {
  // Create a 2D device.
  NPDeviceContext2DConfig config;
  NPDeviceContext2D context;
  EXPECT_EQ(NPERR_NO_ERROR,
            pepper_plugin()->Device2DInitializeContext(&config, &context));

  // Flush the bitmap. Here we fake the invalidate call to the RenderView since
  // there isn't an actual visible web page that would otherwise get painted.
  // The callback should not get called synchronously.
  pepper_plugin()->Device2DFlushContext(npp(), &context, &FlushCalled, NULL);
  view_->didInvalidateRect(WebKit::WebRect(0, 0, 100, 100));
  EXPECT_TRUE(flush_calls_.empty());

  // Run the message loop which should process the pending paints, there should
  // still be no callbacks since the stuff hasn't been copied to the screen,
  // but there should be a paint message sent to the browser.
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(flush_calls_.empty());
  EXPECT_TRUE(render_thread_.sink().GetFirstMessageMatching(
      ViewHostMsg_UpdateRect::ID));

  // Send a paint ACK, this should trigger the callback.
  view_->OnMessageReceived(ViewMsg_UpdateRect_ACK(view_->routing_id()));
  EXPECT_EQ(1u, flush_calls_.size());
}
#endif

TEST_F(PepperDeviceTest, AudioInit) {
  NPDeviceContextAudioConfig config;
  config.sampleRate = NPAudioSampleRate44100Hz;
  config.sampleType = NPAudioSampleTypeInt16;
  config.outputChannelMap = NPAudioChannelStereo;
  config.callback = &AudioCallback;
  config.userData = this;
  NPDeviceContextAudio context;
  EXPECT_EQ(NPERR_NO_ERROR,
            pepper_plugin()->DeviceAudioInitializeContext(&config, &context));
  EXPECT_TRUE(render_thread_.sink().GetFirstMessageMatching(
      ViewHostMsg_CreateAudioStream::ID));
  EXPECT_EQ(NPERR_NO_ERROR,
            pepper_plugin()->DeviceAudioDestroyContext(&context));
  EXPECT_TRUE(render_thread_.sink().GetFirstMessageMatching(
      ViewHostMsg_CloseAudioStream::ID));
}

