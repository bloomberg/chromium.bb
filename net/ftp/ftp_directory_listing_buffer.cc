// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/ftp/ftp_directory_listing_buffer.h"

#include "base/i18n/icu_string_conversions.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/ftp/ftp_directory_listing_parsers.h"
#include "unicode/ucsdet.h"

namespace {

// A very simple-minded character encoding detection.
// TODO(jungshik): We can apply more heuristics here (e.g. using various hints
// like TLD, the UI language/default encoding of a client, etc). In that case,
// this should be pulled out of here and moved somewhere in base because there
// can be other use cases.
std::string DetectEncoding(const std::string& text) {
  if (IsStringASCII(text))
    return std::string();
  UErrorCode status = U_ZERO_ERROR;
  UCharsetDetector* detector = ucsdet_open(&status);
  ucsdet_setText(detector, text.data(), static_cast<int32_t>(text.length()),
                 &status);
  const UCharsetMatch* match = ucsdet_detect(detector, &status);
  const char* encoding = ucsdet_getName(match, &status);
  // Should we check the quality of the match? A rather arbitrary number is
  // assigned by ICU and it's hard to come up with a lower limit.
  if (U_FAILURE(status))
    return std::string();
  return encoding;
}

}  // namespace

namespace net {

FtpDirectoryListingBuffer::FtpDirectoryListingBuffer()
    : current_parser_(NULL) {
  parsers_.insert(new FtpLsDirectoryListingParser());
  parsers_.insert(new FtpVmsDirectoryListingParser());
}

FtpDirectoryListingBuffer::~FtpDirectoryListingBuffer() {
  STLDeleteElements(&parsers_);
}

int FtpDirectoryListingBuffer::ConsumeData(const char* data, int data_length) {
  buffer_.append(data, data_length);

  if (!encoding_.empty() || buffer_.length() > 1024) {
    int rv = ExtractFullLinesFromBuffer();
    if (rv != OK)
      return rv;
  }

  return ParseLines();
}

int FtpDirectoryListingBuffer::ProcessRemainingData() {
  int rv = ExtractFullLinesFromBuffer();
  if (rv != OK)
    return rv;

  return ParseLines();
}

bool FtpDirectoryListingBuffer::EntryAvailable() const {
  return (current_parser_ ? current_parser_->EntryAvailable() : false);
}

FtpDirectoryListingEntry FtpDirectoryListingBuffer::PopEntry() {
  DCHECK(EntryAvailable());
  return current_parser_->PopEntry();
}

bool FtpDirectoryListingBuffer::ConvertToDetectedEncoding(
    const std::string& from, string16* to) {
  std::string encoding(encoding_.empty() ? "ascii" : encoding_);
  return base::CodepageToUTF16(from, encoding.c_str(),
                               base::OnStringConversionError::FAIL, to);
}

int FtpDirectoryListingBuffer::ExtractFullLinesFromBuffer() {
  if (encoding_.empty())
    encoding_ = DetectEncoding(buffer_);

  int cut_pos = 0;
  // TODO(phajdan.jr): This code accepts all endlines matching \r*\n. Should it
  // be more strict, or enforce consistent line endings?
  for (size_t i = 0; i < buffer_.length(); ++i) {
    if (buffer_[i] != '\n')
      continue;
    int line_length = i - cut_pos;
    if (i >= 1 && buffer_[i - 1] == '\r')
      line_length--;
    std::string line(buffer_.substr(cut_pos, line_length));
    cut_pos = i + 1;
    string16 line_converted;
    if (!ConvertToDetectedEncoding(line, &line_converted)) {
      buffer_.erase(0, cut_pos);
      return ERR_ENCODING_CONVERSION_FAILED;
    }
    lines_.push_back(line_converted);
  }
  buffer_.erase(0, cut_pos);
  return OK;
}

int FtpDirectoryListingBuffer::ParseLines() {
  while (!lines_.empty()) {
    string16 line = lines_.front();
    lines_.pop_front();
    if (current_parser_) {
      if (!current_parser_->ConsumeLine(line))
        return ERR_FAILED;
    } else {
      ParserSet::iterator i = parsers_.begin();
      while (i != parsers_.end()) {
        if ((*i)->ConsumeLine(line)) {
          i++;
        } else {
          delete *i;
          parsers_.erase(i++);
        }
      }
      if (parsers_.empty())
        return ERR_UNRECOGNIZED_FTP_DIRECTORY_LISTING_FORMAT;
      if (parsers_.size() == 1)
        current_parser_ = *parsers_.begin();
    }
  }

  return OK;
}

}  // namespace net
