// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_MLSD_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_MLSD_H_
#pragma once

#include <queue>

#include "net/ftp/ftp_directory_listing_parser.h"

namespace net {

// Parser for MLSD directory listing (RFC-3659). For more info see
// http://tools.ietf.org/html/rfc3659#page-23.
class FtpDirectoryListingParserMlsd : public FtpDirectoryListingParser {
 public:
  FtpDirectoryListingParserMlsd();

  // FtpDirectoryListingParser methods:
  virtual FtpServerType GetServerType() const { return SERVER_MLSD; }
  virtual bool ConsumeLine(const string16& line);
  virtual bool OnEndOfInput();
  virtual bool EntryAvailable() const;
  virtual FtpDirectoryListingEntry PopEntry();

 private:
  std::queue<FtpDirectoryListingEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(FtpDirectoryListingParserMlsd);
};

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_MLSD_H_
