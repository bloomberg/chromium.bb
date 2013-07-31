// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/fake_video_capture_device.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "media/audio/fake_audio_input_stream.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace media {

static const int kFakeCaptureTimeoutMs = 50;
static const int kFakeCaptureBeepCycle = 20;  // Visual beep every 1s.
static const int kFakeCaptureCapabilityChangePeriod = 30;
enum { kNumberOfFakeDevices = 2 };

bool FakeVideoCaptureDevice::fail_next_create_ = false;

void FakeVideoCaptureDevice::GetDeviceNames(Names* const device_names) {
  // Empty the name list.
  device_names->erase(device_names->begin(), device_names->end());

  for (int n = 0; n < kNumberOfFakeDevices; n++) {
    Name name(base::StringPrintf("fake_device_%d", n),
              base::StringPrintf("/dev/video%d", n));
    device_names->push_back(name);
  }
}

VideoCaptureDevice* FakeVideoCaptureDevice::Create(const Name& device_name) {
  if (fail_next_create_) {
    fail_next_create_ = false;
    return NULL;
  }
  for (int n = 0; n < kNumberOfFakeDevices; ++n) {
    std::string possible_id = base::StringPrintf("/dev/video%d", n);
    if (device_name.id().compare(possible_id) == 0) {
      return new FakeVideoCaptureDevice(device_name);
    }
  }
  return NULL;
}

void FakeVideoCaptureDevice::SetFailNextCreate() {
  fail_next_create_ = true;
}

FakeVideoCaptureDevice::FakeVideoCaptureDevice(const Name& device_name)
    : device_name_(device_name),
      observer_(NULL),
      state_(kIdle),
      capture_thread_("CaptureThread"),
      frame_count_(0) {
}

FakeVideoCaptureDevice::~FakeVideoCaptureDevice() {
  // Check if the thread is running.
  // This means that the device have not been DeAllocated properly.
  DCHECK(!capture_thread_.IsRunning());
}

void FakeVideoCaptureDevice::Allocate(
    const VideoCaptureCapability& capture_format,
    EventHandler* observer) {
  capture_format_.frame_size_type = capture_format.frame_size_type;
  if (capture_format.frame_size_type == VariableResolutionVideoCaptureDevice)
    PopulateCapabilitiesRoster();

  if (state_ != kIdle) {
    return;  // Wrong state.
  }

  observer_ = observer;
  capture_format_.color = VideoCaptureCapability::kI420;
  capture_format_.expected_capture_delay = 0;
  capture_format_.interlaced = false;
  if (capture_format.width > 320) {  // VGA
    capture_format_.width = 640;
    capture_format_.height = 480;
    capture_format_.frame_rate = 30;
  } else {  // QVGA
    capture_format_.width = 320;
    capture_format_.height = 240;
    capture_format_.frame_rate = 30;
  }

  size_t fake_frame_size =
      capture_format_.width * capture_format_.height * 3 / 2;
  fake_frame_.reset(new uint8[fake_frame_size]);

  state_ = kAllocated;
  observer_->OnFrameInfo(capture_format_);
}

void FakeVideoCaptureDevice::Reallocate() {
  DCHECK_EQ(state_, kCapturing);
  capture_format_ = capabilities_roster_.at(++capabilities_roster_index_ %
                                            capabilities_roster_.size());
  DCHECK_EQ(capture_format_.color, VideoCaptureCapability::kI420);
  DVLOG(3) << "Reallocating FakeVideoCaptureDevice, new capture resolution ("
           << capture_format_.width << "x" << capture_format_.height << ")";

  size_t fake_frame_size =
      capture_format_.width * capture_format_.height * 3 / 2;
  fake_frame_.reset(new uint8[fake_frame_size]);

  observer_->OnFrameInfoChanged(capture_format_);
}

void FakeVideoCaptureDevice::Start() {
  if (state_ != kAllocated) {
      return;  // Wrong state.
  }
  state_ = kCapturing;
  capture_thread_.Start();
  capture_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FakeVideoCaptureDevice::OnCaptureTask,
                 base::Unretained(this)));
}

void FakeVideoCaptureDevice::Stop() {
  if (state_ != kCapturing) {
      return;  // Wrong state.
  }
  capture_thread_.Stop();
  state_ = kAllocated;
}

void FakeVideoCaptureDevice::DeAllocate() {
  if (state_ != kAllocated && state_ != kCapturing) {
      return;  // Wrong state.
  }
  capture_thread_.Stop();
  state_ = kIdle;
}

const VideoCaptureDevice::Name& FakeVideoCaptureDevice::device_name() {
  return device_name_;
}

void FakeVideoCaptureDevice::OnCaptureTask() {
  if (state_ != kCapturing) {
    return;
  }

  int frame_size = capture_format_.width * capture_format_.height * 3 / 2;
  memset(fake_frame_.get(), 0, frame_size);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kA8_Config,
                   capture_format_.width,
                   capture_format_.height,
                   capture_format_.width);
  bitmap.setPixels(fake_frame_.get());

  SkCanvas canvas(bitmap);

  // Draw a sweeping circle to show an animation.
  int radius = std::min(capture_format_.width, capture_format_.height) / 4;
  SkRect rect = SkRect::MakeXYWH(
      capture_format_.width / 2 - radius, capture_format_.height / 2 - radius,
      2 * radius, 2 * radius);

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);

  // Only Y plane is being drawn and this gives 50% grey on the Y
  // plane. The result is a light green color in RGB space.
  paint.setAlpha(128);

  int end_angle = (frame_count_ % kFakeCaptureBeepCycle * 360) /
      kFakeCaptureBeepCycle;
  if (!end_angle)
    end_angle = 360;
  canvas.drawArc(rect, 0, end_angle, true, paint);

  // Draw current time.
  int elapsed_ms = kFakeCaptureTimeoutMs * frame_count_;
  int milliseconds = elapsed_ms % 1000;
  int seconds = (elapsed_ms / 1000) % 60;
  int minutes = (elapsed_ms / 1000 / 60) % 60;
  int hours = (elapsed_ms / 1000 / 60 / 60) % 60;

  std::string time_string =
      base::StringPrintf("%d:%02d:%02d:%03d %d", hours, minutes,
                         seconds, milliseconds, frame_count_);
  canvas.scale(3, 3);
  canvas.drawText(time_string.data(), time_string.length(), 30, 20,
                  paint);

  if (frame_count_ % kFakeCaptureBeepCycle == 0) {
    // Generate a synchronized beep sound if there is one audio input
    // stream created.
    FakeAudioInputStream::BeepOnce();
  }

  frame_count_++;

  // Give the captured frame to the observer.
  observer_->OnIncomingCapturedFrame(
      fake_frame_.get(), frame_size, base::Time::Now(), 0, false, false);
  if (!(frame_count_ % kFakeCaptureCapabilityChangePeriod) &&
      (capture_format_.frame_size_type ==
       VariableResolutionVideoCaptureDevice)) {
    Reallocate();
  }
  // Reschedule next CaptureTask.
  capture_thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeVideoCaptureDevice::OnCaptureTask,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kFakeCaptureTimeoutMs));
}

void FakeVideoCaptureDevice::PopulateCapabilitiesRoster() {
  capabilities_roster_.push_back(
      media::VideoCaptureCapability(320,
                                    240,
                                    30,
                                    VideoCaptureCapability::kI420,
                                    0,
                                    false,
                                    VariableResolutionVideoCaptureDevice));
  capabilities_roster_.push_back(
      media::VideoCaptureCapability(640,
                                    480,
                                    30,
                                    VideoCaptureCapability::kI420,
                                    0,
                                    false,
                                    VariableResolutionVideoCaptureDevice));
  capabilities_roster_.push_back(
      media::VideoCaptureCapability(800,
                                    600,
                                    30,
                                    VideoCaptureCapability::kI420,
                                    0,
                                    false,
                                    VariableResolutionVideoCaptureDevice));

  capabilities_roster_index_ = 0;
}

}  // namespace media
