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
#include "chrome/browser/android/download/video_frame_thumbnail_converter.h"
#include "content/public/browser/android/gpu_video_accelerator_factories_provider.h"
#include "content/public/common/service_manager_connection.h"
#include "media/base/overlay_info.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/interfaces/constants.mojom.h"
#include "media/mojo/interfaces/media_service.mojom.h"
#include "media/mojo/services/media_interface_provider.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// Returns if the mime type is video or audio.
bool IsSupportedMediaMimeType(const std::string& mime_type) {
  return base::StartsWith(mime_type, "audio/",
                          base::CompareCase::INSENSITIVE_ASCII) ||
         base::StartsWith(mime_type, "video/",
                          base::CompareCase::INSENSITIVE_ASCII);
}

void OnRequestOverlayInfo(bool decoder_requires_restart_for_overlay,
                          const media::ProvideOverlayInfoCB& overlay_info_cb) {
  // No android overlay associated with video thumbnail.
  overlay_info_cb.Run(media::OverlayInfo());
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
      decode_done_(false),
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
  OnError();
}

void DownloadMediaParser::OnMediaMetadataParsed(
    bool parse_success,
    chrome::mojom::MediaMetadataPtr metadata,
    const std::vector<metadata::AttachedImage>& attached_images) {
  if (!parse_success) {
    OnError();
    return;
  }
  metadata_ = std::move(metadata);
  DCHECK(metadata_);

  // TODO(xingliu): Make |attached_images| movable and use this as a thumbnail
  // source as well as video frame.
  attached_images_ = attached_images;

  // For audio file, we only need metadata and poster.
  if (base::StartsWith(mime_type_, "audio/",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    NotifyComplete(SkBitmap());
    return;
  }

  DCHECK(base::StartsWith(mime_type_, "video/",
                          base::CompareCase::INSENSITIVE_ASCII));

  // Start to retrieve video thumbnail.
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

  content::CreateGpuVideoAcceleratorFactories(base::BindRepeating(
      &DownloadMediaParser::OnGpuVideoAcceleratorFactoriesReady,
      weak_factory_.GetWeakPtr()));
}

void DownloadMediaParser::OnGpuVideoAcceleratorFactoriesReady(
    std::unique_ptr<media::GpuVideoAcceleratorFactories> factories) {
  gpu_factories_ = std::move(factories);
  DecodeVideoFrame();
}

void DownloadMediaParser::DecodeVideoFrame() {
  media::mojom::VideoDecoderPtr video_decoder_ptr;
  GetMediaInterfaceFactory()->CreateVideoDecoder(
      mojo::MakeRequest(&video_decoder_ptr));

  // Build and config the decoder.
  DCHECK(gpu_factories_);
  decoder_ = std::make_unique<media::MojoVideoDecoder>(
      base::ThreadTaskRunnerHandle::Get(), gpu_factories_.get(), this,
      std::move(video_decoder_ptr), base::BindRepeating(&OnRequestOverlayInfo),
      gfx::ColorSpace());

  decoder_->Initialize(
      config_, false, nullptr,
      base::BindRepeating(&DownloadMediaParser::OnVideoDecoderInitialized,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&DownloadMediaParser::OnVideoFrameDecoded,
                          weak_factory_.GetWeakPtr()),
      base::RepeatingClosure());
}

void DownloadMediaParser::OnVideoDecoderInitialized(bool success) {
  if (!success) {
    OnError();
    return;
  }

  // Build the video buffer to decode.
  auto buffer =
      media::DecoderBuffer::CopyFrom(&encoded_data_[0], encoded_data_.size());
  encoded_data_.clear();

  // Decode one frame buffer, followed by eos buffer.
  DCHECK_GE(decoder_->GetMaxDecodeRequests(), 2);
  decoder_->Decode(
      buffer, base::BindRepeating(&DownloadMediaParser::OnVideoBufferDecoded,
                                  weak_factory_.GetWeakPtr()));
  decoder_->Decode(media::DecoderBuffer::CreateEOSBuffer(),
                   base::BindRepeating(&DownloadMediaParser::OnEosBufferDecoded,
                                       weak_factory_.GetWeakPtr()));
}

void DownloadMediaParser::OnVideoBufferDecoded(media::DecodeStatus status) {
  if (status != media::DecodeStatus::OK)
    OnError();
}

void DownloadMediaParser::OnEosBufferDecoded(media::DecodeStatus status) {
  if (status != media::DecodeStatus::OK)
    OnError();

  // Fails if no decoded video frame is generated when eos arrives.
  if (!decode_done_)
    OnError();
}

void DownloadMediaParser::OnVideoFrameDecoded(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(frame);
  DCHECK(frame->HasTextures());
  decode_done_ = true;

  RenderVideoFrame(frame);
}

void DownloadMediaParser::RenderVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  auto converter = VideoFrameThumbnailConverter::Create(
      config_.codec(), gpu_factories_->GetMediaContextProvider());
  converter->ConvertToBitmap(
      video_frame,
      base::BindOnce(&DownloadMediaParser::OnBitmapGenerated,
                     weak_factory_.GetWeakPtr(), std::move(converter)));
}

void DownloadMediaParser::OnBitmapGenerated(
    std::unique_ptr<VideoFrameThumbnailConverter>,
    bool success,
    SkBitmap bitmap) {
  if (!success) {
    OnError();
    return;
  }

  NotifyComplete(std::move(bitmap));
}

media::mojom::InterfaceFactory*
DownloadMediaParser::GetMediaInterfaceFactory() {
  if (!media_interface_factory_) {
    service_manager::mojom::InterfaceProviderPtr interfaces;
    media_interface_provider_ = std::make_unique<media::MediaInterfaceProvider>(
        mojo::MakeRequest(&interfaces));
    media::mojom::MediaServicePtr media_service;
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(media::mojom::kMediaServiceName, &media_service);
    media_service->CreateInterfaceFactory(
        MakeRequest(&media_interface_factory_), std::move(interfaces));
    media_interface_factory_.set_connection_error_handler(
        base::BindOnce(&DownloadMediaParser::OnDecoderConnectionError,
                       base::Unretained(this)));
  }

  return media_interface_factory_.get();
}

void DownloadMediaParser::OnDecoderConnectionError() {
  OnError();
}

void DownloadMediaParser::OnMediaDataReady(
    chrome::mojom::MediaDataSource::ReadCallback callback,
    std::unique_ptr<std::string> data) {
  // TODO(xingliu): Change media_parser.mojom to move the data instead of copy.
  if (media_parser())
    std::move(callback).Run(std::vector<uint8_t>(data->begin(), data->end()));
}

void DownloadMediaParser::NotifyComplete(SkBitmap bitmap) {
  // TODO(xingliu): Return the metadata and video thumbnail data in
  // |parse_complete_cb_|.
  DCHECK(metadata_);
  if (parse_complete_cb_)
    std::move(parse_complete_cb_)
        .Run(true, std::move(metadata_), std::move(bitmap));
}

void DownloadMediaParser::OnError() {
  if (parse_complete_cb_)
    std::move(parse_complete_cb_)
        .Run(false, chrome::mojom::MediaMetadata::New(), SkBitmap());
}
