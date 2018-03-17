// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/public/cpp/safe_media_metadata_parser.h"

#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/services/media_gallery_util/public/mojom/constants.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/blob_reader.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/connector.h"

class SafeMediaMetadataParser::MediaDataSourceImpl
    : public chrome::mojom::MediaDataSource {
 public:
  MediaDataSourceImpl(SafeMediaMetadataParser* owner,
                      chrome::mojom::MediaDataSourcePtr* interface)
      : binding_(this, mojo::MakeRequest(interface)),
        safe_media_metadata_parser_(owner) {}

  ~MediaDataSourceImpl() override = default;

 private:
  void ReadBlob(int64_t position,
                int64_t length,
                ReadBlobCallback callback) override {
    safe_media_metadata_parser_->StartBlobRequest(std::move(callback), position,
                                                  length);
  }

  mojo::Binding<chrome::mojom::MediaDataSource> binding_;
  // |safe_media_metadata_parser_| owns |this|.
  SafeMediaMetadataParser* const safe_media_metadata_parser_;

  DISALLOW_COPY_AND_ASSIGN(MediaDataSourceImpl);
};

SafeMediaMetadataParser::SafeMediaMetadataParser(
    content::BrowserContext* browser_context,
    const std::string& blob_uuid,
    int64_t blob_size,
    const std::string& mime_type,
    bool get_attached_images)
    : browser_context_(browser_context),
      blob_uuid_(blob_uuid),
      blob_size_(blob_size),
      mime_type_(mime_type),
      get_attached_images_(get_attached_images),
      weak_factory_(this) {}

SafeMediaMetadataParser::~SafeMediaMetadataParser() = default;

void SafeMediaMetadataParser::Start(service_manager::Connector* connector,
                                    DoneCallback callback) {
  DCHECK(!media_parser_ptr_);
  DCHECK(callback);

  callback_ = std::move(callback);

  connector->BindInterface(chrome::mojom::kMediaGalleryUtilServiceName,
                           mojo::MakeRequest(&media_parser_ptr_));
  // It's safe to use Unretained below as |this| owns |media_parser_ptr_|.
  media_parser_ptr_.set_connection_error_handler(
      base::BindOnce(&SafeMediaMetadataParser::ParseMediaMetadataFailed,
                     base::Unretained(this)));

  chrome::mojom::MediaDataSourcePtr source;
  media_data_source_ = std::make_unique<MediaDataSourceImpl>(this, &source);
  media_parser_ptr_->ParseMediaMetadata(
      mime_type_, blob_size_, get_attached_images_, std::move(source),
      base::BindOnce(&SafeMediaMetadataParser::ParseMediaMetadataDone,
                     base::Unretained(this)));
}

void SafeMediaMetadataParser::ParseMediaMetadataFailed() {
  media_parser_ptr_.reset();  // Terminate the utility process.
  media_data_source_.reset();

  auto metadata_dictionary = std::make_unique<base::DictionaryValue>();
  auto attached_images =
      std::make_unique<std::vector<metadata::AttachedImage>>();

  std::move(callback_).Run(/*parse_success=*/false,
                           std::move(metadata_dictionary),
                           std::move(attached_images));
}

void SafeMediaMetadataParser::ParseMediaMetadataDone(
    bool parse_success,
    std::unique_ptr<base::DictionaryValue> metadata_dictionary,
    const std::vector<metadata::AttachedImage>& attached_images) {
  media_parser_ptr_.reset();  // Terminate the utility process.
  media_data_source_.reset();

  auto attached_images_copy =
      std::make_unique<std::vector<metadata::AttachedImage>>(attached_images);

  std::move(callback_).Run(parse_success, std::move(metadata_dictionary),
                           std::move(attached_images_copy));
}

void SafeMediaMetadataParser::StartBlobRequest(
    chrome::mojom::MediaDataSource::ReadBlobCallback callback,
    int64_t position,
    int64_t length) {
  BlobReader* reader = new BlobReader(  // BlobReader is self-deleting.
      browser_context_, blob_uuid_,
      base::Bind(&SafeMediaMetadataParser::BlobReaderDone,
                 weak_factory_.GetWeakPtr(), base::Passed(&callback)));
  reader->SetByteRange(position, length);
  reader->Start();
}

void SafeMediaMetadataParser::BlobReaderDone(
    chrome::mojom::MediaDataSource::ReadBlobCallback callback,
    std::unique_ptr<std::string> data,
    int64_t /* blob_total_size */) {
  if (media_parser_ptr_)
    std::move(callback).Run(std::vector<uint8_t>(data->begin(), data->end()));
}
