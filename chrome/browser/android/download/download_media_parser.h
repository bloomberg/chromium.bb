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

namespace media {
class VideoDecoderConfig;
}  // namespace media

// Local media files parser is used to process local media files. This object
// lives on main thread in browser process.
class DownloadMediaParser : public MediaParserProvider {
 public:
  using ParseCompleteCB = base::OnceCallback<void(bool)>;

  DownloadMediaParser(int64_t size,
                      const std::string& mime_type,
                      const base::FilePath& file_path,
                      ParseCompleteCB parse_complete_cb);
  ~DownloadMediaParser() override;

  // Parse media metadata in a local file. All file IO will run on
  // |file_task_runner|. The metadata is parsed in an utility process safely.
  // However, the result is still comes from user-defined input, thus should be
  // used with caution.
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

  // Overlays media data source read operation. Gradually read data from media
  // file.
  void OnMediaDataReady(chrome::mojom::MediaDataSource::ReadCallback callback,
                        std::unique_ptr<std::string> data);

  void NotifyComplete();
  void OnError();

  int64_t size_;
  std::string mime_type_;
  base::FilePath file_path_;

  ParseCompleteCB parse_complete_cb_;
  chrome::mojom::MediaMetadataPtr metadata_;
  std::vector<metadata::AttachedImage> attached_images_;

  std::unique_ptr<chrome::mojom::MediaDataSource> media_data_source_;

  // The task runner to do blocking disk IO.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Encoded frame to be decoded. The data comes from a safe sandboxed process.
  // This data can be large for high resolution video, should be std::move or
  // cleared whenever possible.
  std::vector<uint8_t> encoded_data_;
  media::VideoDecoderConfig config_;

  base::WeakPtrFactory<DownloadMediaParser> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadMediaParser);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MEDIA_PARSER_H_
