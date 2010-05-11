// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_BUFFER_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_BUFFER_H_

#include <deque>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/time.h"
#include "net/ftp/ftp_server_type_histograms.h"

namespace net {

struct FtpDirectoryListingEntry;
class FtpDirectoryListingParser;

class FtpDirectoryListingBuffer {
 public:
  // Constructor. When the current time is needed to guess the year on partial
  // date strings, |current_time| will be used. This allows passing a specific
  // date during testing.
  explicit FtpDirectoryListingBuffer(const base::Time& current_time);
  ~FtpDirectoryListingBuffer();

  // Called when data is received from the data socket. Returns network
  // error code.
  int ConsumeData(const char* data, int data_length);

  // Called when all received data has been consumed by this buffer. Tells the
  // buffer to try to parse remaining raw data and returns network error code.
  int ProcessRemainingData();

  bool EntryAvailable() const;

  // Returns the next entry. It is an error to call this function
  // unless EntryAvailable returns true.
  FtpDirectoryListingEntry PopEntry();

  // Returns recognized server type. It is valid to call this function at any
  // time, although it will return SERVER_UNKNOWN if it doesn't know the answer.
  FtpServerType GetServerType() const;

  const std::string& encoding() const { return encoding_; }

 private:
  typedef std::set<FtpDirectoryListingParser*> ParserSet;

  // Converts the string |from| to detected encoding and stores it in |to|.
  // Returns true on success.
  bool ConvertToDetectedEncoding(const std::string& from, string16* to);

  // Tries to extract full lines from the raw buffer, converting them to the
  // detected encoding. Returns network error code.
  int ExtractFullLinesFromBuffer();

  // Tries to parse full lines stored in |lines_|. Returns network error code.
  int ParseLines();

  // Called when we received the entire input. Propagates that info to remaining
  // parsers. Returns network error code.
  int OnEndOfInput();

  // Detected encoding of the response (empty if unknown or ASCII).
  std::string encoding_;

  // Buffer to keep not-yet-split data.
  std::string buffer_;

  // CRLF-delimited lines, without the CRLF, not yet consumed by parser.
  std::deque<string16> lines_;

  // A collection of parsers for different listing styles. The parsers are owned
  // by this FtpDirectoryListingBuffer.
  ParserSet parsers_;

  // When we're sure about the listing format, its parser is stored in
  // |current_parser_|.
  FtpDirectoryListingParser* current_parser_;

  DISALLOW_COPY_AND_ASSIGN(FtpDirectoryListingBuffer);
};

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_BUFFER_H_
