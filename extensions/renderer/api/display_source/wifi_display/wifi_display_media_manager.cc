// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_manager.h"

#include "base/logging.h"
#include "base/rand_util.h"

namespace extensions {

WiFiDisplayMediaManager::WiFiDisplayMediaManager() {
}

WiFiDisplayMediaManager::~WiFiDisplayMediaManager() {
}

void WiFiDisplayMediaManager::Play() {
  NOTIMPLEMENTED();
}

void WiFiDisplayMediaManager::Teardown() {
  NOTIMPLEMENTED();
}

void WiFiDisplayMediaManager::Pause() {
  NOTIMPLEMENTED();
}

bool WiFiDisplayMediaManager::IsPaused() const {
  NOTIMPLEMENTED();
  return true;
}

wds::SessionType WiFiDisplayMediaManager::GetSessionType() const {
  NOTIMPLEMENTED();
  return wds::AudioVideoSession;
}

void WiFiDisplayMediaManager::SetSinkRtpPorts(int port1, int port2) {
  NOTIMPLEMENTED();
}

std::pair<int,int> WiFiDisplayMediaManager::GetSinkRtpPorts() const {
  NOTIMPLEMENTED();
  return std::pair<int,int>();
}

int WiFiDisplayMediaManager::GetLocalRtpPort() const {
  NOTIMPLEMENTED();
  return 0;
}

wds::H264VideoFormat WiFiDisplayMediaManager::GetOptimalVideoFormat() const {
  NOTIMPLEMENTED();
  return wds::H264VideoFormat();
}

void WiFiDisplayMediaManager::SendIDRPicture() {
  NOTIMPLEMENTED();
}

std::string WiFiDisplayMediaManager::GetSessionId() const {
  return base::RandBytesAsString(8);
}

bool WiFiDisplayMediaManager::InitOptimalVideoFormat(
    const wds::NativeVideoFormat& sink_native_format,
    const std::vector<wds::H264VideoCodec>& sink_supported_codecs) {
  NOTIMPLEMENTED();
  return false;
}

bool WiFiDisplayMediaManager::InitOptimalAudioFormat(
    const std::vector<wds::AudioCodec>& sink_codecs) {
  NOTIMPLEMENTED();
  return false;
}

wds::AudioCodec WiFiDisplayMediaManager::GetOptimalAudioFormat() const {
  NOTIMPLEMENTED();
  return wds::AudioCodec();
}

}  // namespace extensions
