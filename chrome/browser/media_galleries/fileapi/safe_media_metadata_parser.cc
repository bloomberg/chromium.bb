// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_media_metadata_parser.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/blob_reader.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/l10n/l10n_util.h"

namespace metadata {

class SafeMediaMetadataParser::MediaDataSourceImpl
    : public extensions::mojom::MediaDataSource {
 public:
  MediaDataSourceImpl(SafeMediaMetadataParser* owner,
                      extensions::mojom::MediaDataSourcePtr* interface)
      : binding_(this, interface), safe_media_metadata_parser_(owner) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  }

  ~MediaDataSourceImpl() override = default;

 private:
  void ReadBlob(int64_t position,
                int64_t length,
                const ReadBlobCallback& callback) override {
    safe_media_metadata_parser_->StartBlobRequest(callback, position, length);
  }

  mojo::Binding<extensions::mojom::MediaDataSource> binding_;
  // |safe_media_metadata_parser_| owns |this|.
  SafeMediaMetadataParser* const safe_media_metadata_parser_;

  DISALLOW_COPY_AND_ASSIGN(MediaDataSourceImpl);
};

SafeMediaMetadataParser::SafeMediaMetadataParser(Profile* profile,
                                                 const std::string& blob_uuid,
                                                 int64_t blob_size,
                                                 const std::string& mime_type,
                                                 bool get_attached_images)
    : profile_(profile),
      blob_uuid_(blob_uuid),
      blob_size_(blob_size),
      mime_type_(mime_type),
      get_attached_images_(get_attached_images) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void SafeMediaMetadataParser::Start(const DoneCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeMediaMetadataParser::StartOnIOThread, this, callback));
}

SafeMediaMetadataParser::~SafeMediaMetadataParser() = default;

void SafeMediaMetadataParser::StartOnIOThread(const DoneCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!utility_process_mojo_client_);
  DCHECK(!callback.is_null());

  callback_ = callback;

  const base::string16 utility_process_name =
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_MEDIA_FILE_CHECKER_NAME);

  utility_process_mojo_client_.reset(
      new content::UtilityProcessMojoClient<extensions::mojom::MediaParser>(
          utility_process_name));
  utility_process_mojo_client_->set_error_callback(
      base::Bind(&SafeMediaMetadataParser::ParseMediaMetadataFailed, this));

  utility_process_mojo_client_->Start();  // Start the utility process.

  extensions::mojom::MediaDataSourcePtr source;
  media_data_source_ = base::MakeUnique<MediaDataSourceImpl>(this, &source);

  utility_process_mojo_client_->service()->ParseMediaMetadata(
      mime_type_, blob_size_, get_attached_images_, std::move(source),
      base::Bind(&SafeMediaMetadataParser::ParseMediaMetadataDone, this));
}

void SafeMediaMetadataParser::ParseMediaMetadataFailed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  utility_process_mojo_client_.reset();  // Terminate the utility process.
  media_data_source_.reset();

  std::unique_ptr<base::DictionaryValue> metadata_dictionary =
      base::MakeUnique<base::DictionaryValue>();
  std::unique_ptr<std::vector<metadata::AttachedImage>> attached_images =
      base::MakeUnique<std::vector<metadata::AttachedImage>>();

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback_, false, base::Passed(&metadata_dictionary),
                 base::Passed(&attached_images)));
}

void SafeMediaMetadataParser::ParseMediaMetadataDone(
    bool parse_success,
    std::unique_ptr<base::DictionaryValue> metadata_dictionary,
    const std::vector<AttachedImage>& attached_images) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  utility_process_mojo_client_.reset();  // Terminate the utility process.
  media_data_source_.reset();

  // We need to make a scoped copy of this vector since it will be destroyed
  // at the end of the handler.
  std::unique_ptr<std::vector<metadata::AttachedImage>> attached_images_copy =
      base::MakeUnique<std::vector<metadata::AttachedImage>>(attached_images);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback_, parse_success, base::Passed(&metadata_dictionary),
                 base::Passed(&attached_images_copy)));
}

void SafeMediaMetadataParser::StartBlobRequest(
    const extensions::mojom::MediaDataSource::ReadBlobCallback& callback,
    int64_t position,
    int64_t length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SafeMediaMetadataParser::StartBlobReaderOnUIThread, this,
                 callback, position, length));
}

void SafeMediaMetadataParser::StartBlobReaderOnUIThread(
    const extensions::mojom::MediaDataSource::ReadBlobCallback& callback,
    int64_t position,
    int64_t length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  BlobReader* reader = new BlobReader(  // BlobReader is self-deleting.
      profile_, blob_uuid_,
      base::Bind(&SafeMediaMetadataParser::BlobReaderDoneOnUIThread, this,
                 callback));
  reader->SetByteRange(position, length);
  reader->Start();
}

void SafeMediaMetadataParser::BlobReaderDoneOnUIThread(
    const extensions::mojom::MediaDataSource::ReadBlobCallback& callback,
    std::unique_ptr<std::string> data,
    int64_t /* blob_total_size */) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeMediaMetadataParser::FinishBlobRequest, this, callback,
                 base::Passed(std::move(data))));
}

void SafeMediaMetadataParser::FinishBlobRequest(
    const extensions::mojom::MediaDataSource::ReadBlobCallback& callback,
    std::unique_ptr<std::string> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (utility_process_mojo_client_)
    callback.Run(std::vector<uint8_t>(data->begin(), data->end()));
}

}  // namespace metadata
