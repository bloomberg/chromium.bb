// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_buffer.h"

#include "base/i18n/icu_encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/ftp/ftp_directory_listing_parser_ls.h"
#include "net/ftp/ftp_directory_listing_parser_netware.h"
#include "net/ftp/ftp_directory_listing_parser_vms.h"
#include "net/ftp/ftp_directory_listing_parser_windows.h"

namespace net {

FtpDirectoryListingBuffer::FtpDirectoryListingBuffer(
    const base::Time& current_time)
    : current_parser_(NULL) {
  parsers_.insert(new FtpDirectoryListingParserLs(current_time));
  parsers_.insert(new FtpDirectoryListingParserNetware(current_time));
  parsers_.insert(new FtpDirectoryListingParserVms());
  parsers_.insert(new FtpDirectoryListingParserWindows());
}

FtpDirectoryListingBuffer::~FtpDirectoryListingBuffer() {
  STLDeleteElements(&parsers_);
}

int FtpDirectoryListingBuffer::ConsumeData(const char* data, int data_length) {
  buffer_.append(data, data_length);

  if (!encoding_.empty() || buffer_.length() > 1024) {
    int rv = ConsumeBuffer();
    if (rv != OK)
      return rv;
  }

  return ParseLines();
}

int FtpDirectoryListingBuffer::ProcessRemainingData() {
  int rv = ConsumeBuffer();
  if (rv != OK)
    return rv;

  DCHECK(buffer_.empty());
  if (!converted_buffer_.empty())
    return ERR_INVALID_RESPONSE;

  rv = ParseLines();
  if (rv != OK)
    return rv;

  rv = OnEndOfInput();
  if (rv != OK)
    return rv;

  return OK;
}

bool FtpDirectoryListingBuffer::EntryAvailable() const {
  return (current_parser_ ? current_parser_->EntryAvailable() : false);
}

FtpDirectoryListingEntry FtpDirectoryListingBuffer::PopEntry() {
  DCHECK(EntryAvailable());
  return current_parser_->PopEntry();
}

FtpServerType FtpDirectoryListingBuffer::GetServerType() const {
  return (current_parser_ ? current_parser_->GetServerType() : SERVER_UNKNOWN);
}

int FtpDirectoryListingBuffer::DecodeBufferUsingEncoding(
    const std::string& encoding) {
  string16 converted;
  if (!base::CodepageToUTF16(buffer_,
                             encoding.c_str(),
                             base::OnStringConversionError::FAIL,
                             &converted))
    return ERR_ENCODING_CONVERSION_FAILED;

  buffer_.clear();
  converted_buffer_ += converted;
  return OK;
}

int FtpDirectoryListingBuffer::ConvertBufferToUTF16() {
  if (encoding_.empty()) {
    std::vector<std::string> encodings;
    if (!base::DetectAllEncodings(buffer_, &encodings))
      return ERR_ENCODING_DETECTION_FAILED;

    // Use first encoding that can be used to decode the buffer.
    for (size_t i = 0; i < encodings.size(); i++) {
      if (DecodeBufferUsingEncoding(encodings[i]) == OK) {
        encoding_ = encodings[i];
        return OK;
      }
    }

    return ERR_ENCODING_DETECTION_FAILED;
  }

  return DecodeBufferUsingEncoding(encoding_);
}

void FtpDirectoryListingBuffer::ExtractFullLinesFromBuffer() {
  int cut_pos = 0;
  // TODO(phajdan.jr): This code accepts all endlines matching \r*\n. Should it
  // be more strict, or enforce consistent line endings?
  for (size_t i = 0; i < converted_buffer_.length(); ++i) {
    if (converted_buffer_[i] != '\n')
      continue;
    int line_length = i - cut_pos;
    if (i >= 1 && converted_buffer_[i - 1] == '\r')
      line_length--;
    lines_.push_back(converted_buffer_.substr(cut_pos, line_length));
    cut_pos = i + 1;
  }
  converted_buffer_.erase(0, cut_pos);
}

int FtpDirectoryListingBuffer::ConsumeBuffer() {
  int rv = ConvertBufferToUTF16();
  if (rv != OK)
    return rv;

  ExtractFullLinesFromBuffer();
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

int FtpDirectoryListingBuffer::OnEndOfInput() {
  ParserSet::iterator i = parsers_.begin();
  while (i != parsers_.end()) {
    if ((*i)->OnEndOfInput()) {
      i++;
    } else {
      delete *i;
      parsers_.erase(i++);
    }
  }

  if (parsers_.size() != 1) {
    current_parser_ = NULL;

    // We may hit an ambiguity in case of listings which have no entries. That's
    // fine, as long as all remaining parsers agree that the listing is empty.
    bool all_listings_empty = true;
    for (ParserSet::iterator i = parsers_.begin(); i != parsers_.end(); ++i) {
      if ((*i)->EntryAvailable())
        all_listings_empty = false;
    }
    if (all_listings_empty)
      return OK;

    return ERR_UNRECOGNIZED_FTP_DIRECTORY_LISTING_FORMAT;
  }

  current_parser_ = *parsers_.begin();
  return OK;
}

}  // namespace net
