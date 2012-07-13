// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "content/renderer/media/audio_device.h"
#include "content/renderer/media/audio_message_filter.h"
#include "media/audio/sample_rates.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Derived from AudioDevice to gain access to the protected constructor.
class TestAudioDevice : public AudioDevice {
 public:
  explicit TestAudioDevice(const scoped_refptr<base::MessageLoopProxy>& io_loop)
      : AudioDevice(io_loop) {}

 protected:
  virtual ~TestAudioDevice() {}
};

class FakeRenderCallback : public media::AudioRendererSink::RenderCallback {
 public:
  FakeRenderCallback() {}
  virtual ~FakeRenderCallback() {}

  virtual int Render(const std::vector<float*>& audio_data,
                     int number_of_frames,
                     int audio_delay_milliseconds) OVERRIDE {
    NOTIMPLEMENTED();
    return 0;
  }

  virtual void OnRenderError() OVERRIDE {
    ADD_FAILURE();
  }
};

class FakeAudioMessageFilter : public AudioMessageFilter {
 public:
  FakeAudioMessageFilter() {}

  virtual bool Send(IPC::Message* message) {
    // TODO(tommi): Override to simulate message passing.
    NOTIMPLEMENTED();
    delete message;
    return true;
  }

 protected:
  virtual ~FakeAudioMessageFilter() {}
};

}  // namespace.

class AudioDeviceTest : public testing::Test {
 public:
  AudioDeviceTest() {}
  ~AudioDeviceTest() {}

 protected:
  // Used to clean up TLS pointers that the test(s) will initialize.
  // Must remain the first member of this class.
  base::ShadowingAtExitManager at_exit_manager_;
  MessageLoopForIO io_loop_;
};

// The simplest test for AudioDevice.  Used to test construction of AudioDevice
// and that the runtime environment is set up correctly (e.g. ChildProcess and
// AudioMessageFilter global pointers).
TEST_F(AudioDeviceTest, Initialize) {
  FakeRenderCallback callback;
  media::AudioParameters audio_parameters(
      media::AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
      media::k48000Hz, 16, 1024);

  // This sets a global audio_message_filter pointer.  AudioDevice will pick
  // up a pointer to this variable via the static AudioMessageFilter::Get()
  // method.
  scoped_refptr<FakeAudioMessageFilter> audio_message_filter =
      new FakeAudioMessageFilter();

  scoped_refptr<AudioDevice> audio_device(
      new TestAudioDevice(io_loop_.message_loop_proxy()));
  audio_device->Initialize(audio_parameters, &callback);
}
