// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_parser_android.h"

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/services/media_gallery_util/ipc_data_source.h"
#include "media/base/bind_to_current_loop.h"
#include "media/filters/android/video_frame_extractor.h"

namespace {

void OnThumbnailGenerated(
    std::unique_ptr<media::VideoFrameExtractor> video_frame_extractor,
    MediaParser::ExtractVideoFrameCallback extract_frame_cb,
    bool success,
    const std::vector<uint8_t>& data,
    const media::VideoDecoderConfig& config) {
  std::move(extract_frame_cb).Run(success, data, config);
}

void ExtractVideoFrameOnMediaThread(
    media::DataSource* data_source,
    MediaParser::ExtractVideoFrameCallback extract_frame_callback) {
  auto extractor = std::make_unique<media::VideoFrameExtractor>(data_source);
  extractor->Start(base::BindOnce(&OnThumbnailGenerated, std::move(extractor),
                                  std::move(extract_frame_callback)));
}

}  // namespace

MediaParserAndroid::MediaParserAndroid(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : MediaParser(std::move(service_ref)),
      media_task_runner_(
          base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()})) {}

MediaParserAndroid::~MediaParserAndroid() = default;

void MediaParserAndroid::ExtractVideoFrame(
    const std::string& mime_type,
    uint32_t total_size,
    chrome::mojom::MediaDataSourcePtr media_data_source,
    MediaParser::ExtractVideoFrameCallback extract_frame_callback) {
  data_source_ = std::make_unique<IPCDataSource>(
      std::move(media_data_source), static_cast<int64_t>(total_size));

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ExtractVideoFrameOnMediaThread, data_source_.get(),
          media::BindToCurrentLoop(std::move(extract_frame_callback))));
}
