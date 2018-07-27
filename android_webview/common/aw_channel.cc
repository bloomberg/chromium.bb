// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_channel.h"

#include "components/version_info/android/channel_getter.h"

namespace android_webview {

using version_info::Channel;

Channel GetChannel() {
  Channel channel = version_info::GetChannel();
  // There are separate Monochrome APKs built for each channel, but only one
  // stand-alone WebView APK for all channels, so stand-alone WebView has
  // channel "unknown". Pretend stand-alone WebView is always "stable" for the
  // purpose of variations. This simplifies experiment design, since stand-alone
  // WebView need not be considered separately when choosing channels.
  return channel == Channel::UNKNOWN ? Channel::STABLE : channel;
}

}  // namespace android_webview
