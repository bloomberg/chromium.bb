// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/zip_analyzer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>

#include "base/i18n/streaming_utf8_validator.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/safe_browsing/csd.pb.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "third_party/zlib/google/zip_reader.h"

namespace safe_browsing {
namespace zip_analyzer {

namespace {

// A writer delegate that computes a SHA-256 hash digest over the data while
// writing it to a file.
class HashingFileWriter : public zip::FileWriterDelegate {
 public:
  explicit HashingFileWriter(base::File* file);

  // zip::FileWriterDelegate methods:
  bool WriteBytes(const char* data, int num_bytes) override;

  void ComputeDigest(uint8_t* digest, size_t digest_length);

 private:
  std::unique_ptr<crypto::SecureHash> sha256_;

  DISALLOW_COPY_AND_ASSIGN(HashingFileWriter);
};

HashingFileWriter::HashingFileWriter(base::File* file)
    : zip::FileWriterDelegate(file),
      sha256_(crypto::SecureHash::Create(crypto::SecureHash::SHA256)) {
}

bool HashingFileWriter::WriteBytes(const char* data, int num_bytes) {
  if (!zip::FileWriterDelegate::WriteBytes(data, num_bytes))
    return false;
  sha256_->Update(data, num_bytes);
  return true;
}

void HashingFileWriter::ComputeDigest(uint8_t* digest, size_t digest_length) {
  sha256_->Finish(digest, digest_length);
}

void AnalyzeContainedFile(
    const scoped_refptr<BinaryFeatureExtractor>& binary_feature_extractor,
    const base::FilePath& file_path,
    zip::ZipReader* reader,
    base::File* temp_file,
    ClientDownloadRequest::ArchivedBinary* archived_binary) {
  std::string file_basename(file_path.BaseName().AsUTF8Unsafe());
  if (base::StreamingUtf8Validator::Validate(file_basename))
    archived_binary->set_file_basename(file_basename);
  archived_binary->set_download_type(
      download_protection_util::GetDownloadType(file_path));
  archived_binary->set_length(reader->current_entry_info()->original_size());
  HashingFileWriter writer(temp_file);
  if (reader->ExtractCurrentEntry(&writer)) {
    uint8_t digest[crypto::kSHA256Length];
    writer.ComputeDigest(&digest[0], arraysize(digest));
    archived_binary->mutable_digests()->set_sha256(&digest[0],
                                                   arraysize(digest));
    if (!binary_feature_extractor->ExtractImageFeaturesFromFile(
            temp_file->Duplicate(),
            BinaryFeatureExtractor::kDefaultOptions,
            archived_binary->mutable_image_headers(),
            archived_binary->mutable_signature()->mutable_signed_data())) {
      archived_binary->clear_image_headers();
      archived_binary->clear_signature();
    } else if (!archived_binary->signature().signed_data_size()) {
      // No SignedData blobs were extracted, so clear the signature field.
      archived_binary->clear_signature();
    }
  }
}

}  // namespace

void AnalyzeZipFile(base::File zip_file,
                    base::File temp_file,
                    ArchiveAnalyzerResults* results) {
  std::set<base::FilePath> archived_archive_filenames;
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor(
      new BinaryFeatureExtractor());
  zip::ZipReader reader;
  if (!reader.OpenFromPlatformFile(zip_file.GetPlatformFile())) {
    DVLOG(1) << "Failed to open zip file";
    return;
  }

  bool advanced = true;
  for (; reader.HasMore(); advanced = reader.AdvanceToNextEntry()) {
    if (!advanced) {
      DVLOG(1) << "Could not advance to next entry, aborting zip scan.";
      return;
    }
    if (!reader.OpenCurrentEntryInZip()) {
      DVLOG(1) << "Failed to open current entry in zip file";
      continue;
    }
    const base::FilePath& file = reader.current_entry_info()->file_path();
    if (FileTypePolicies::GetInstance()->IsArchiveFile(file)) {
      DVLOG(2) << "Downloaded a zipped archive: " << file.value();
      results->has_archive = true;
      archived_archive_filenames.insert(file.BaseName());
      ClientDownloadRequest::ArchivedBinary* archived_archive =
          results->archived_binary.Add();
      std::string file_basename_utf8(file.BaseName().AsUTF8Unsafe());
      if (base::StreamingUtf8Validator::Validate(file_basename_utf8))
        archived_archive->set_file_basename(file_basename_utf8);
      archived_archive->set_download_type(
          ClientDownloadRequest::ZIPPED_ARCHIVE);
    } else if (FileTypePolicies::GetInstance()->IsCheckedBinaryFile(file)) {
      DVLOG(2) << "Downloaded a zipped executable: " << file.value();
      results->has_executable = true;
      AnalyzeContainedFile(binary_feature_extractor, file, &reader, &temp_file,
                           results->archived_binary.Add());
    } else {
      DVLOG(3) << "Ignoring non-binary file: " << file.value();
    }
  }
  results->archived_archive_filenames.assign(archived_archive_filenames.begin(),
                                             archived_archive_filenames.end());
  results->success = true;
}

}  // namespace zip_analyzer
}  // namespace safe_browsing
