// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "extensions/shell/test/shell_apitest.h"
#if defined(OS_CHROMEOS)
#include "chromeos/audio/audio_devices_pref_handler_stub.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cras_audio_client.h"
#endif
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

#if defined(OS_CHROMEOS)
using chromeos::AudioDevice;
using chromeos::AudioDeviceList;
using chromeos::AudioNode;
using chromeos::AudioNodeList;

const uint64_t kJabraSpeaker1Id = 30001;
const uint64_t kJabraSpeaker2Id = 30002;
const uint64_t kHDMIOutputId = 30003;
const uint64_t kJabraMic1Id = 40001;
const uint64_t kJabraMic2Id = 40002;
const uint64_t kWebcamMicId = 40003;

const AudioNode kJabraSpeaker1(false,
                               kJabraSpeaker1Id,
                               "Jabra Speaker",
                               "USB",
                               "Jabra Speaker 1",
                               false,
                               0);

const AudioNode kJabraSpeaker2(false,
                               kJabraSpeaker2Id,
                               "Jabra Speaker",
                               "USB",
                               "Jabra Speaker 2",
                               false,
                               0);

const AudioNode kHDMIOutput(false,
                            kHDMIOutputId,
                            "HDMI output",
                            "HDMI",
                            "HDA Intel MID",
                            false,
                            0);

const AudioNode
    kJabraMic1(true, kJabraMic1Id, "Jabra Mic", "USB", "Jabra Mic 1", false, 0);

const AudioNode
    kJabraMic2(true, kJabraMic2Id, "Jabra Mic", "USB", "Jabra Mic 2", false, 0);

const AudioNode kUSBCameraMic(true,
                              kWebcamMicId,
                              "Webcam Mic",
                              "USB",
                              "Logitech Webcam",
                              false,
                              0);

class AudioApiTest : public ShellApiTest {
 public:
  AudioApiTest() : cras_audio_handler_(NULL) {}
  ~AudioApiTest() override {}

  void SetUpCrasAudioHandlerWithTestingNodes(const AudioNodeList& audio_nodes) {
    chromeos::DBusThreadManager* dbus_manager =
        chromeos::DBusThreadManager::Get();
    DCHECK(dbus_manager);
    chromeos::FakeCrasAudioClient* fake_cras_audio_client =
        static_cast<chromeos::FakeCrasAudioClient*>(
            dbus_manager->GetCrasAudioClient());
    fake_cras_audio_client->SetAudioNodesAndNotifyObserversForTesting(
        audio_nodes);
    cras_audio_handler_ = chromeos::CrasAudioHandler::Get();
    DCHECK(cras_audio_handler_);
    message_loop_.RunUntilIdle();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  chromeos::CrasAudioHandler* cras_audio_handler_;  // Not owned.
};

IN_PROC_BROWSER_TEST_F(AudioApiTest, Audio) {
  // Set up the audio nodes for testing.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kJabraSpeaker1);
  audio_nodes.push_back(kJabraSpeaker2);
  audio_nodes.push_back(kHDMIOutput);
  audio_nodes.push_back(kJabraMic1);
  audio_nodes.push_back(kJabraMic2);
  audio_nodes.push_back(kUSBCameraMic);
  SetUpCrasAudioHandlerWithTestingNodes(audio_nodes);

  EXPECT_TRUE(RunAppTest("api_test/audio")) << message_;
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnLevelChangedOutputDevice) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kJabraSpeaker1);
  audio_nodes.push_back(kHDMIOutput);
  SetUpCrasAudioHandlerWithTestingNodes(audio_nodes);

  // Verify the jabra speaker is the active output device.
  AudioDevice device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(&device));
  EXPECT_EQ(device.id, kJabraSpeaker1.id);

  // Loads background app.
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");
  ASSERT_TRUE(LoadApp("api_test/audio/volume_change"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Change output device volume.
  const int kVolume = 60;
  cras_audio_handler_->SetOutputVolumePercent(kVolume);

  // Verify the output volume is changed to the designated value.
  EXPECT_EQ(kVolume, cras_audio_handler_->GetOutputVolumePercent());
  EXPECT_EQ(kVolume,
            cras_audio_handler_->GetOutputVolumePercentForDevice(device.id));

  // Verify the background app got the OnOutputNodeVolumeChanged event
  // with the expected node id and volume value.
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", result_listener.message());
}
#endif  // OS_CHROMEOS

}  // namespace extensions
