// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_hprc.h"

#include "base/logging.h"
#include "base/time.h"

namespace net {

FtpDirectoryListingParserHprc::FtpDirectoryListingParserHprc(
    const base::Time& current_time)
    : current_time_(current_time) {
}

FtpServerType FtpDirectoryListingParserHprc::GetServerType() const {
  return SERVER_HPRC;
}

bool FtpDirectoryListingParserHprc::ConsumeLine(const string16& line) {
  if (line.empty())
    return false;

  // All lines begin with a space.
  if (line[0] != ' ')
    return false;

  FtpDirectoryListingEntry entry;
  entry.name = line.substr(1);

  // We don't know anything beyond the file name, so just pick some arbitrary
  // values.
  // TODO(phajdan.jr): consider adding an UNKNOWN entry type.
  entry.type = FtpDirectoryListingEntry::FILE;
  entry.size = 0;
  entry.last_modified = current_time_;

  entries_.push(entry);
  return true;
}

bool FtpDirectoryListingParserHprc::OnEndOfInput() {
  return true;
}

bool FtpDirectoryListingParserHprc::EntryAvailable() const {
  return !entries_.empty();
}

FtpDirectoryListingEntry FtpDirectoryListingParserHprc::PopEntry() {
  DCHECK(EntryAvailable());
  FtpDirectoryListingEntry entry = entries_.front();
  entries_.pop();
  return entry;
}

}  // namespace net
