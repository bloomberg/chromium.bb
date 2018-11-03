// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/gzip_source_stream.h"

#include <utility>

#include "third_party/zlib/zlib.h"

namespace net {

namespace {

const char kDeflate[] = "DEFLATE";
const char kGzip[] = "GZIP";

}  // namespace

GzipSourceStream::~GzipSourceStream() = default;

std::unique_ptr<GzipSourceStream> GzipSourceStream::Create(
    std::unique_ptr<SourceStream> upstream,
    SourceStream::SourceType type) {
  DCHECK(type == TYPE_GZIP || type == TYPE_DEFLATE);
  std::unique_ptr<GzipSourceStream> source(
      new GzipSourceStream(std::move(upstream), type));

  if (!source->Init())
    return nullptr;
  return source;
}

GzipSourceStream::GzipSourceStream(std::unique_ptr<SourceStream> upstream,
                                   SourceStream::SourceType type)
    : FilterSourceStream(type, std::move(upstream)),
      zlib_stream_(type == TYPE_DEFLATE
                       ? ZLibStreamWrapper::SourceType::kDeflate
                       : ZLibStreamWrapper::SourceType::kGzip) {}

bool GzipSourceStream::Init() {
  return zlib_stream_.Init();
}

std::string GzipSourceStream::GetTypeAsString() const {
  switch (type()) {
    case TYPE_GZIP:
      return kGzip;
    case TYPE_DEFLATE:
      return kDeflate;
    default:
      NOTREACHED();
      return "";
  }
}

int GzipSourceStream::FilterData(IOBuffer* output_buffer,
                                 int output_buffer_size,
                                 IOBuffer* input_buffer,
                                 int input_buffer_size,
                                 int* consumed_bytes,
                                 bool upstream_end_reached) {
  return zlib_stream_.FilterData(output_buffer, output_buffer_size,
                                 input_buffer, input_buffer_size,
                                 consumed_bytes, upstream_end_reached);
}

}  // namespace net
