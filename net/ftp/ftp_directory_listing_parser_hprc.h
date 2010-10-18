// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_HPRC_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_HPRC_H_
#pragma once

#include <queue>

#include "base/basictypes.h"
#include "base/time.h"
#include "net/ftp/ftp_directory_listing_parser.h"

namespace net {

// Parser for directory listings served by HPRC,
// see http://hprc.external.hp.com/ and http://crbug.com/56547.
class FtpDirectoryListingParserHprc : public FtpDirectoryListingParser {
 public:
  // Constructor. When we need to provide the last modification time
  // that we don't know, |current_time| will be used. This allows passing
  // a specific date during testing.
  explicit FtpDirectoryListingParserHprc(const base::Time& current_time);
  virtual ~FtpDirectoryListingParserHprc();

  // FtpDirectoryListingParser methods:
  virtual FtpServerType GetServerType() const;
  virtual bool ConsumeLine(const string16& line);
  virtual bool OnEndOfInput();
  virtual bool EntryAvailable() const;
  virtual FtpDirectoryListingEntry PopEntry();

 private:
  // Store the current time. We use it in place of last modification time
  // that is unknown (the server doesn't send it).
  const base::Time current_time_;

  std::queue<FtpDirectoryListingEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(FtpDirectoryListingParserHprc);
};

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_HPRC_H_
