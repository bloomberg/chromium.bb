// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/vibration_message_filter.h"

#include <algorithm>

#include "base/safe_numerics.h"
#include "content/common/view_messages.h"
#include "jni/VibrationMessageFilter_jni.h"
#include "third_party/WebKit/public/platform/WebVibration.h"

using base::android::AttachCurrentThread;

namespace content {

// Minimum duration of a vibration is 1 millisecond.
const int64 kMinimumVibrationDurationMs = 1;

VibrationMessageFilter::VibrationMessageFilter() {
}

VibrationMessageFilter::~VibrationMessageFilter() {
}

// static
bool VibrationMessageFilter::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool VibrationMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(VibrationMessageFilter,
                           message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Vibrate, OnVibrate)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CancelVibration, OnCancelVibration)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void VibrationMessageFilter::OnVibrate(int64 milliseconds) {
  // Though the Blink implementation already sanitizes vibration times, don't
  // trust any values passed from the renderer.
  milliseconds = std::max(kMinimumVibrationDurationMs,
      std::min(milliseconds,
          base::checked_numeric_cast<int64>(WebKit::kVibrationDurationMax)));

  if (j_vibration_message_filter_.is_null()) {
    j_vibration_message_filter_.Reset(
        Java_VibrationMessageFilter_create(
            AttachCurrentThread(),
            base::android::GetApplicationContext()));
  }
  Java_VibrationMessageFilter_vibrate(AttachCurrentThread(),
                                j_vibration_message_filter_.obj(),
                                milliseconds);
}

void VibrationMessageFilter::OnCancelVibration() {
  Java_VibrationMessageFilter_cancelVibration(AttachCurrentThread(),
                                        j_vibration_message_filter_.obj());
}

}  // namespace content
