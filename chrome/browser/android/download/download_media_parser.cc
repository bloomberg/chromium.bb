// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_media_parser.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/android/download/local_media_data_source_factory.h"
#include "content/public/common/service_manager_connection.h"

namespace {

// Returns if the mime type is video or audio.
bool IsSupportedMediaMimeType(const std::string& mime_type) {
  return base::StartsWith(mime_type, "audio/",
                          base::CompareCase::INSENSITIVE_ASCII) ||
         base::StartsWith(mime_type, "video/",
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

DownloadMediaParser::DownloadMediaParser(int64_t size,
                                         const std::string& mime_type,
                                         const base::FilePath& file_path,
                                         ParseCompleteCB parse_complete_cb)
    : size_(size),
      mime_type_(mime_type),
      file_path_(file_path),
      parse_complete_cb_(std::move(parse_complete_cb)),
      file_task_runner_(
          base::CreateSingleThreadTaskRunnerWithTraits({base::MayBlock()})),
      weak_factory_(this) {}

DownloadMediaParser::~DownloadMediaParser() = default;

void DownloadMediaParser::Start() {
  // Only process media mime types.
  if (!IsSupportedMediaMimeType(mime_type_)) {
    OnError();
    return;
  }

  RetrieveMediaParser(
      content::ServiceManagerConnection::GetForProcess()->GetConnector());
}

void DownloadMediaParser::OnMediaParserCreated() {
  auto media_source_factory = std::make_unique<LocalMediaDataSourceFactory>(
      file_path_, file_task_runner_);
  chrome::mojom::MediaDataSourcePtr source_ptr;
  media_data_source_ = media_source_factory->CreateMediaDataSource(
      &source_ptr, base::BindRepeating(&DownloadMediaParser::OnMediaDataReady,
                                       weak_factory_.GetWeakPtr()));

  media_parser()->ParseMediaMetadata(
      mime_type_, size_, true /* get_attached_images */, std::move(source_ptr),
      base::BindOnce(&DownloadMediaParser::OnMediaMetadataParsed,
                     weak_factory_.GetWeakPtr()));
}

void DownloadMediaParser::OnConnectionError() {
  if (parse_complete_cb_)
    std::move(parse_complete_cb_).Run(false);
}

void DownloadMediaParser::OnMediaMetadataParsed(
    bool parse_success,
    chrome::mojom::MediaMetadataPtr metadata,
    const std::vector<metadata::AttachedImage>& attached_images) {
  if (!parse_success) {
    std::move(parse_complete_cb_).Run(false);
    return;
  }
  metadata_ = std::move(metadata);

  // TODO(xingliu): Make |attached_images| movable and use this as a thumbnail
  // source as well as video frame.
  attached_images_ = attached_images;

  // For audio file, we only need metadata and poster.
  if (base::StartsWith(mime_type_, "audio/",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    NotifyComplete();
    return;
  }

  DCHECK(base::StartsWith(mime_type_, "video/",
                          base::CompareCase::INSENSITIVE_ASCII));

  // Retrieves video thumbnail if needed.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DownloadMediaParser::RetrieveEncodedVideoFrame,
                                weak_factory_.GetWeakPtr()));
}

void DownloadMediaParser::RetrieveEncodedVideoFrame() {
  media_data_source_.reset();

  auto media_source_factory = std::make_unique<LocalMediaDataSourceFactory>(
      file_path_, file_task_runner_);
  chrome::mojom::MediaDataSourcePtr source_ptr;
  media_data_source_ = media_source_factory->CreateMediaDataSource(
      &source_ptr, base::BindRepeating(&DownloadMediaParser::OnMediaDataReady,
                                       weak_factory_.GetWeakPtr()));

  media_parser()->ExtractVideoFrame(
      mime_type_, base::saturated_cast<uint32_t>(size_), std::move(source_ptr),
      base::BindOnce(&DownloadMediaParser::OnEncodedVideoFrameRetrieved,
                     weak_factory_.GetWeakPtr()));
}

void DownloadMediaParser::OnEncodedVideoFrameRetrieved(
    bool success,
    const std::vector<uint8_t>& data,
    const media::VideoDecoderConfig& config) {
  if (data.empty()) {
    OnError();
    return;
  }

  encoded_data_ = data;
  config_ = config;

  // TODO(xingliu): Decode the video frame with MojoVideoDecoder.
  NotifyComplete();
}

void DownloadMediaParser::OnMediaDataReady(
    chrome::mojom::MediaDataSource::ReadCallback callback,
    std::unique_ptr<std::string> data) {
  // TODO(xingliu): Change media_parser.mojom to move the data instead of copy.
  if (media_parser())
    std::move(callback).Run(std::vector<uint8_t>(data->begin(), data->end()));
}

void DownloadMediaParser::NotifyComplete() {
  // TODO(xingliu): Return the metadata and video thumbnail data in
  // |parse_complete_cb_|.
  if (parse_complete_cb_)
    std::move(parse_complete_cb_).Run(true);
}

void DownloadMediaParser::OnError() {
  if (parse_complete_cb_)
    std::move(parse_complete_cb_).Run(false);
}
