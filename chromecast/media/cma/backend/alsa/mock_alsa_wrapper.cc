// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/mock_alsa_wrapper.h"

#include "base/logging.h"

// Define dummy structs here to avoid linking in the ALSA lib.
struct _snd_pcm_hw_params {};
struct _snd_pcm_status {};
struct _snd_pcm {};

namespace chromecast {
namespace media {

const size_t kBytesPerSample = sizeof(int32_t);
const int kNumChannels = 2;

// This class keeps basic state to ensure that ALSA is being used correctly.
// Calls from MockAlsaWrapper are delegated to an instance of this class in
// cases where internal ALSA state change occurs, or where a valid value needs
// to be returned to the caller.
class MockAlsaWrapper::FakeAlsaWrapper : public AlsaWrapper {
 public:
  FakeAlsaWrapper()
      : state_(SND_PCM_STATE_RUNNING),
        avail_(0),
        fake_handle_(nullptr),
        fake_pcm_hw_params_(nullptr),
        fake_pcm_status_(nullptr) {}

  ~FakeAlsaWrapper() override {
    delete fake_handle_;
    delete fake_pcm_hw_params_;
    delete fake_pcm_status_;
  }

  // AlsaWrapper implementation:
  int PcmPause(snd_pcm_t* handle, int pause) override {
    CHECK_NE(pause, (state_ == SND_PCM_STATE_PAUSED));
    if (pause)
      state_ = SND_PCM_STATE_PAUSED;
    else
      state_ = SND_PCM_STATE_RUNNING;
    return 0;
  }

  snd_pcm_state_t PcmState(snd_pcm_t* handle) override { return state_; }

  int PcmOpen(snd_pcm_t** handle,
              const char* name,
              snd_pcm_stream_t stream,
              int mode) override {
    fake_handle_ = new snd_pcm_t();
    CHECK(fake_handle_);
    *handle = fake_handle_;
    return 0;
  }

  snd_pcm_sframes_t PcmWritei(snd_pcm_t* handle,
                              const void* buffer,
                              snd_pcm_uframes_t size) override {
    CHECK_EQ(fake_handle_, handle);
    CHECK(buffer);
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(buffer);
    int len = size * kNumChannels * kBytesPerSample;
    data_.assign(ptr, ptr + len);
    return size;
  }

  int PcmHwParamsMalloc(snd_pcm_hw_params_t** ptr) override {
    fake_pcm_hw_params_ = new snd_pcm_hw_params_t();
    CHECK(fake_pcm_hw_params_);
    *ptr = fake_pcm_hw_params_;
    return 0;
  }

  int PcmStatusMalloc(snd_pcm_status_t** ptr) override {
    fake_pcm_status_ = new snd_pcm_status_t();
    CHECK(fake_pcm_status_);
    *ptr = fake_pcm_status_;
    return 0;
  }

  snd_pcm_uframes_t PcmStatusGetAvail(const snd_pcm_status_t* obj) override {
    return avail_;
  }

  snd_pcm_state_t PcmStatusGetState(const snd_pcm_status_t* obj) override {
    return state_;
  }

  ssize_t PcmFormatSize(snd_pcm_format_t format, size_t samples) override {
    return kBytesPerSample * samples;
  };

  snd_pcm_state_t state() const { return state_; }
  void set_state(snd_pcm_state_t state) { state_ = state; }
  void set_avail(snd_pcm_uframes_t avail) { avail_ = avail; }
  std::vector<uint8_t>& data() { return data_; }

 private:
  snd_pcm_state_t state_;
  snd_pcm_uframes_t avail_;
  snd_pcm_t* fake_handle_;
  snd_pcm_hw_params_t* fake_pcm_hw_params_;
  snd_pcm_status_t* fake_pcm_status_;
  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(FakeAlsaWrapper);
};

using ::testing::_;
using ::testing::Invoke;

MockAlsaWrapper::MockAlsaWrapper() : fake_(new FakeAlsaWrapper()) {
  // Delgate calls that need to be faked.
  ON_CALL(*this, PcmOpen(_, _, _, _))
      .WillByDefault(testing::Invoke(fake_.get(), &FakeAlsaWrapper::PcmOpen));
  ON_CALL(*this, PcmWritei(_, _, _))
      .WillByDefault(testing::Invoke(fake_.get(), &FakeAlsaWrapper::PcmWritei));
  ON_CALL(*this, PcmState(_))
      .WillByDefault(testing::Invoke(fake_.get(), &FakeAlsaWrapper::PcmState));
  ON_CALL(*this, PcmFormatSize(_, _)).WillByDefault(
      testing::Invoke(fake_.get(), &FakeAlsaWrapper::PcmFormatSize));
  ON_CALL(*this, PcmPause(_, _))
      .WillByDefault(testing::Invoke(fake_.get(), &FakeAlsaWrapper::PcmPause));
  ON_CALL(*this, PcmHwParamsCanPause(_)).WillByDefault(testing::Return(true));
  ON_CALL(*this, PcmHwParamsMalloc(_)).WillByDefault(
      testing::Invoke(fake_.get(), &FakeAlsaWrapper::PcmHwParamsMalloc));
  ON_CALL(*this, PcmStatusMalloc(_))
      .WillByDefault(
          testing::Invoke(fake_.get(), &FakeAlsaWrapper::PcmStatusMalloc));
  ON_CALL(*this, PcmStatusGetState(_))
      .WillByDefault(
          testing::Invoke(fake_.get(), &FakeAlsaWrapper::PcmStatusGetState));
  ON_CALL(*this, PcmStatusGetAvail(_))
      .WillByDefault(
          testing::Invoke(fake_.get(), &FakeAlsaWrapper::PcmStatusGetAvail));
}

MockAlsaWrapper::~MockAlsaWrapper() {
}

snd_pcm_state_t MockAlsaWrapper::state() {
  return fake_->state();
}

void MockAlsaWrapper::set_state(snd_pcm_state_t state) {
  fake_->set_state(state);
}

void MockAlsaWrapper::set_avail(snd_pcm_uframes_t avail) {
  fake_->set_avail(avail);
}

const std::vector<uint8_t>& MockAlsaWrapper::data() {
  return fake_->data();
}

}  // namespace media
}  // namespace chromecast
