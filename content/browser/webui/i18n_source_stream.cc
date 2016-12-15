// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/i18n_source_stream.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "net/base/io_buffer.h"

namespace content {

I18nSourceStream::~I18nSourceStream() {}

std::unique_ptr<I18nSourceStream> I18nSourceStream::Create(
    std::unique_ptr<SourceStream> upstream,
    SourceStream::SourceType type,
    const ui::TemplateReplacements* replacements) {
  DCHECK(replacements);
  std::unique_ptr<I18nSourceStream> source(
      new I18nSourceStream(std::move(upstream), type, replacements));
  return source;
}

I18nSourceStream::I18nSourceStream(std::unique_ptr<SourceStream> upstream,
                                   SourceStream::SourceType type,
                                   const ui::TemplateReplacements* replacements)
    : FilterSourceStream(type, std::move(upstream)),
      drain_offset_(0),
      replacements_(replacements) {}

std::string I18nSourceStream::GetTypeAsString() const {
  return "i18n";
}

int I18nSourceStream::FilterData(net::IOBuffer* output_buffer,
                                 int output_buffer_size,
                                 net::IOBuffer* input_buffer,
                                 int input_buffer_size,
                                 int* consumed_bytes,
                                 bool upstream_end_reached) {
  DCHECK(output_.empty() || (upstream_end_reached && input_buffer_size == 0));
  *consumed_bytes = input_buffer_size;
  // TODO(dschuyler): Perform replacements without accumulation.
  // Accumulate.
  input_.append(input_buffer->data(), input_buffer_size);
  if (upstream_end_reached && !drain_offset_ && output_.empty()) {
    // Process.
    output_ = ui::ReplaceTemplateExpressions(input_, *replacements_);
  }

  if (drain_offset_ == output_.size())
    return 0;

  // Drain.
  int bytes_out = std::min(output_.size() - drain_offset_,
                           static_cast<size_t>(output_buffer_size));
  output_.copy(output_buffer->data(), bytes_out, drain_offset_);
  drain_offset_ += bytes_out;
  return bytes_out;
}

}  // namespace content
