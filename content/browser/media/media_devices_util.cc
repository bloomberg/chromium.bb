// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_util.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/media_switches.h"

namespace content {

namespace {

std::string GetDefaultMediaDeviceIDOnUIThread(MediaDeviceType device_type,
                                              int render_process_id,
                                              int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!frame_host)
    return std::string();

  RenderFrameHostDelegate* delegate = frame_host->delegate();
  if (!delegate)
    return std::string();

  MediaStreamType media_stream_type;
  switch (device_type) {
    case MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      media_stream_type = MEDIA_DEVICE_AUDIO_CAPTURE;
      break;
    case MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      media_stream_type = MEDIA_DEVICE_VIDEO_CAPTURE;
      break;
    default:
      return std::string();
  }

  return delegate->GetDefaultMediaDeviceID(media_stream_type);
}

// This function is intended for testing purposes. It returns an empty string
// if no default device is supplied via the command line.
std::string GetDefaultMediaDeviceIDFromCommandLine(
    MediaDeviceType device_type) {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream));
  const std::string option =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseFakeDeviceForMediaStream);
  // Optional comma delimited parameters to the command line can specify values
  // for the default device IDs.
  // Examples: "video-input-default-id=mycam, audio-input-default-id=mymic"
  base::StringTokenizer option_tokenizer(option, ", ");
  option_tokenizer.set_quote_chars("\"");

  while (option_tokenizer.GetNext()) {
    std::vector<std::string> param =
        base::SplitString(option_tokenizer.token(), "=", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (param.size() != 2u) {
      DLOG(WARNING) << "Forgot a value '" << option << "'? Use name=value for "
                    << switches::kUseFakeDeviceForMediaStream << ".";
      return std::string();
    }

    if (device_type == MEDIA_DEVICE_TYPE_AUDIO_INPUT &&
        param.front() == "audio-input-default-id") {
      return param.back();
    } else if (device_type == MEDIA_DEVICE_TYPE_VIDEO_INPUT &&
               param.front() == "video-input-default-id") {
      return param.back();
    }
  }

  return std::string();
}

}  // namespace

void GetDefaultMediaDeviceID(
    MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    const base::Callback<void(const std::string&)>& callback) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    std::string command_line_default_device_id =
        GetDefaultMediaDeviceIDFromCommandLine(device_type);
    if (!command_line_default_device_id.empty()) {
      callback.Run(command_line_default_device_id);
      return;
    }
  }

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetDefaultMediaDeviceIDOnUIThread, device_type,
                 render_process_id, render_frame_id),
      callback);
}

}  // namespace content
