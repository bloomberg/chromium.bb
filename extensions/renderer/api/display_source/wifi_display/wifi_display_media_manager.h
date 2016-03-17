// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_MANAGER_H_
#define EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_MANAGER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/WebKit/public/web/WebDOMMediaStreamTrack.h"
#include "third_party/wds/src/libwds/public/media_manager.h"

namespace extensions {

class WiFiDisplayMediaManager : public wds::SourceMediaManager {
 public:
  using ErrorCallback = base::Callback<void(const std::string&)>;

  WiFiDisplayMediaManager(
      const blink::WebMediaStreamTrack& video_track,
      const blink::WebMediaStreamTrack& audio_track,
      const ErrorCallback& error_callback);

  ~WiFiDisplayMediaManager() override;

 private:
  // wds::SourceMediaManager overrides.
  void Play() override;

  void Pause() override;
  void Teardown() override;
  bool IsPaused() const override;
  wds::SessionType GetSessionType() const override;
  void SetSinkRtpPorts(int port1, int port2) override;
  std::pair<int, int> GetSinkRtpPorts() const override;
  int GetLocalRtpPort() const override;

  bool InitOptimalVideoFormat(
      const wds::NativeVideoFormat& sink_native_format,
      const std::vector<wds::H264VideoCodec>& sink_supported_codecs) override;
  wds::H264VideoFormat GetOptimalVideoFormat() const override;
  bool InitOptimalAudioFormat(
      const std::vector<wds::AudioCodec>& sink_supported_codecs) override;
  wds::AudioCodec GetOptimalAudioFormat() const override;

  void SendIDRPicture() override;

  std::string GetSessionId() const override;

 private:
  blink::WebMediaStreamTrack video_track_;
  blink::WebMediaStreamTrack audio_track_;

  std::pair<int, int> sink_rtp_ports_;
  wds::H264VideoFormat optimal_video_format_;

  ErrorCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(WiFiDisplayMediaManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_MANAGER_H_
