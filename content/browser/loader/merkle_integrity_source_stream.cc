// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/merkle_integrity_source_stream.h"

#include "base/base64url.h"
#include "base/big_endian.h"
#include "crypto/sha2.h"
#include "net/base/io_buffer.h"

namespace content {

namespace {

// Limit the record size to 16KiB to prevent browser OOM. This matches the
// maximum record size in TLS and the default maximum frame size in HTTP/2.
constexpr uint64_t kMaxRecordSize = 16 * 1024;

constexpr char kMiSha256Header[] = "mi-sha256=";
constexpr size_t kMiSha256HeaderLength = sizeof(kMiSha256Header) - 1;

}  // namespace

MerkleIntegritySourceStream::MerkleIntegritySourceStream(
    base::StringPiece mi_header_value,
    std::unique_ptr<SourceStream> upstream)
    // TODO(ksakamoto): Use appropriate SourceType.
    : net::FilterSourceStream(SourceStream::TYPE_NONE, std::move(upstream)),
      record_size_(0),
      failed_(false) {
  // TODO(ksakamoto): Support quoted parameter value.
  if (mi_header_value.size() < kMiSha256HeaderLength ||
      mi_header_value.substr(0, kMiSha256HeaderLength) != kMiSha256Header ||
      !base::Base64UrlDecode(mi_header_value.substr(kMiSha256HeaderLength),
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &next_proof_) ||
      next_proof_.size() != crypto::kSHA256Length) {
    failed_ = true;
  }
}

MerkleIntegritySourceStream::~MerkleIntegritySourceStream() = default;

int MerkleIntegritySourceStream::FilterData(net::IOBuffer* output_buffer,
                                            int output_buffer_size,
                                            net::IOBuffer* input_buffer,
                                            int input_buffer_size,
                                            int* consumed_bytes,
                                            bool upstream_eof_reached) {
  if (failed_)
    return net::ERR_CONTENT_DECODING_FAILED;

  // TODO(ksakamoto): Avoid unnecessary buffer copying.
  input_.append(input_buffer->data(), input_buffer_size);
  *consumed_bytes = input_buffer_size;

  if (!ProcessInput(upstream_eof_reached)) {
    failed_ = true;
    return net::ERR_CONTENT_DECODING_FAILED;
  }

  int bytes_out =
      std::min(output_.size(), static_cast<size_t>(output_buffer_size));
  output_.copy(output_buffer->data(), bytes_out);
  output_.erase(0, bytes_out);
  return bytes_out;
}

bool MerkleIntegritySourceStream::ProcessInput(bool upstream_eof_reached) {
  // TODO(ksakamoto): Use shift iterator or StringPiece instead of substr/erase.

  // Read the record size (the first 8 octets of the stream).
  if (!record_size_) {
    if (input_.size() < 8)
      return !upstream_eof_reached;

    base::ReadBigEndian(input_.data(), &record_size_);
    input_.erase(0, 8);
    if (record_size_ == 0)
      return false;
    if (record_size_ > kMaxRecordSize) {
      DVLOG(1)
          << "Rejecting MI content encoding because record size is too big: "
          << record_size_;
      return false;
    }
  }

  // Process records other than the last.
  while (input_.size() >= record_size_ + crypto::kSHA256Length) {
    std::string chunk = input_.substr(0, record_size_ + crypto::kSHA256Length);
    input_.erase(0, record_size_ + crypto::kSHA256Length);
    chunk.push_back('\x01');
    std::string hash = crypto::SHA256HashString(chunk);
    if (next_proof_ != hash)
      return false;
    output_.append(chunk.substr(0, record_size_));
    next_proof_ = chunk.substr(record_size_, crypto::kSHA256Length);
  }

  // Process the last record.
  if (upstream_eof_reached && !next_proof_.empty()) {
    if (input_.size() > record_size_)
      return false;

    input_.push_back('\0');
    std::string hash = crypto::SHA256HashString(input_);
    if (next_proof_ != hash)
      return false;

    output_.append(input_.substr(0, input_.size() - 1));
    input_.clear();
    next_proof_.clear();
  }
  return true;
}

std::string MerkleIntegritySourceStream::GetTypeAsString() const {
  return "MI-256";
}

}  // namespace content
