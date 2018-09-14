// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MEDIA_PARSER_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MEDIA_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/services/media_gallery_util/public/cpp/media_parser_provider.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "media/base/media_log.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"

namespace media {
class GpuVideoAcceleratorFactories;
class MediaInterfaceProvider;
class MojoVideoDecoder;
class VideoDecoderConfig;
}  // namespace media

class SkBitmap;
class VideoFrameThumbnailConverter;

// Parse local media files, including media metadata and thumbnails.
// Metadata is always parsed in utility process for both audio and video files.
//
// For video file, the thumbnail may be poster image in metadata or extracted
// video frame. The frame extraction always happens in utility process. The
// decoding may happen in utility or GPU process based on video codec.
class DownloadMediaParser : public MediaParserProvider, public media::MediaLog {
 public:
  using ParseCompleteCB =
      base::OnceCallback<void(bool success,
                              chrome::mojom::MediaMetadataPtr media_metadata,
                              SkBitmap bitmap)>;

  DownloadMediaParser(int64_t size,
                      const std::string& mime_type,
                      const base::FilePath& file_path,
                      ParseCompleteCB parse_complete_cb);
  ~DownloadMediaParser() override;

  // Parse media metadata in a local file. All file IO will run on
  // |file_task_runner|. The metadata is parsed in an utility process safely.
  // The thumbnail is retrieved from GPU process or utility process based on
  // different codec.
  void Start();

 private:
  // MediaParserProvider implementation:
  void OnMediaParserCreated() override;
  void OnConnectionError() override;

  // Called after media metadata are parsed.
  void OnMediaMetadataParsed(
      bool parse_success,
      chrome::mojom::MediaMetadataPtr metadata,
      const std::vector<metadata::AttachedImage>& attached_images);

  // Retrieves an encoded video frame.
  void RetrieveEncodedVideoFrame();
  void OnEncodedVideoFrameRetrieved(bool success,
                                    const std::vector<uint8_t>& data,
                                    const media::VideoDecoderConfig& config);

  // Decodes the video frame.
  void OnGpuVideoAcceleratorFactoriesReady(
      std::unique_ptr<media::GpuVideoAcceleratorFactories>);
  void DecodeVideoFrame();
  void OnVideoDecoderInitialized(bool success);
  void OnVideoBufferDecoded(media::DecodeStatus status);
  void OnEosBufferDecoded(media::DecodeStatus status);
  void OnVideoFrameDecoded(
      const scoped_refptr<media::VideoFrame>& decoded_frame);
  media::mojom::InterfaceFactory* GetMediaInterfaceFactory();
  void OnDecoderConnectionError();

  // Renders the video frame to bitmap.
  void RenderVideoFrame(const scoped_refptr<media::VideoFrame>& video_frame);
  void OnBitmapGenerated(std::unique_ptr<VideoFrameThumbnailConverter>,
                         bool success,
                         SkBitmap bitmap);

  // Overlays media data source read operation. Gradually read data from media
  // file.
  void OnMediaDataReady(chrome::mojom::MediaDataSource::ReadCallback callback,
                        std::unique_ptr<std::string> data);

  void NotifyComplete(SkBitmap bitmap);
  void OnError();

  int64_t size_;
  std::string mime_type_;
  base::FilePath file_path_;

  ParseCompleteCB parse_complete_cb_;
  chrome::mojom::MediaMetadataPtr metadata_;

  // Poster images obtained with |metadata_|.
  std::vector<metadata::AttachedImage> attached_images_;

  std::unique_ptr<chrome::mojom::MediaDataSource> media_data_source_;

  // The task runner to do blocking disk IO.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Encoded video frame to be decoded. This data can be large for high
  // resolution video, should be std::move or cleared whenever possible.
  std::vector<uint8_t> encoded_data_;

  // Objects used to decode the video into media::VideoFrame.
  media::VideoDecoderConfig config_;
  std::unique_ptr<media::MojoVideoDecoder> decoder_;
  media::mojom::InterfaceFactoryPtr media_interface_factory_;
  std::unique_ptr<media::MediaInterfaceProvider> media_interface_provider_;
  std::unique_ptr<media::GpuVideoAcceleratorFactories> gpu_factories_;
  bool decode_done_;

  base::WeakPtrFactory<DownloadMediaParser> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadMediaParser);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MEDIA_PARSER_H_
