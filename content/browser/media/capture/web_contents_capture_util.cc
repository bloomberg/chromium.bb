// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_capture_util.h"

#include "base/basictypes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace content {

bool WebContentsCaptureUtil::IsWebContentsDeviceId(
    const std::string& device_id) {
  int ignored;
  return ExtractTabCaptureTarget(device_id, &ignored, &ignored);
}

bool WebContentsCaptureUtil::ExtractTabCaptureTarget(
    const std::string& device_id_param,
    int* render_process_id,
    int* main_render_frame_id) {
  static const char kDeviceScheme[] = "web-contents-media-stream://";
  if (!base::StartsWith(device_id_param, kDeviceScheme,
                        base::CompareCase::SENSITIVE))
    return false;

  const std::string device_id = device_id_param.substr(
      arraysize(kDeviceScheme) - 1);

  const size_t sep_pos = device_id.find(':');
  if (sep_pos == std::string::npos)
    return false;

  const base::StringPiece component1(device_id.data(), sep_pos);
  size_t end_pos = device_id.find('?');
  if (end_pos == std::string::npos)
    end_pos = device_id.length();
  const base::StringPiece component2(device_id.data() + sep_pos + 1,
                                     end_pos - sep_pos - 1);

  return (base::StringToInt(component1, render_process_id) &&
          base::StringToInt(component2, main_render_frame_id));
}

bool WebContentsCaptureUtil::IsAutoThrottlingOptionSet(
    const std::string& device_id) {
  if (!IsWebContentsDeviceId(device_id))
    return false;

  // Find the option part of the string and just do a naive string compare since
  // there are no other options in the |device_id| to account for (at the time
  // of this writing).
  const size_t option_pos = device_id.find('?');
  if (option_pos == std::string::npos)
    return false;
  const base::StringPiece component(device_id.data() + option_pos,
                                    device_id.length() - option_pos);
  static const char kEnableFlag[] = "?throttling=auto";
  return component.compare(kEnableFlag) == 0;
}

}  // namespace content
