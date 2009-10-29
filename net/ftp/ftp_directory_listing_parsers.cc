// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/ftp/ftp_directory_listing_parsers.h"

#include "base/string_util.h"

namespace {
  
bool LooksLikeUnixPermission(const string16& text) {
  if (text.length() != 3)
    return false;
  
  return ((text[0] == 'r' || text[0] == '-') &&
          (text[1] == 'w' || text[1] == '-') &&
          (text[2] == 'x' || text[2] == 's' || text[2] == 'S' ||
           text[2] == '-'));
}

bool LooksLikeUnixPermissionsListing(const string16& text) {
  if (text.length() != 10)
    return false;
  
  if (text[0] != 'b' && text[0] != 'c' && text[0] != 'd' &&
      text[0] != 'l' && text[0] != 'p' && text[0] != 's' &&
      text[0] != '-')
    return false;
  
  return (LooksLikeUnixPermission(text.substr(1, 3)) &&
          LooksLikeUnixPermission(text.substr(4, 3)) &&
          LooksLikeUnixPermission(text.substr(7, 3)));
}
  
bool IsStringNonNegativeNumber(const string16& text) {
  int number;
  if (!StringToInt(text, &number))
    return false;
  
  return number >= 0;
}
  
bool ThreeLetterMonthToNumber(const string16& text, int* number) {
  const static char* months[] = { "jan", "feb", "mar", "apr", "may", "jun",
                                  "jul", "aug", "sep", "oct", "nov", "dec" };
  
  for (size_t i = 0; i < arraysize(months); i++) {
    if (LowerCaseEqualsASCII(text, months[i])) {
      *number = i + 1;
      return true;
    }
  }
  
  return false;
}
  
bool UnixDateListingToTime(const std::vector<string16>& columns,
                           base::Time* time) {
  DCHECK_EQ(9U, columns.size());
  
  base::Time::Exploded time_exploded = { 0 };
  
  if (!ThreeLetterMonthToNumber(columns[5], &time_exploded.month))
    return false;
  
  if (!StringToInt(columns[6], &time_exploded.day_of_month))
    return false;
  
  if (!StringToInt(columns[7], &time_exploded.year)) {
    // Maybe it's time. Does it look like time (MM:HH)?
    if (columns[7].length() != 5 || columns[7][2] != ':')
      return false;
    
    if (!StringToInt(columns[7].substr(0, 2), &time_exploded.hour))
      return false;
    
    if (!StringToInt(columns[7].substr(3, 2), &time_exploded.minute))
      return false;
    
    // Use current year.
    base::Time::Exploded now_exploded;
    base::Time::Now().LocalExplode(&now_exploded);
    time_exploded.year = now_exploded.year;
  }
  
  // We don't know the time zone of the server, so just use local time.
  *time = base::Time::FromLocalExploded(time_exploded);
  return true;
}
  
}  // namespace

namespace net {

FtpDirectoryListingParser::~FtpDirectoryListingParser() {
}
  
FtpLsDirectoryListingParser::FtpLsDirectoryListingParser() {
}

bool FtpLsDirectoryListingParser::ConsumeLine(const string16& line) {
  std::vector<string16> columns;
  SplitString(CollapseWhitespace(line, false), ' ', &columns);
  if (columns.size() == 11) {
    // Check if it is a symlink.
    if (columns[9] != ASCIIToUTF16("->"))
      return false;
    
    // Drop the symlink target from columns, we don't use it.
    columns.resize(9);
  }
  
  if (columns.size() != 9)
    return false;
  
  if (!LooksLikeUnixPermissionsListing(columns[0]))
    return false;
  
  FtpDirectoryListingEntry entry;
  if (columns[0][0] == 'l') {
    entry.type = FtpDirectoryListingEntry::SYMLINK;
  } else if (columns[0][0] == 'd') {
    entry.type = FtpDirectoryListingEntry::DIRECTORY;
  } else {
    entry.type = FtpDirectoryListingEntry::FILE;
  }
  
  if (!IsStringNonNegativeNumber(columns[1]))
    return false;
  
  if (!StringToInt64(columns[4], &entry.size))
    return false;
  if (entry.size < 0)
    return false;
  if (entry.type != FtpDirectoryListingEntry::FILE)
    entry.size = -1;
  
  if (!UnixDateListingToTime(columns, &entry.last_modified))
    return false;
  
  entry.name = columns[8];
  
  entries_.push(entry);
  return true;
}
  
bool FtpLsDirectoryListingParser::EntryAvailable() const {
  return !entries_.empty();
}
  
FtpDirectoryListingEntry FtpLsDirectoryListingParser::PopEntry() {
  FtpDirectoryListingEntry entry = entries_.front();
  entries_.pop();
  return entry;
}

}  // namespace net
