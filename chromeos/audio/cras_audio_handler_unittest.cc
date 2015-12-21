// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/cras_audio_handler.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/audio/audio_devices_pref_handler_stub.h"
#include "chromeos/dbus/audio_node.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cras_audio_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

const uint64_t kInternalSpeakerId = 10001;
const uint64_t kHeadphoneId = 10002;
const uint64_t kInternalMicId = 10003;
const uint64_t kUSBMicId = 10004;
const uint64_t kBluetoothHeadsetId = 10005;
const uint64_t kHDMIOutputId = 10006;
const uint64_t kUSBHeadphoneId1 = 10007;
const uint64_t kUSBHeadphoneId2 = 10008;
const uint64_t kMicJackId = 10009;
const uint64_t kKeyboardMicId = 10010;
const uint64_t kOtherTypeOutputId = 90001;
const uint64_t kOtherTypeInputId = 90002;
const uint64_t kUSBJabraSpeakerOutputId1 = 90003;
const uint64_t kUSBJabraSpeakerOutputId2 = 90004;
const uint64_t kUSBJabraSpeakerInputId1 = 90005;
const uint64_t kUSBJabraSpeakerInputId2 = 90006;
const uint64_t kUSBCameraInputId = 90007;

const AudioNode kInternalSpeaker(
    false,
    kInternalSpeakerId,
    "Fake Speaker",
    "INTERNAL_SPEAKER",
    "Speaker",
    false,
    0
);

const AudioNode kHeadphone(
    false,
    kHeadphoneId,
    "Fake Headphone",
    "HEADPHONE",
    "Headphone",
    false,
    0
);

const AudioNode kInternalMic(
    true,
    kInternalMicId,
    "Fake Mic",
    "INTERNAL_MIC",
    "Internal Mic",
    false,
    0
);

const AudioNode kMicJack(
    true,
    kMicJackId,
    "Fake Mic Jack",
    "MIC",
    "Mic Jack",
    false,
    0
);

const AudioNode kUSBMic(
    true,
    kUSBMicId,
    "Fake USB Mic",
    "USB",
    "USB Microphone",
    false,
    0
);

const AudioNode kKeyboardMic(
    true,
    kKeyboardMicId,
    "Fake Keyboard Mic",
    "KEYBOARD_MIC",
    "Keyboard Mic",
    false,
    0
);

const AudioNode kOtherTypeOutput(
    false,
    kOtherTypeOutputId,
    "Output Device",
    "SOME_OTHER_TYPE",
    "Other Type Output Device",
    false,
    0
);

const AudioNode kOtherTypeInput(
    true,
    kOtherTypeInputId,
    "Input Device",
    "SOME_OTHER_TYPE",
    "Other Type Input Device",
    false,
    0
);

const AudioNode kBluetoothHeadset(false,
                                  kBluetoothHeadsetId,
                                  "Bluetooth Headset",
                                  "BLUETOOTH",
                                  "Bluetooth Headset 1",
                                  false,
                                  0);

const AudioNode kHDMIOutput(false,
                            kHDMIOutputId,
                            "HDMI output",
                            "HDMI",
                            "HDMI output",
                            false,
                            0);

const AudioNode kUSBHeadphone1(false,
                               kUSBHeadphoneId1,
                               "USB Headphone",
                               "USB",
                               "USB Headphone 1",
                               false,
                               0);

const AudioNode kUSBHeadphone2(false,
                               kUSBHeadphoneId2,
                               "USB Headphone",
                               "USB",
                               "USB Headphone 1",
                               false,
                               0);

const AudioNode kUSBJabraSpeakerOutput1(false,
                                        kUSBJabraSpeakerOutputId1,
                                        "Jabra Speaker 1",
                                        "USB",
                                        "Jabra Speaker 1",
                                        false,
                                        0);

const AudioNode kUSBJabraSpeakerOutput2(false,
                                        kUSBJabraSpeakerOutputId2,
                                        "Jabra Speaker 2",
                                        "USB",
                                        "Jabra Speaker 2",
                                        false,
                                        0);

const AudioNode kUSBJabraSpeakerInput1(true,
                                       kUSBJabraSpeakerInputId1,
                                       "Jabra Speaker 1",
                                       "USB",
                                       "Jabra Speaker",
                                       false,
                                       0);

const AudioNode kUSBJabraSpeakerInput2(true,
                                       kUSBJabraSpeakerInputId2,
                                       "Jabra Speaker 2",
                                       "USB",
                                       "Jabra Speaker 2",
                                       false,
                                       0);

const AudioNode kUSBCameraInput(true,
                                kUSBCameraInputId,
                                "USB Camera",
                                "USB",
                                "USB Camera",
                                false,
                                0);

class TestObserver : public chromeos::CrasAudioHandler::AudioObserver {
 public:
  TestObserver()
      : active_output_node_changed_count_(0),
        active_input_node_changed_count_(0),
        audio_nodes_changed_count_(0),
        output_mute_changed_count_(0),
        input_mute_changed_count_(0),
        output_volume_changed_count_(0),
        input_gain_changed_count_(0),
        output_mute_by_system_(false) {}

  int active_output_node_changed_count() const {
    return active_output_node_changed_count_;
  }

  void reset_active_output_node_changed_count() {
    active_output_node_changed_count_ = 0;
  }

  int active_input_node_changed_count() const {
    return active_input_node_changed_count_;
  }

  void reset_active_input_node_changed_count() {
    active_input_node_changed_count_ = 0;
  }

  int audio_nodes_changed_count() const {
    return audio_nodes_changed_count_;
  }

  int output_mute_changed_count() const {
    return output_mute_changed_count_;
  }

  void reset_output_mute_changed_count() { input_mute_changed_count_ = 0; }

  int input_mute_changed_count() const {
    return input_mute_changed_count_;
  }

  int output_volume_changed_count() const {
    return output_volume_changed_count_;
  }

  int input_gain_changed_count() const {
    return input_gain_changed_count_;
  }

  bool output_mute_by_system() const { return output_mute_by_system_; }

  ~TestObserver() override {}

 protected:
  // chromeos::CrasAudioHandler::AudioObserver overrides.
  void OnActiveOutputNodeChanged() override {
    ++active_output_node_changed_count_;
  }

  void OnActiveInputNodeChanged() override {
    ++active_input_node_changed_count_;
  }

  void OnAudioNodesChanged() override { ++audio_nodes_changed_count_; }

  void OnOutputMuteChanged(bool /* mute_on */, bool system_adjust) override {
    ++output_mute_changed_count_;
    output_mute_by_system_ = system_adjust;
  }

  void OnInputMuteChanged(bool /* mute_on */) override {
    ++input_mute_changed_count_;
  }

  void OnOutputNodeVolumeChanged(uint64_t /* node_id */,
                                 int /* volume */) override {
    ++output_volume_changed_count_;
  }

  void OnInputNodeGainChanged(uint64_t /* node_id */, int /* gain */) override {
    ++input_gain_changed_count_;
  }

 private:
  int active_output_node_changed_count_;
  int active_input_node_changed_count_;
  int audio_nodes_changed_count_;
  int output_mute_changed_count_;
  int input_mute_changed_count_;
  int output_volume_changed_count_;
  int input_gain_changed_count_;
  bool output_mute_by_system_;  // output mute state adjusted by system.

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class CrasAudioHandlerTest : public testing::Test {
 public:
  CrasAudioHandlerTest() : cras_audio_handler_(NULL),
                           fake_cras_audio_client_(NULL) {
  }
  ~CrasAudioHandlerTest() override {}

  void SetUp() override {}

  void TearDown() override {
    cras_audio_handler_->RemoveAudioObserver(test_observer_.get());
    test_observer_.reset();
    CrasAudioHandler::Shutdown();
    audio_pref_handler_ = NULL;
    DBusThreadManager::Shutdown();
  }

  void SetUpCrasAudioHandler(const AudioNodeList& audio_nodes) {
    DBusThreadManager::Initialize();
    fake_cras_audio_client_ = static_cast<FakeCrasAudioClient*>(
        DBusThreadManager::Get()->GetCrasAudioClient());
    fake_cras_audio_client_->SetAudioNodesForTesting(audio_nodes);
    audio_pref_handler_ = new AudioDevicesPrefHandlerStub();
    CrasAudioHandler::Initialize(audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_.reset(new TestObserver);
    cras_audio_handler_->AddAudioObserver(test_observer_.get());
    message_loop_.RunUntilIdle();
  }

  void SetUpCrasAudioHandlerWithPrimaryActiveNode(
      const AudioNodeList& audio_nodes,
      const AudioNode& primary_active_node) {
    DBusThreadManager::Initialize();
    fake_cras_audio_client_ = static_cast<FakeCrasAudioClient*>(
        DBusThreadManager::Get()->GetCrasAudioClient());
    fake_cras_audio_client_->SetAudioNodesForTesting(audio_nodes);
    fake_cras_audio_client_->SetActiveOutputNode(primary_active_node.id),
        audio_pref_handler_ = new AudioDevicesPrefHandlerStub();
    CrasAudioHandler::Initialize(audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
    test_observer_.reset(new TestObserver);
    cras_audio_handler_->AddAudioObserver(test_observer_.get());
    message_loop_.RunUntilIdle();
  }

  void ChangeAudioNodes(const AudioNodeList& audio_nodes) {
    fake_cras_audio_client_->SetAudioNodesAndNotifyObserversForTesting(
        audio_nodes);
    message_loop_.RunUntilIdle();
  }

  const AudioDevice* GetDeviceFromId(uint64_t id) {
    return cras_audio_handler_->GetDeviceFromId(id);
  }

  int GetActiveDeviceCount() const {
    int num_active_nodes = 0;
    AudioDeviceList audio_devices;
    cras_audio_handler_->GetAudioDevices(&audio_devices);
    for (size_t i = 0; i < audio_devices.size(); ++i) {
      if (audio_devices[i].active)
        ++num_active_nodes;
    }
    return num_active_nodes;
  }

  void SetActiveHDMIRediscover() {
    cras_audio_handler_->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
  }

  void SetHDMIRediscoverGracePeriodDuration(int duration_in_ms) {
    cras_audio_handler_->SetHDMIRediscoverGracePeriodForTesting(duration_in_ms);
  }

  bool IsDuringHDMIRediscoverGracePeriod() {
    return cras_audio_handler_->hdmi_rediscovering();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  CrasAudioHandler* cras_audio_handler_;  // Not owned.
  FakeCrasAudioClient* fake_cras_audio_client_;  // Not owned.
  scoped_ptr<TestObserver> test_observer_;
  scoped_refptr<AudioDevicesPrefHandlerStub> audio_pref_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrasAudioHandlerTest);
};

class HDMIRediscoverWaiter {
 public:
  HDMIRediscoverWaiter(CrasAudioHandlerTest* cras_audio_handler_test,
                       int grace_period_duration_in_ms)
      : cras_audio_handler_test_(cras_audio_handler_test),
        grace_period_duration_in_ms_(grace_period_duration_in_ms) {}

  void WaitUntilTimeOut(int wait_duration_in_ms) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(wait_duration_in_ms));
    run_loop.Run();
  }

  void CheckHDMIRediscoverGracePeriodEnd(const base::Closure& quit_loop_func) {
    if (!cras_audio_handler_test_->IsDuringHDMIRediscoverGracePeriod()) {
      quit_loop_func.Run();
      return;
    }
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&HDMIRediscoverWaiter::CheckHDMIRediscoverGracePeriodEnd,
                   base::Unretained(this), quit_loop_func),
        base::TimeDelta::FromMilliseconds(grace_period_duration_in_ms_ / 4));
  }

  void WaitUntilHDMIRediscoverGracePeriodEnd() {
    base::RunLoop run_loop;
    CheckHDMIRediscoverGracePeriodEnd(run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  CrasAudioHandlerTest* cras_audio_handler_test_;  // not owned
  int grace_period_duration_in_ms_;

  DISALLOW_COPY_AND_ASSIGN(HDMIRediscoverWaiter);
};

TEST_F(CrasAudioHandlerTest, InitializeWithOnlyDefaultAudioDevices) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kInternalMic);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the internal speaker has been selected as the active output.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());

  // Ensure the internal microphone has been selected as the active input.
  AudioDevice active_input;
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());
}

TEST_F(CrasAudioHandlerTest, InitializeWithAlternativeAudioDevices) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHeadphone);
  audio_nodes.push_back(kInternalMic);
  audio_nodes.push_back(kUSBMic);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the headphone has been selected as the active output.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone.id, active_output.id);
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Ensure the USB microphone has been selected as the active input.
  EXPECT_EQ(kUSBMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
}

TEST_F(CrasAudioHandlerTest, InitializeWithKeyboardMic) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kInternalMic);
  audio_nodes.push_back(kKeyboardMic);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_TRUE(cras_audio_handler_->HasKeyboardMic());

  // Verify the internal speaker has been selected as the active output.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());

  // Ensure the internal microphone has been selected as the active input,
  // not affected by keyboard mic.
  AudioDevice active_input;
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());
  const AudioDevice* keyboard_mic = GetDeviceFromId(kKeyboardMic.id);
  EXPECT_FALSE(keyboard_mic->active);
}

TEST_F(CrasAudioHandlerTest, SetKeyboardMicActive) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalMic);
  audio_nodes.push_back(kKeyboardMic);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_TRUE(cras_audio_handler_->HasKeyboardMic());

  // Ensure the internal microphone has been selected as the active input,
  // not affected by keyboard mic.
  AudioDevice active_input;
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());
  const AudioDevice* keyboard_mic = GetDeviceFromId(kKeyboardMic.id);
  EXPECT_FALSE(keyboard_mic->active);

  // Make keyboard mic active.
  cras_audio_handler_->SetKeyboardMicActive(true);
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  const AudioDevice* active_keyboard_mic = GetDeviceFromId(kKeyboardMic.id);
  EXPECT_TRUE(active_keyboard_mic->active);

  // Make keyboard mic inactive.
  cras_audio_handler_->SetKeyboardMicActive(false);
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  const AudioDevice* inactive_keyboard_mic = GetDeviceFromId(kKeyboardMic.id);
  EXPECT_FALSE(inactive_keyboard_mic->active);
}

TEST_F(CrasAudioHandlerTest, SwitchActiveOutputDevice) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHeadphone);
  SetUpCrasAudioHandler(audio_nodes);
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the initial active output device is headphone.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone.id, active_output.id);
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Switch the active output to internal speaker.
  AudioDevice internal_speaker(kInternalSpeaker);
  cras_audio_handler_->SwitchToDevice(internal_speaker, true);

  // Verify the active output is switched to internal speaker, and the
  // ActiveOutputNodeChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_F(CrasAudioHandlerTest, SwitchActiveInputDevice) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalMic);
  audio_nodes.push_back(kUSBMic);
  SetUpCrasAudioHandler(audio_nodes);
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the initial active input device is USB mic.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Switch the active input to internal mic.
  AudioDevice internal_mic(kInternalMic);
  cras_audio_handler_->SwitchToDevice(internal_mic, true);

  // Verify the active output is switched to internal speaker, and the active
  // ActiveInputNodeChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_F(CrasAudioHandlerTest, PlugHeadphone) {
  // Set up initial audio devices, only with internal speaker.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the internal speaker has been selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());

  // Plug the headphone.
  audio_nodes.clear();
  AudioNode internal_speaker(kInternalSpeaker);
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(kHeadphone);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to headphone and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone.id, active_output.id);
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, UnplugHeadphone) {
  // Set up initial audio devices, with internal speaker and headphone.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHeadphone);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the headphone has been selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone.id, active_output.id);
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Unplug the headphone.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalSpeaker);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size - 1, audio_devices.size());

  // Verify the active output device is switched to internal speaker and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, InitializeWithBluetoothHeadset) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kBluetoothHeadset);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the bluetooth headset has been selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kBluetoothHeadset.id, active_output.id);
  EXPECT_EQ(kBluetoothHeadset.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, ConnectAndDisconnectBluetoothHeadset) {
  // Initialize with internal speaker and headphone.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHeadphone);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the headphone is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone.id, active_output.id);
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Connect to bluetooth headset. Since it is plugged in later than
  // headphone, active output should be switched to it.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalSpeaker);
  AudioNode headphone(kHeadphone);
  headphone.plugged_time = 80000000;
  headphone.active = true;
  audio_nodes.push_back(headphone);
  AudioNode bluetooth_headset(kBluetoothHeadset);
  bluetooth_headset.plugged_time = 90000000;
  audio_nodes.push_back(bluetooth_headset);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to bluetooth headset, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kBluetoothHeadset.id, active_output.id);
  EXPECT_EQ(kBluetoothHeadset.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Disconnect bluetooth headset.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalSpeaker);
  headphone.active = false;
  audio_nodes.push_back(headphone);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to headphone, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone.id, active_output.id);
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, InitializeWithHDMIOutput) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHDMIOutput);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the HDMI device has been selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput.id, active_output.id);
  EXPECT_EQ(kHDMIOutput.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, ConnectAndDisconnectHDMIOutput) {
  // Initialize with internal speaker.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the internal speaker is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());

  // Connect to HDMI output.
  audio_nodes.clear();
  AudioNode internal_speaker(kInternalSpeaker);
  internal_speaker.active = true;
  internal_speaker.plugged_time = 80000000;
  audio_nodes.push_back(internal_speaker);
  AudioNode hdmi(kHDMIOutput);
  hdmi.plugged_time = 90000000;
  audio_nodes.push_back(hdmi);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to hdmi output, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput.id, active_output.id);
  EXPECT_EQ(kHDMIOutput.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Disconnect hdmi headset.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalSpeaker);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to internal speaker, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, HandleHeadphoneAndHDMIOutput) {
  // Initialize with internal speaker, headphone and HDMI output.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHeadphone);
  audio_nodes.push_back(kHDMIOutput);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the headphone is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone.id, active_output.id);
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Disconnect HDMI output.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHDMIOutput);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size - 1, audio_devices.size());

  // Verify the active output device is switched to HDMI output, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput.id, active_output.id);
  EXPECT_EQ(kHDMIOutput.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, InitializeWithUSBHeadphone) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kUSBHeadphone1);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the usb headphone has been selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1.id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, PlugAndUnplugUSBHeadphone) {
  // Initialize with internal speaker.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the internal speaker is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());

  // Plug in usb headphone
  audio_nodes.clear();
  AudioNode internal_speaker(kInternalSpeaker);
  internal_speaker.active = true;
  internal_speaker.plugged_time = 80000000;
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headphone(kUSBHeadphone1);
  usb_headphone.plugged_time = 90000000;
  audio_nodes.push_back(usb_headphone);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to usb headphone, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1.id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Unplug usb headphone.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalSpeaker);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to internal speaker, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, HandleMultipleUSBHeadphones) {
  // Initialize with internal speaker and one usb headphone.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kUSBHeadphone1);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the usb headphone is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1.id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Plug in another usb headphone.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalSpeaker);
  AudioNode usb_headphone_1(kUSBHeadphone1);
  usb_headphone_1.active = true;
  usb_headphone_1.plugged_time = 80000000;
  audio_nodes.push_back(usb_headphone_1);
  AudioNode usb_headphone_2(kUSBHeadphone2);
  usb_headphone_2.plugged_time = 90000000;
  audio_nodes.push_back(usb_headphone_2);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to the 2nd usb headphone, which
  // is plugged later, and ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone2.id, active_output.id);
  EXPECT_EQ(kUSBHeadphone2.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Unplug the 2nd usb headphone.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kUSBHeadphone1);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to the first usb headphone, and
  // ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1.id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, UnplugUSBHeadphonesWithActiveSpeaker) {
  // Initialize with internal speaker and one usb headphone.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kUSBHeadphone1);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the usb headphone is selected as the active output initially.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kUSBHeadphone1.id, active_output.id);
  EXPECT_EQ(kUSBHeadphone1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Plug in the headphone jack.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalSpeaker);
  AudioNode usb_headphone_1(kUSBHeadphone1);
  usb_headphone_1.active = true;
  usb_headphone_1.plugged_time = 80000000;
  audio_nodes.push_back(usb_headphone_1);
  AudioNode headphone_jack(kHeadphone);
  headphone_jack.plugged_time = 90000000;
  audio_nodes.push_back(headphone_jack);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active output device is switched to the headphone jack, which
  // is plugged later, and ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone.id, active_output.id);
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Select the speaker to be the active output device.
  AudioDevice internal_speaker(kInternalSpeaker);
  cras_audio_handler_->SwitchToDevice(internal_speaker, true);

  // Verify the active output is switched to internal speaker, and the
  // ActiveOutputNodeChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Unplug the usb headphone.
  audio_nodes.clear();
  AudioNode internal_speaker_node(kInternalSpeaker);
  internal_speaker_node.active = true;
  internal_speaker_node.plugged_time = 70000000;
  audio_nodes.push_back(internal_speaker_node);
  headphone_jack.active = false;
  audio_nodes.push_back(headphone_jack);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and one audio device is
  // removed.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device remains to be speaker.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
}

TEST_F(CrasAudioHandlerTest, OneActiveAudioOutputAfterLoginNewUserSession) {
  // This tests the case found with crbug.com/273271.
  // Initialize with internal speaker, bluetooth headphone and headphone jack
  // for a new chrome session after user signs out from the previous session.
  // Headphone jack is plugged in later than bluetooth headphone, but bluetooth
  // headphone is selected as the active output by user from previous user
  // session.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  AudioNode bluetooth_headphone(kBluetoothHeadset);
  bluetooth_headphone.active = true;
  bluetooth_headphone.plugged_time = 70000000;
  audio_nodes.push_back(bluetooth_headphone);
  AudioNode headphone_jack(kHeadphone);
  headphone_jack.plugged_time = 80000000;
  audio_nodes.push_back(headphone_jack);
  SetUpCrasAudioHandlerWithPrimaryActiveNode(audio_nodes, bluetooth_headphone);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the headphone jack is selected as the active output and all other
  // audio devices are not active.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone.id, active_output.id);
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id != kHeadphone.id)
      EXPECT_FALSE(audio_devices[i].active);
  }
}

TEST_F(CrasAudioHandlerTest, BluetoothSpeakerIdChangedOnFly) {
  // Initialize with internal speaker and bluetooth headset.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kBluetoothHeadset);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the bluetooth headset is selected as the active output and all other
  // audio devices are not active.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kBluetoothHeadset.id, active_output.id);
  EXPECT_EQ(kBluetoothHeadset.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Cras changes the bluetooth headset's id on the fly.
  audio_nodes.clear();
  AudioNode internal_speaker(kInternalSpeaker);
  internal_speaker.active = false;
  audio_nodes.push_back(internal_speaker);
  AudioNode bluetooth_headphone(kBluetoothHeadset);
  // Change bluetooth headphone id.
  bluetooth_headphone.id = kBluetoothHeadsetId + 20000;
  bluetooth_headphone.active = false;
  audio_nodes.push_back(bluetooth_headphone);
  ChangeAudioNodes(audio_nodes);

  // Verify NodesChanged event is fired, and the audio devices size is not
  // changed.
  audio_devices.clear();
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());

  // Verify ActiveOutputNodeChanged event is fired, and active device should be
  // bluetooth headphone.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(bluetooth_headphone.id, active_output.id);
}

TEST_F(CrasAudioHandlerTest, PlugUSBMic) {
  // Set up initial audio devices, only with internal mic.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalMic);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());

  // Verify the internal mic is selected as the active input.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());

  // Plug the USB Mic.
  audio_nodes.clear();
  AudioNode internal_mic(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(kUSBMic);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired and new audio device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active input device is switched to USB mic and
  // and ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
}

TEST_F(CrasAudioHandlerTest, UnplugUSBMic) {
  // Set up initial audio devices, with internal mic and USB Mic.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalMic);
  audio_nodes.push_back(kUSBMic);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the USB mic is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Unplug the USB Mic.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalMic);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, and one audio device is
  // removed.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size - 1, audio_devices.size());

  // Verify the active input device is switched to internal mic, and
  // and ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());
}

TEST_F(CrasAudioHandlerTest, PlugUSBMicNotAffectActiveOutput) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHeadphone);
  audio_nodes.push_back(kInternalMic);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the internal mic is selected as the active input.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());

  // Verify the headphone is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kHeadphoneId, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Switch the active output to internal speaker.
  AudioDevice internal_speaker(kInternalSpeaker);
  cras_audio_handler_->SwitchToDevice(internal_speaker, true);

  // Verify the active output is switched to internal speaker, and the
  // ActiveOutputNodeChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Plug the USB Mic.
  audio_nodes.clear();
  AudioNode internal_speaker_node(kInternalSpeaker);
  internal_speaker_node.active = true;
  audio_nodes.push_back(internal_speaker_node);
  audio_nodes.push_back(kHeadphone);
  AudioNode internal_mic(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(kUSBMic);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, one new device is added.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());

  // Verify the active input device is switched to USB mic, and
  // and ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Verify the active output device is not changed.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

TEST_F(CrasAudioHandlerTest, PlugHeadphoneAutoUnplugSpeakerWithActiveUSB) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kUSBHeadphone1);
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kInternalMic);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the internal mic is selected as the active input.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_input());

  // Verify the USB headphone is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kUSBHeadphoneId1,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Plug the headphone and auto-unplug internal speaker.
  audio_nodes.clear();
  AudioNode usb_headphone_node(kUSBHeadphone1);
  usb_headphone_node.active = true;
  audio_nodes.push_back(usb_headphone_node);
  AudioNode headphone_node(kHeadphone);
  headphone_node.plugged_time = 1000;
  audio_nodes.push_back(headphone_node);
  AudioNode internal_mic(kInternalMic);
  internal_mic.active = true;
  audio_nodes.push_back(internal_mic);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, with nodes count unchanged.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to headphone, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Unplug the headphone and internal speaker auto-plugs back.
  audio_nodes.clear();
  audio_nodes.push_back(kUSBHeadphone1);
  AudioNode internal_speaker_node(kInternalSpeaker);
  internal_speaker_node.plugged_time = 2000;
  audio_nodes.push_back(internal_speaker_node);
  audio_nodes.push_back(internal_mic);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, with nodes count unchanged.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched back to USB, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kUSBHeadphone1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Verify the active input device is not changed.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_F(CrasAudioHandlerTest, PlugMicAutoUnplugInternalMicWithActiveUSB) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kUSBHeadphone1);
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kUSBMic);
  audio_nodes.push_back(kInternalMic);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the internal mic is selected as the active input.
  EXPECT_EQ(0, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Verify the internal speaker is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kUSBHeadphoneId1,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Plug the headphone and mic, auto-unplug internal mic and speaker.
  audio_nodes.clear();
  AudioNode usb_headphone_node(kUSBHeadphone1);
  usb_headphone_node.active = true;
  audio_nodes.push_back(usb_headphone_node);
  AudioNode headphone_node(kHeadphone);
  headphone_node.plugged_time = 1000;
  audio_nodes.push_back(headphone_node);
  AudioNode usb_mic(kUSBMic);
  usb_mic.active = true;
  audio_nodes.push_back(usb_mic);
  AudioNode mic_jack(kMicJack);
  mic_jack.plugged_time = 1000;
  audio_nodes.push_back(mic_jack);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, with nodes count unchanged.
  EXPECT_EQ(1, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched to headphone, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Verify the active input device is switched to mic jack, and
  // an ActiveInputChanged event is fired.
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kMicJack.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Unplug the headphone and internal speaker auto-plugs back.
  audio_nodes.clear();
  audio_nodes.push_back(kUSBHeadphone1);
  AudioNode internal_speaker_node(kInternalSpeaker);
  internal_speaker_node.plugged_time = 2000;
  audio_nodes.push_back(internal_speaker_node);
  audio_nodes.push_back(kUSBMic);
  AudioNode internal_mic(kInternalMic);
  internal_mic.plugged_time = 2000;
  audio_nodes.push_back(internal_mic);
  ChangeAudioNodes(audio_nodes);

  // Verify the AudioNodesChanged event is fired, with nodes count unchanged.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the active output device is switched back to USB, and
  // an ActiveOutputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kUSBHeadphone1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Verify the active input device is switched back to USB mic, and
  // an ActiveInputChanged event is fired.
  EXPECT_EQ(2, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(kUSBMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
}

TEST_F(CrasAudioHandlerTest, MultipleNodesChangedSignalsOnPlugInHeadphone) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kBluetoothHeadset);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the bluetooth headset is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kBluetoothHeadsetId,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Plug in headphone, but fire NodesChanged signal twice.
  audio_nodes.clear();
  audio_nodes.push_back(kInternalSpeaker);
  AudioNode bluetooth_headset(kBluetoothHeadset);
  bluetooth_headset.plugged_time = 1000;
  bluetooth_headset.active = true;
  audio_nodes.push_back(bluetooth_headset);
  AudioNode headphone(kHeadphone);
  headphone.active = false;
  headphone.plugged_time = 2000;
  audio_nodes.push_back(headphone);
  ChangeAudioNodes(audio_nodes);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is set to headphone.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  EXPECT_LE(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(headphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(headphone.id, active_output.id);

  // Verfiy the audio devices data is consistent, i.e., the active output device
  // should be headphone.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id == kInternalSpeaker.id)
      EXPECT_FALSE(audio_devices[i].active);
    else if (audio_devices[i].id == bluetooth_headset.id)
      EXPECT_FALSE(audio_devices[i].active);
    else if (audio_devices[i].id == headphone.id)
      EXPECT_TRUE(audio_devices[i].active);
    else
      NOTREACHED();
  }
}

TEST_F(CrasAudioHandlerTest, MultipleNodesChangedSignalsOnPlugInUSBMic) {
  // Set up initial audio devices.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalMic);
  SetUpCrasAudioHandler(audio_nodes);
  const size_t init_nodes_size = audio_nodes.size();

  // Verify the audio devices size.
  EXPECT_EQ(0, test_observer_->audio_nodes_changed_count());
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());

  // Verify the internal mic is selected as the active output.
  EXPECT_EQ(0, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_FALSE(cras_audio_handler_->has_alternative_output());
  EXPECT_TRUE(audio_devices[0].active);

  // Plug in usb mic, but fire NodesChanged signal twice.
  audio_nodes.clear();
  AudioNode internal_mic(kInternalMic);
  internal_mic.active = true;
  internal_mic.plugged_time = 1000;
  audio_nodes.push_back(internal_mic);
  AudioNode usb_mic(kUSBMic);
  usb_mic.active = false;
  usb_mic.plugged_time = 2000;
  audio_nodes.push_back(usb_mic);
  ChangeAudioNodes(audio_nodes);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is set to headphone.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  EXPECT_LE(1, test_observer_->active_input_node_changed_count());
  EXPECT_EQ(usb_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Verfiy the audio devices data is consistent, i.e., the active input device
  // should be usb mic.
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size + 1, audio_devices.size());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id == kInternalMic.id)
      EXPECT_FALSE(audio_devices[i].active);
    else if (audio_devices[i].id == usb_mic.id)
      EXPECT_TRUE(audio_devices[i].active);
    else
      NOTREACHED();
  }
}

// This is the case of crbug.com/291303.
TEST_F(CrasAudioHandlerTest, MultipleNodesChangedSignalsOnSystemBoot) {
  // Set up audio handler with empty audio_nodes.
  AudioNodeList audio_nodes;
  SetUpCrasAudioHandler(audio_nodes);

  AudioNode internal_speaker(kInternalSpeaker);
  internal_speaker.active = false;
  AudioNode headphone(kHeadphone);
  headphone.active = false;
  AudioNode internal_mic(kInternalMic);
  internal_mic.active = false;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(headphone);
  audio_nodes.push_back(internal_mic);
  const size_t init_nodes_size = audio_nodes.size();

  // Simulate AudioNodesChanged signal being fired twice during system boot.
  ChangeAudioNodes(audio_nodes);
  ChangeAudioNodes(audio_nodes);

  // Verify the active output device is set to headphone.
  EXPECT_EQ(2, test_observer_->audio_nodes_changed_count());
  EXPECT_LE(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(headphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(headphone.id, active_output.id);

  // Verify the active input device id is set to internal mic.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Verfiy the audio devices data is consistent, i.e., the active output device
  // should be headphone, and the active input device should internal mic.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_nodes_size, audio_devices.size());
  for (size_t i = 0; i < audio_devices.size(); ++i) {
    if (audio_devices[i].id == internal_speaker.id)
      EXPECT_FALSE(audio_devices[i].active);
    else if (audio_devices[i].id == headphone.id)
      EXPECT_TRUE(audio_devices[i].active);
    else if (audio_devices[i].id == internal_mic.id)
      EXPECT_TRUE(audio_devices[i].active);
    else
      NOTREACHED();
  }
}

// This is the case of crbug.com/448924.
TEST_F(CrasAudioHandlerTest,
       TwoNodesChangedSignalsForLosingTowNodesOnOneUnplug) {
  // Set up audio handler with 4 audio_nodes.
  AudioNodeList audio_nodes;
  AudioNode internal_speaker(kInternalSpeaker);
  internal_speaker.active = false;
  AudioNode headphone(kHeadphone);
  headphone.active = false;
  AudioNode internal_mic(kInternalMic);
  internal_mic.active = false;
  AudioNode micJack(kMicJack);
  micJack.active = false;
  audio_nodes.push_back(internal_speaker);
  audio_nodes.push_back(headphone);
  audio_nodes.push_back(internal_mic);
  audio_nodes.push_back(micJack);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the headphone has been selected as the active output.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHeadphone.id, active_output.id);
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(active_output.active);
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Verify the mic Jack has been selected as the active input.
  EXPECT_EQ(micJack.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  const AudioDevice* active_input = GetDeviceFromId(micJack.id);
  EXPECT_TRUE(active_input->active);
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());

  // Simulate the nodes list in first NodesChanged signal, only headphone is
  // removed, other nodes remains the same.
  AudioNodeList changed_nodes_1;
  internal_speaker.active = false;
  changed_nodes_1.push_back(internal_speaker);
  internal_mic.active = false;
  changed_nodes_1.push_back(internal_mic);
  micJack.active = true;
  changed_nodes_1.push_back(micJack);

  // Simulate the nodes list in second NodesChanged signal, the micJac is
  // removed, but the internal_mic is inactive, which does not reflect the
  // active status set from the first NodesChanged signal since this was sent
  // before cras receives the SetActiveOutputNode from the first NodesChanged
  // handling.
  AudioNodeList changed_nodes_2;
  changed_nodes_2.push_back(internal_speaker);
  changed_nodes_2.push_back(internal_mic);

  // Simulate AudioNodesChanged signal being fired twice for unplug an audio
  // device with both input and output nodes on it.
  ChangeAudioNodes(changed_nodes_1);
  ChangeAudioNodes(changed_nodes_2);

  AudioDeviceList changed_devices;
  cras_audio_handler_->GetAudioDevices(&changed_devices);
  EXPECT_EQ(2u, changed_devices.size());

  // Verify the active output device is set to internal speaker.
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(internal_speaker.id, active_output.id);
  EXPECT_TRUE(active_output.active);

  // Verify the active input device id is set to internal mic.
  EXPECT_EQ(internal_mic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  const AudioDevice* changed_active_input = GetDeviceFromId(internal_mic.id);
  EXPECT_TRUE(changed_active_input->active);
}

TEST_F(CrasAudioHandlerTest, SetOutputMute) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_mute_changed_count());

  // Mute the device.
  cras_audio_handler_->SetOutputMute(true);

  // Verify the output is muted, OnOutputMuteChanged event is fired,
  // and mute value is saved in the preferences.
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());
  EXPECT_EQ(1, test_observer_->output_mute_changed_count());
  AudioDevice speaker(kInternalSpeaker);
  EXPECT_TRUE(audio_pref_handler_->GetMuteValue(speaker));

  // Unmute the device.
  cras_audio_handler_->SetOutputMute(false);

  // Verify the output is unmuted, OnOutputMuteChanged event is fired,
  // and mute value is saved in the preferences.
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
  EXPECT_EQ(2, test_observer_->output_mute_changed_count());
  EXPECT_FALSE(audio_pref_handler_->GetMuteValue(speaker));
}

TEST_F(CrasAudioHandlerTest, SetInputMute) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalMic);
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->input_mute_changed_count());

  // Mute the device.
  cras_audio_handler_->SetInputMute(true);

  // Verify the input is muted, OnInputMuteChanged event is fired.
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(1, test_observer_->input_mute_changed_count());

  // Unmute the device.
  cras_audio_handler_->SetInputMute(false);

  // Verify the input is unmuted, OnInputMuteChanged event is fired.
  EXPECT_FALSE(cras_audio_handler_->IsInputMuted());
  EXPECT_EQ(2, test_observer_->input_mute_changed_count());
}

TEST_F(CrasAudioHandlerTest, SetOutputVolumePercent) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->output_volume_changed_count());

  cras_audio_handler_->SetOutputVolumePercent(60);

  // Verify the output volume is changed to the designated value,
  // OnOutputNodeVolumeChanged event is fired, and the device volume value
  // is saved the preferences.
  const int kVolume = 60;
  EXPECT_EQ(kVolume, cras_audio_handler_->GetOutputVolumePercent());
  EXPECT_EQ(1, test_observer_->output_volume_changed_count());
  AudioDevice device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(&device));
  EXPECT_EQ(device.id, kInternalSpeaker.id);
  EXPECT_EQ(kVolume, audio_pref_handler_->GetOutputVolumeValue(&device));
}

TEST_F(CrasAudioHandlerTest, SetInputGainPercent) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalMic);
  SetUpCrasAudioHandler(audio_nodes);
  EXPECT_EQ(0, test_observer_->input_gain_changed_count());

  cras_audio_handler_->SetInputGainPercent(60);

  // Verify the input gain changed to the designated value,
  // OnInputNodeGainChanged event is fired, and the device gain value
  // is saved in the preferences.
  const int kGain = 60;
  EXPECT_EQ(kGain, cras_audio_handler_->GetInputGainPercent());
  EXPECT_EQ(1, test_observer_->input_gain_changed_count());
  AudioDevice internal_mic(kInternalMic);
  EXPECT_EQ(kGain, audio_pref_handler_->GetInputGainValue(&internal_mic));
}

TEST_F(CrasAudioHandlerTest, SetMuteForDevice) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHeadphone);
  audio_nodes.push_back(kInternalMic);
  audio_nodes.push_back(kUSBMic);
  SetUpCrasAudioHandler(audio_nodes);

  // Mute the active output device.
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  cras_audio_handler_->SetMuteForDevice(kHeadphone.id, true);

  // Verify the headphone is muted and mute value is saved in the preferences.
  EXPECT_TRUE(cras_audio_handler_->IsOutputMutedForDevice(kHeadphone.id));
  AudioDevice headphone(kHeadphone);
  EXPECT_TRUE(audio_pref_handler_->GetMuteValue(headphone));

  // Mute the non-active output device.
  cras_audio_handler_->SetMuteForDevice(kInternalSpeaker.id, true);

  // Verify the internal speaker is muted and mute value is saved in the
  // preferences.
  EXPECT_TRUE(cras_audio_handler_->IsOutputMutedForDevice(kInternalSpeaker.id));
  AudioDevice internal_speaker(kInternalSpeaker);
  EXPECT_TRUE(audio_pref_handler_->GetMuteValue(internal_speaker));

  // Mute the active input device.
  EXPECT_EQ(kUSBMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  cras_audio_handler_->SetMuteForDevice(kUSBMic.id, true);

  // Verify the USB Mic is muted.
  EXPECT_TRUE(cras_audio_handler_->IsInputMutedForDevice(kUSBMic.id));

  // Mute the non-active input device should be a no-op, see crbug.com/365050.
  cras_audio_handler_->SetMuteForDevice(kInternalMic.id, true);

  // Verify IsInputMutedForDevice returns false for non-active input device.
  EXPECT_FALSE(cras_audio_handler_->IsInputMutedForDevice(kInternalMic.id));
}

TEST_F(CrasAudioHandlerTest, SetVolumeGainPercentForDevice) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHeadphone);
  audio_nodes.push_back(kInternalMic);
  audio_nodes.push_back(kUSBMic);
  SetUpCrasAudioHandler(audio_nodes);

  // Set volume percent for active output device.
  const int kHeadphoneVolume = 30;
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  cras_audio_handler_->SetVolumeGainPercentForDevice(kHeadphone.id,
                                                     kHeadphoneVolume);

  // Verify the volume percent of headphone is set, and saved in preferences.
  EXPECT_EQ(kHeadphoneVolume,
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kHeadphone.id));
  AudioDevice headphone(kHeadphone);
  EXPECT_EQ(kHeadphoneVolume,
            audio_pref_handler_->GetOutputVolumeValue(&headphone));

  // Set volume percent for non-active output device.
  const int kSpeakerVolume = 60;
  cras_audio_handler_->SetVolumeGainPercentForDevice(kInternalSpeaker.id,
                                                     kSpeakerVolume);

  // Verify the volume percent of speaker is set, and saved in preferences.
  EXPECT_EQ(kSpeakerVolume,
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kInternalSpeaker.id));
  AudioDevice speaker(kInternalSpeaker);
  EXPECT_EQ(kSpeakerVolume,
            audio_pref_handler_->GetOutputVolumeValue(&speaker));

  // Set gain percent for active input device.
  const int kUSBMicGain = 30;
  EXPECT_EQ(kUSBMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  cras_audio_handler_->SetVolumeGainPercentForDevice(kUSBMic.id,
                                                     kUSBMicGain);

  // Verify the gain percent of USB mic is set, and saved in preferences.
  EXPECT_EQ(kUSBMicGain,
            cras_audio_handler_->GetOutputVolumePercentForDevice(kUSBMic.id));
  AudioDevice usb_mic(kHeadphone);
  EXPECT_EQ(kUSBMicGain,
            audio_pref_handler_->GetInputGainValue(&usb_mic));

  // Set gain percent for non-active input device.
  const int kInternalMicGain = 60;
  cras_audio_handler_->SetVolumeGainPercentForDevice(kInternalMic.id,
                                                     kInternalMicGain);

  // Verify the gain percent of internal mic is set, and saved in preferences.
  EXPECT_EQ(kInternalMicGain,
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kInternalMic.id));
  AudioDevice internal_mic(kInternalMic);
  EXPECT_EQ(kInternalMicGain,
            audio_pref_handler_->GetInputGainValue(&internal_mic));
}

TEST_F(CrasAudioHandlerTest, HandleOtherDeviceType) {
  const size_t kNumValidAudioDevices = 4;
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kOtherTypeOutput);
  audio_nodes.push_back(kInternalMic);
  audio_nodes.push_back(kOtherTypeInput);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(kNumValidAudioDevices, audio_devices.size());

  // Verify the internal speaker has been selected as the active output,
  // and the output device with some randown unknown type is handled gracefully.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());

  // Ensure the internal microphone has been selected as the active input,
  // and the input device with some random unknown type is handled gracefully.
  AudioDevice active_input;
  EXPECT_EQ(kInternalMic.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_input());
}

TEST_F(CrasAudioHandlerTest, ChangeActiveNodesHotrodInit) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kHDMIOutput);
  audio_nodes.push_back(kUSBJabraSpeakerOutput1);
  audio_nodes.push_back(kUSBJabraSpeakerOutput2);
  audio_nodes.push_back(kUSBJabraSpeakerInput1);
  audio_nodes.push_back(kUSBJabraSpeakerInput2);
  audio_nodes.push_back(kUSBCameraInput);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify only the 1st jabra speaker's output and input are selected as active
  // nodes by CrasAudioHandler.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(2, GetActiveDeviceCount());
  AudioDevice primary_active_device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(
      &primary_active_device));
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id, primary_active_device.id);
  EXPECT_EQ(kUSBJabraSpeakerInput1.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Set both jabra speakers's input and output nodes to active, this simulate
  // the call sent by hotrod initialization process.
  test_observer_->reset_active_output_node_changed_count();
  test_observer_->reset_active_input_node_changed_count();
  CrasAudioHandler::NodeIdList active_nodes;
  active_nodes.push_back(kUSBJabraSpeakerOutput1.id);
  active_nodes.push_back(kUSBJabraSpeakerOutput2.id);
  active_nodes.push_back(kUSBJabraSpeakerInput1.id);
  active_nodes.push_back(kUSBJabraSpeakerInput2.id);
  cras_audio_handler_->ChangeActiveNodes(active_nodes);

  // Verify both jabra speakers' input/output nodes are made active.
  // num_active_nodes = GetActiveDeviceCount();
  EXPECT_EQ(4, GetActiveDeviceCount());
  const AudioDevice* active_output_1 =
      GetDeviceFromId(kUSBJabraSpeakerOutput1.id);
  EXPECT_TRUE(active_output_1->active);
  const AudioDevice* active_output_2 =
      GetDeviceFromId(kUSBJabraSpeakerOutput2.id);
  EXPECT_TRUE(active_output_2->active);
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(
      &primary_active_device));
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id, primary_active_device.id);
  const AudioDevice* active_input_1 =
      GetDeviceFromId(kUSBJabraSpeakerInput1.id);
  EXPECT_TRUE(active_input_1->active);
  const AudioDevice* active_input_2 =
      GetDeviceFromId(kUSBJabraSpeakerInput2.id);
  EXPECT_TRUE(active_input_2->active);
  EXPECT_EQ(kUSBJabraSpeakerInput1.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Verify only 1 ActiveOutputNodeChanged notification has been sent out
  // by calling ChangeActiveNodes.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());

  // Verify all active devices are the not muted and their volume values are
  // the same.
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kUSBJabraSpeakerOutput1.id));
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kUSBJabraSpeakerOutput2.id));
  EXPECT_EQ(cras_audio_handler_->GetOutputVolumePercent(),
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kUSBJabraSpeakerOutput1.id));
  EXPECT_EQ(cras_audio_handler_->GetOutputVolumePercent(),
            cras_audio_handler_->GetOutputVolumePercentForDevice(
                kUSBJabraSpeakerOutput2.id));

  // Adjust the volume of output devices, verify all active nodes are set to
  // the same volume.
  cras_audio_handler_->SetOutputVolumePercent(25);
  EXPECT_EQ(25, cras_audio_handler_->GetOutputVolumePercent());
  EXPECT_EQ(25, cras_audio_handler_->GetOutputVolumePercentForDevice(
                    kUSBJabraSpeakerOutput1.id));
  EXPECT_EQ(25, cras_audio_handler_->GetOutputVolumePercentForDevice(
                    kUSBJabraSpeakerOutput2.id));
}

TEST_F(CrasAudioHandlerTest, ChangeActiveNodesHotrodInitWithCameraInputActive) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kHDMIOutput);
  audio_nodes.push_back(kUSBJabraSpeakerOutput1);
  audio_nodes.push_back(kUSBJabraSpeakerOutput2);
  audio_nodes.push_back(kUSBJabraSpeakerInput1);
  audio_nodes.push_back(kUSBJabraSpeakerInput2);
  // Make the camera input to be plugged in later than jabra's input.
  AudioNode usb_camera(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the 1st jabra speaker's output is selected as active output
  // node and camera's input is selected active input by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBCameraInput.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Set both jabra speakers's input and output nodes to active, this simulates
  // the call sent by hotrod initialization process.
  test_observer_->reset_active_output_node_changed_count();
  test_observer_->reset_active_input_node_changed_count();
  CrasAudioHandler::NodeIdList active_nodes;
  active_nodes.push_back(kUSBJabraSpeakerOutput1.id);
  active_nodes.push_back(kUSBJabraSpeakerOutput2.id);
  active_nodes.push_back(kUSBJabraSpeakerInput1.id);
  active_nodes.push_back(kUSBJabraSpeakerInput2.id);
  cras_audio_handler_->ChangeActiveNodes(active_nodes);

  // Verify both jabra speakers' input/output nodes are made active.
  // num_active_nodes = GetActiveDeviceCount();
  EXPECT_EQ(4, GetActiveDeviceCount());
  const AudioDevice* active_output_1 =
      GetDeviceFromId(kUSBJabraSpeakerOutput1.id);
  EXPECT_TRUE(active_output_1->active);
  const AudioDevice* active_output_2 =
      GetDeviceFromId(kUSBJabraSpeakerOutput2.id);
  EXPECT_TRUE(active_output_2->active);
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* active_input_1 =
      GetDeviceFromId(kUSBJabraSpeakerInput1.id);
  EXPECT_TRUE(active_input_1->active);
  const AudioDevice* active_input_2 =
      GetDeviceFromId(kUSBJabraSpeakerInput2.id);
  EXPECT_TRUE(active_input_2->active);
  EXPECT_EQ(kUSBJabraSpeakerInput1.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Verify only 1 ActiveOutputNodeChanged notification has been sent out
  // by calling ChangeActiveNodes.
  EXPECT_EQ(1, test_observer_->active_output_node_changed_count());
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
}

TEST_F(CrasAudioHandlerTest, ChangeActiveNodesWithFewerActives) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kHDMIOutput);
  audio_nodes.push_back(kUSBJabraSpeakerOutput1);
  audio_nodes.push_back(kUSBJabraSpeakerOutput2);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Set all three nodes to be active.
  CrasAudioHandler::NodeIdList active_nodes;
  active_nodes.push_back(kHDMIOutput.id);
  active_nodes.push_back(kUSBJabraSpeakerOutput1.id);
  active_nodes.push_back(kUSBJabraSpeakerOutput2.id);
  cras_audio_handler_->ChangeActiveNodes(active_nodes);

  // Verify all three nodes are active.
  EXPECT_EQ(3, GetActiveDeviceCount());
  const AudioDevice* active_output_1 = GetDeviceFromId(kHDMIOutput.id);
  EXPECT_TRUE(active_output_1->active);
  const AudioDevice* active_output_2 =
      GetDeviceFromId(kUSBJabraSpeakerOutput1.id);
  EXPECT_TRUE(active_output_2->active);
  const AudioDevice* active_output_3 =
      GetDeviceFromId(kUSBJabraSpeakerOutput2.id);
  EXPECT_TRUE(active_output_3->active);

  // Now call ChangeActiveDevices with only 2 nodes.
  active_nodes.clear();
  active_nodes.push_back(kUSBJabraSpeakerOutput1.id);
  active_nodes.push_back(kUSBJabraSpeakerOutput2.id);
  cras_audio_handler_->ChangeActiveNodes(active_nodes);

  // Verify only 2 nodes are active.
  EXPECT_EQ(2, GetActiveDeviceCount());
  const AudioDevice* output_1 = GetDeviceFromId(kHDMIOutput.id);
  EXPECT_FALSE(output_1->active);
  const AudioDevice* output_2 = GetDeviceFromId(kUSBJabraSpeakerOutput1.id);
  EXPECT_TRUE(output_2->active);
  const AudioDevice* output_3 = GetDeviceFromId(kUSBJabraSpeakerOutput2.id);
  EXPECT_TRUE(output_3->active);
}

TEST_F(CrasAudioHandlerTest, HotrodInitWithSingleJabra) {
  // Simulates the hotrod initializated with a single jabra device and
  // CrasAudioHandler selected jabra input/output as active devices.
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kHDMIOutput);
  audio_nodes.push_back(kUSBJabraSpeakerOutput1);
  audio_nodes.push_back(kUSBJabraSpeakerInput1);
  audio_nodes.push_back(kUSBCameraInput);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output and input are selected as active nodes
  // by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_F(CrasAudioHandlerTest,
       ChangeActiveNodesHotrodInitWithSingleJabraCameraPlugInLater) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kHDMIOutput);
  audio_nodes.push_back(kUSBJabraSpeakerOutput1);
  audio_nodes.push_back(kUSBJabraSpeakerInput1);
  AudioNode usb_camera(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output is selected as active output, and
  // camera's input is selected as active input by CrasAudioHandler
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBCameraInput.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Simulate hotrod app call to set jabra input as active device with only
  // jabra input node in the active node list, which does not conform to the
  // new SetActiveDevices protocol, but just show we can still handle it if
  // this happens.
  CrasAudioHandler::NodeIdList active_nodes;
  active_nodes.push_back(kUSBJabraSpeakerOutput1.id);
  active_nodes.push_back(kUSBJabraSpeakerInput1.id);
  cras_audio_handler_->ChangeActiveNodes(active_nodes);

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_F(CrasAudioHandlerTest,
       ChangeActiveNodesHotrodInitWithSingleJabraCameraPlugInLaterOldCall) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kHDMIOutput);
  audio_nodes.push_back(kUSBJabraSpeakerOutput1);
  audio_nodes.push_back(kUSBJabraSpeakerInput1);
  AudioNode usb_camera(kUSBCameraInput);
  usb_camera.plugged_time = 10000000;
  audio_nodes.push_back(usb_camera);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output is selected as active output, and
  // camera's input is selected as active input by CrasAudioHandler
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBCameraInput.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Simulate hotrod app call to set jabra input as active device with only
  // jabra input node in the active node list, which does not conform to the
  // new SetActiveDevices protocol, but just show we can still handle it if
  // this happens.
  CrasAudioHandler::NodeIdList active_nodes;
  active_nodes.push_back(kUSBJabraSpeakerInput1.id);
  cras_audio_handler_->ChangeActiveNodes(active_nodes);

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_F(CrasAudioHandlerTest,
       ChangeActiveNodesHotrodInitWithSingleJabraChangeOutput) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kHDMIOutput);
  audio_nodes.push_back(kUSBJabraSpeakerOutput1);
  audio_nodes.push_back(kUSBJabraSpeakerInput1);
  audio_nodes.push_back(kUSBCameraInput);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output and input are selected as active output
  // by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Simulate hotrod app call SetActiveDevices to change active output
  // with only complete list of active nodes passed in, which is the new
  // way of hotrod app.
  CrasAudioHandler::NodeIdList active_nodes;
  active_nodes.push_back(kHDMIOutput.id);
  active_nodes.push_back(kUSBJabraSpeakerInput1.id);
  cras_audio_handler_->ChangeActiveNodes(active_nodes);

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kHDMIOutput.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_F(CrasAudioHandlerTest,
       ChangeActiveNodesHotrodInitWithSingleJabraChangeOutputOldCall) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kHDMIOutput);
  audio_nodes.push_back(kUSBJabraSpeakerOutput1);
  audio_nodes.push_back(kUSBJabraSpeakerInput1);
  audio_nodes.push_back(kUSBCameraInput);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the jabra speaker's output and input are selected as active output
  // by CrasAudioHandler.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kUSBJabraSpeakerOutput1.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());

  // Simulate hotrod app call SetActiveDevices to change active output
  // with only a single active output nodes passed in, which is the old
  // way of hotrod app.
  CrasAudioHandler::NodeIdList active_nodes;
  active_nodes.push_back(kHDMIOutput.id);
  cras_audio_handler_->ChangeActiveNodes(active_nodes);

  // Verify the jabra speaker's output is selected as active output, and
  // jabra's input is selected as active input.
  EXPECT_EQ(2, GetActiveDeviceCount());
  EXPECT_EQ(kHDMIOutput.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_EQ(kUSBJabraSpeakerInput1.id,
            cras_audio_handler_->GetPrimaryActiveInputNode());
}

TEST_F(CrasAudioHandlerTest, NoMoreAudioInputDevices) {
  // Some device like chromebox does not have the internal input device. The
  // active devices should be reset when the user plugs a device and then
  // unplugs it to such device.

  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  SetUpCrasAudioHandler(audio_nodes);

  EXPECT_EQ(0ULL, cras_audio_handler_->GetPrimaryActiveInputNode());

  audio_nodes.push_back(kMicJack);
  ChangeAudioNodes(audio_nodes);

  EXPECT_EQ(kMicJack.id, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
  test_observer_->reset_active_input_node_changed_count();

  audio_nodes.pop_back();
  ChangeAudioNodes(audio_nodes);
  EXPECT_EQ(0ULL, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_EQ(1, test_observer_->active_input_node_changed_count());
}

// Test the case in which an HDMI output is plugged in with other higher
// priority
// output devices already plugged and user has manually selected an active
// output.
// The hotplug of hdmi output should not change user's selection of active
// device.
// crbug.com/447826.
TEST_F(CrasAudioHandlerTest, HotPlugHDMINotChangeActiveOutput) {
  AudioNodeList audio_nodes;
  AudioNode internal_speaker(kInternalSpeaker);
  audio_nodes.push_back(internal_speaker);
  AudioNode usb_headset(kUSBHeadphone1);
  usb_headset.plugged_time = 80000000;
  audio_nodes.push_back(usb_headset);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the audio devices size.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());

  // Verify the USB headset is selected as active output by default.
  EXPECT_EQ(usb_headset.id, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Manually set the active output to internal speaker.
  AudioDevice internal_output(kInternalSpeaker);
  cras_audio_handler_->SwitchToDevice(internal_output, true);

  // Verify the active output is switched to internal speaker.
  EXPECT_EQ(internal_speaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_LT(kInternalSpeaker.plugged_time, usb_headset.plugged_time);
  const AudioDevice* usb_device = GetDeviceFromId(usb_headset.id);
  EXPECT_FALSE(usb_device->active);

  // Plug in HDMI output.
  audio_nodes.clear();
  internal_speaker.active = true;
  audio_nodes.push_back(internal_speaker);
  usb_headset.active = false;
  audio_nodes.push_back(usb_headset);
  AudioNode hdmi(kHDMIOutput);
  hdmi.plugged_time = 90000000;
  audio_nodes.push_back(hdmi);
  ChangeAudioNodes(audio_nodes);

  // The active output should not change.
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
}

// Test the case in which the active device was set to inactive from cras after
// resuming from suspension state. See crbug.com/478968.
TEST_F(CrasAudioHandlerTest, ActiveNodeLostAfterResume) {
  AudioNodeList audio_nodes;
  EXPECT_FALSE(kHeadphone.active);
  audio_nodes.push_back(kHeadphone);
  EXPECT_FALSE(kHDMIOutput.active);
  audio_nodes.push_back(kHDMIOutput);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the headphone is selected as the active output.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* active_headphone = GetDeviceFromId(kHeadphone.id);
  EXPECT_EQ(kHeadphone.id, active_headphone->id);
  EXPECT_TRUE(active_headphone->active);

  // Simulate NodesChanged signal with headphone turning into inactive state,
  // and HDMI node removed.
  audio_nodes.clear();
  audio_nodes.push_back(kHeadphone);
  ChangeAudioNodes(audio_nodes);

  // Verify the headphone is set to active again.
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* headphone_resumed = GetDeviceFromId(kHeadphone.id);
  EXPECT_EQ(kHeadphone.id, headphone_resumed->id);
  EXPECT_TRUE(headphone_resumed->active);
}

// Test the case in which there are two NodesChanged signal for discovering
// output devices, and there is race between NodesChange and SetActiveOutput
// during this process. See crbug.com/478968.
TEST_F(CrasAudioHandlerTest, ActiveNodeLostDuringLoginSession) {
  AudioNodeList audio_nodes;
  EXPECT_FALSE(kHeadphone.active);
  audio_nodes.push_back(kHeadphone);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the headphone is selected as the active output.
  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(audio_nodes.size(), audio_devices.size());
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* active_headphone = GetDeviceFromId(kHeadphone.id);
  EXPECT_EQ(kHeadphone.id, active_headphone->id);
  EXPECT_TRUE(active_headphone->active);

  // Simulate NodesChanged signal with headphone turning into inactive state,
  // and add a new HDMI output node.
  audio_nodes.clear();
  audio_nodes.push_back(kHeadphone);
  audio_nodes.push_back(kHDMIOutput);
  ChangeAudioNodes(audio_nodes);

  // Verify the headphone is set to active again.
  EXPECT_EQ(kHeadphone.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  const AudioDevice* headphone_resumed = GetDeviceFromId(kHeadphone.id);
  EXPECT_EQ(kHeadphone.id, headphone_resumed->id);
  EXPECT_TRUE(headphone_resumed->active);
}

// This test HDMI output rediscovering case in crbug.com/503667.
TEST_F(CrasAudioHandlerTest, HDMIOutputRediscover) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHDMIOutput);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the HDMI device has been selected as the active output, and audio
  // output is not muted.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput.id, active_output.id);
  EXPECT_EQ(kHDMIOutput.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());

  // Trigger HDMI rediscovering grace period, and remove the HDMI node.
  const int grace_period_in_ms = 200;
  SetHDMIRediscoverGracePeriodDuration(grace_period_in_ms);
  SetActiveHDMIRediscover();
  AudioNodeList audio_nodes_lost_hdmi;
  audio_nodes_lost_hdmi.push_back(kInternalSpeaker);
  ChangeAudioNodes(audio_nodes_lost_hdmi);

  // Verify the active output is switched to internal speaker, it is not muted
  // by preference, but the system output is muted during the grace period.
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kInternalSpeaker.id));
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());

  // Re-attach the HDMI device after a little delay.
  HDMIRediscoverWaiter waiter(this, grace_period_in_ms);
  waiter.WaitUntilTimeOut(grace_period_in_ms / 4);
  ChangeAudioNodes(audio_nodes);

  // After HDMI re-discover grace period, verify HDMI output is selected as the
  // active device and not muted.
  waiter.WaitUntilHDMIRediscoverGracePeriodEnd();
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput.id, active_output.id);
  EXPECT_EQ(kHDMIOutput.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
}

// This tests the case of output unmuting event is notified after the hdmi
// output re-discover grace period ends, see crbug.com/512601.
TEST_F(CrasAudioHandlerTest, HDMIOutputUnplugDuringSuspension) {
  AudioNodeList audio_nodes;
  audio_nodes.push_back(kInternalSpeaker);
  audio_nodes.push_back(kHDMIOutput);
  SetUpCrasAudioHandler(audio_nodes);

  // Verify the HDMI device has been selected as the active output, and audio
  // output is not muted.
  AudioDevice active_output;
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kHDMIOutput.id, active_output.id);
  EXPECT_EQ(kHDMIOutput.id, cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_TRUE(cras_audio_handler_->has_alternative_output());
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());

  // Trigger HDMI rediscovering grace period, and remove the HDMI node.
  const int grace_period_in_ms = 200;
  SetHDMIRediscoverGracePeriodDuration(grace_period_in_ms);
  SetActiveHDMIRediscover();
  AudioNodeList audio_nodes_lost_hdmi;
  audio_nodes_lost_hdmi.push_back(kInternalSpeaker);
  ChangeAudioNodes(audio_nodes_lost_hdmi);

  // Verify the active output is switched to internal speaker, it is not muted
  // by preference, but the system output is muted during the grace period.
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_FALSE(
      cras_audio_handler_->IsOutputMutedForDevice(kInternalSpeaker.id));
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());

  // After HDMI re-discover grace period, verify internal speaker is still the
  // active output and not muted, and unmute event by system is notified.
  test_observer_->reset_output_mute_changed_count();
  HDMIRediscoverWaiter waiter(this, grace_period_in_ms);
  waiter.WaitUntilHDMIRediscoverGracePeriodEnd();
  EXPECT_TRUE(
      cras_audio_handler_->GetPrimaryActiveOutputDevice(&active_output));
  EXPECT_EQ(kInternalSpeaker.id, active_output.id);
  EXPECT_EQ(kInternalSpeaker.id,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());
  EXPECT_EQ(1, test_observer_->output_mute_changed_count());
  EXPECT_TRUE(test_observer_->output_mute_by_system());
}

}  // namespace chromeos
