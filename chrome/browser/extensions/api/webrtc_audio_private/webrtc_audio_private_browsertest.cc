// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/web_contents.h"
#include "media/audio/audio_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::JSONWriter;
using content::RenderViewHost;
using content::WebContents;
using media::AudioDeviceNames;
using media::AudioManager;

namespace extensions {

using extension_function_test_utils::RunFunctionAndReturnError;
using extension_function_test_utils::RunFunctionAndReturnSingleResult;

class WebrtcAudioPrivateTest : public ExtensionApiTest {
 public:
  WebrtcAudioPrivateTest() : enumeration_event_(false, false) {
  }

 protected:
  std::string InvokeGetActiveSink(int tab_id) {
    ListValue parameters;
    parameters.AppendInteger(tab_id);
    std::string parameter_string;
    JSONWriter::Write(&parameters, &parameter_string);

    scoped_refptr<WebrtcAudioPrivateGetActiveSinkFunction> function =
        new WebrtcAudioPrivateGetActiveSinkFunction();
    scoped_ptr<base::Value> result(
        RunFunctionAndReturnSingleResult(function.get(),
                                         parameter_string,
                                         browser()));
    ListValue* list = NULL;
    std::string device_id;
    result->GetAsList(&list);
    list->GetString(0, &device_id);
    return device_id;
  }

  // Synchronously (from the calling thread's point of view) runs the
  // given enumeration function on the device thread. On return,
  // |device_names| has been filled with the device names resulting
  // from that call.
  void GetAudioDeviceNames(
      void (AudioManager::*EnumerationFunc)(AudioDeviceNames*),
      AudioDeviceNames* device_names) {
    AudioManager* audio_manager = AudioManager::Get();

    if (!audio_manager->GetMessageLoop()->BelongsToCurrentThread()) {
      audio_manager->GetMessageLoop()->PostTask(
          FROM_HERE,
          base::Bind(&WebrtcAudioPrivateTest::GetAudioDeviceNames, this,
                     EnumerationFunc, device_names));
      enumeration_event_.Wait();
    } else {
      (audio_manager->*EnumerationFunc)(device_names);
      enumeration_event_.Signal();
    }
  }

  // Event used to signal completion of enumeration.
  base::WaitableEvent enumeration_event_;
};

IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, GetSinks) {
  AudioDeviceNames devices;
  GetAudioDeviceNames(&AudioManager::GetAudioOutputDeviceNames, &devices);

  scoped_refptr<WebrtcAudioPrivateGetSinksFunction> function =
      new WebrtcAudioPrivateGetSinksFunction();
  scoped_ptr<base::Value> result(
      RunFunctionAndReturnSingleResult(function.get(), "[]", browser()));
  base::ListValue* parameter_list = NULL;
  result->GetAsList(&parameter_list);
  base::ListValue* sink_list = NULL;
  parameter_list->GetList(0, &sink_list);

  std::string result_string;
  JSONWriter::Write(result.get(), &result_string);
  VLOG(2) << result_string;

  EXPECT_EQ(devices.size(), sink_list->GetSize());

  // Iterate through both lists in lockstep and compare. The order
  // should be identical.
  size_t ix = 0;
  AudioDeviceNames::const_iterator it = devices.begin();
  for (; ix < sink_list->GetSize() && it != devices.end();
       ++ix, ++it) {
    base::DictionaryValue* dict = NULL;
    sink_list->GetDictionary(ix, &dict);
    std::string sink_id;
    dict->GetString("sinkId", &sink_id);
    EXPECT_EQ(it->unique_id, sink_id);
    std::string sink_label;
    dict->GetString("sinkLabel", &sink_label);
    EXPECT_EQ(it->device_name, sink_label);

    // TODO(joi): Verify the contents of these once we start actually
    // filling them in.
    EXPECT_TRUE(dict->HasKey("isDefault"));
    EXPECT_TRUE(dict->HasKey("isReady"));
    EXPECT_TRUE(dict->HasKey("sampleRate"));
  }
}

// This exercises the case where you have a tab with no active media
// stream and try to retrieve the currently active audio sink.
IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, GetActiveSinkNoMediaStream) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(tab);
  base::ListValue parameters;
  parameters.AppendInteger(tab_id);
  std::string parameter_string;
  JSONWriter::Write(&parameters, &parameter_string);

  scoped_refptr<WebrtcAudioPrivateGetActiveSinkFunction> function =
      new WebrtcAudioPrivateGetActiveSinkFunction();
  scoped_ptr<base::Value> result(
      RunFunctionAndReturnSingleResult(function.get(),
                                       parameter_string,
                                       browser()));

  std::string result_string;
  JSONWriter::Write(result.get(), &result_string);
  EXPECT_EQ("[\"\"]", result_string);
}

// This exercises the case where you have a tab with no active media
// stream and try to set the audio sink.
IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, SetActiveSinkNoMediaStream) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(tab);
  ListValue parameters;
  parameters.AppendInteger(tab_id);
  parameters.AppendString("no such id");
  std::string parameter_string;
  JSONWriter::Write(&parameters, &parameter_string);

  scoped_refptr<WebrtcAudioPrivateSetActiveSinkFunction> function =
      new WebrtcAudioPrivateSetActiveSinkFunction();
  std::string error(RunFunctionAndReturnError(function.get(),
                                              parameter_string,
                                              browser()));
  EXPECT_EQ(base::StringPrintf("No active stream for tab with id: %d.", tab_id),
            error);
}

// Used by the test below to wait until audio is playing.
static void OnAudioControllers(
    bool* audio_playing,
    const RenderViewHost::AudioOutputControllerList& list) {
  if (!list.empty())
    *audio_playing = true;
}

// TODO(joi): Currently this needs to be run manually. See if we can
// enable this on bots.
IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest,
                       DISABLED_GetAndSetWithMediaStream) {
  // First get the list of output devices, so that we can (if
  // available) set the active device to a device other than the one
  // it starts as. This function is not threadsafe and is normally
  // called only from the audio IO thread, but we know no other code
  // is currently running so we call it directly.
  AudioDeviceNames devices;
  GetAudioDeviceNames(&AudioManager::GetAudioOutputDeviceNames, &devices);

  ASSERT_TRUE(StartEmbeddedTestServer());

  // Open a normal page that uses an audio sink.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL(embedded_test_server()->GetURL("/extensions/loop_audio.html")));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(tab);

  // Wait for audio to start playing. We gate this on there being one
  // or more AudioOutputController objects for our tab.
  bool audio_playing = false;
  for (size_t remaining_tries = 50; remaining_tries > 0; --remaining_tries) {
    tab->GetRenderViewHost()->GetAudioOutputControllers(
        base::Bind(OnAudioControllers, &audio_playing));
    base::MessageLoop::current()->RunUntilIdle();
    if (audio_playing)
      break;

    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }

  if (!audio_playing)
    FAIL() << "Audio did not start playing within ~5 seconds.";

  std::string current_device = InvokeGetActiveSink(tab_id);
  VLOG(2) << "Before setting, current device: " << current_device;
  EXPECT_NE("", current_device);

  // Try to find a different device in the list of output
  // devices. Even if we don't find one, we still go through the
  // motions of setting the active device to the same device ID as
  // before, to exercise the code paths involved.
  std::string target_device(current_device);
  for (AudioDeviceNames::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if (it->unique_id != current_device) {
      target_device = it->unique_id;
      VLOG(2) << "Found different device ID to set to: " << target_device;
      break;
    }
  }

  ListValue parameters;
  parameters.AppendInteger(tab_id);
  parameters.AppendString(target_device);
  std::string parameter_string;
  JSONWriter::Write(&parameters, &parameter_string);

  scoped_refptr<WebrtcAudioPrivateSetActiveSinkFunction> function =
      new WebrtcAudioPrivateSetActiveSinkFunction();
  scoped_ptr<base::Value> result(
      RunFunctionAndReturnSingleResult(function.get(),
                                       parameter_string,
                                       browser()));
  // The function was successful if the above invocation doesn't
  // fail. Just for kicks, also check that it returns no result.
  EXPECT_EQ(NULL, result.get());

  current_device = InvokeGetActiveSink(tab_id);
  VLOG(2) << "After setting to " << target_device
          << ", current device is " << current_device;
  EXPECT_EQ(target_device, current_device);
}

IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, GetAssociatedSink) {
  // Get the list of input devices. We can cheat in the unit test and
  // run this on the main thread since nobody else will be running at
  // the same time.
  AudioDeviceNames devices;
  GetAudioDeviceNames(&AudioManager::GetAudioInputDeviceNames, &devices);

  // Try to get an associated sink for each source.
  for (AudioDeviceNames::const_iterator device = devices.begin();
       device != devices.end();
       ++device) {
    std::string raw_source_id = device->unique_id;
    VLOG(2) << "Trying to find associated sink for device " << raw_source_id;
    GURL origin(GURL("http://www.google.com/").GetOrigin());
    std::string source_id_in_origin =
        content::GetHMACForMediaDeviceID(origin, raw_source_id);

    ListValue parameters;
    parameters.AppendString(origin.spec());
    parameters.AppendString(source_id_in_origin);
    std::string parameter_string;
    JSONWriter::Write(&parameters, &parameter_string);

    scoped_refptr<WebrtcAudioPrivateGetAssociatedSinkFunction> function =
        new WebrtcAudioPrivateGetAssociatedSinkFunction();
    scoped_ptr<base::Value> result(
        RunFunctionAndReturnSingleResult(function.get(),
                                         parameter_string,
                                         browser()));
    std::string result_string;
    JSONWriter::Write(result.get(), &result_string);
    VLOG(2) << "Results: " << result_string;
  }
}

IN_PROC_BROWSER_TEST_F(WebrtcAudioPrivateTest, TriggerEvent) {
  WebrtcAudioPrivateEventService* service =
      WebrtcAudioPrivateEventService::GetFactoryInstance()->GetForProfile(
          profile());

  // Just trigger, without any extension listening.
  service->OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);

  // Now load our test extension and do it again.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("webrtc_audio_private_event_listener"));
  service->OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);

  // Check that the extension got the notification.
  std::string result = ExecuteScriptInBackgroundPage(extension->id(),
                                                     "reportIfGot()");
  EXPECT_EQ("true", result);
}

}  // namespace extensions
