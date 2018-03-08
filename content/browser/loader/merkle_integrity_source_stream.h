// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_MERKLE_INTEGRITY_SOURCE_STREAM_H_
#define CONTENT_BROWSER_LOADER_MERKLE_INTEGRITY_SOURCE_STREAM_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "net/filter/filter_source_stream.h"

namespace content {

// MerkleIntegritySourceStream decodes and validates content encoded with the
// "mi-sha256" content encoding
// (https://tools.ietf.org/html/draft-thomson-http-mice-02).
// TODO(ksakamoto): This class should eventually live in src/net/filter/.
class CONTENT_EXPORT MerkleIntegritySourceStream
    : public net::FilterSourceStream {
 public:
  MerkleIntegritySourceStream(base::StringPiece mi_header_value,
                              std::unique_ptr<SourceStream> upstream);
  ~MerkleIntegritySourceStream() override;

  // net::FilterSourceStream
  int FilterData(net::IOBuffer* output_buffer,
                 int output_buffer_size,
                 net::IOBuffer* input_buffer,
                 int input_buffer_size,
                 int* consumed_bytes,
                 bool upstream_eof_reached) override;
  std::string GetTypeAsString() const override;

 private:
  bool ProcessInput(bool upstream_eof_reached);

  std::string input_;
  std::string output_;
  // SHA-256 hash for the next record, or empty if validation is completed.
  std::string next_proof_;
  uint64_t record_size_;
  bool failed_;

  DISALLOW_COPY_AND_ASSIGN(MerkleIntegritySourceStream);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_MERKLE_INTEGRITY_SOURCE_STREAM_H_
