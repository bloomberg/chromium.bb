// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_mlsd.h"

#include <map>
#include <vector>

#include "base/stl_util-inl.h"
#include "base/string_util.h"

// You can read the specification of the MLSD format at
// http://tools.ietf.org/html/rfc3659#page-23.

namespace {

// The MLSD date listing is specified at
// http://tools.ietf.org/html/rfc3659#page-6.
bool MlsdDateListingToTime(const string16& text, base::Time* time) {
  base::Time::Exploded time_exploded = { 0 };

  // We will only test 12 characters, but RFC-3659 requires 14 (we ignore the
  // last two digits, which contain the number of seconds).
  if (text.length() < 14)
    return false;

  if (!StringToInt(text.substr(0, 4), &time_exploded.year))
    return false;
  if (!StringToInt(text.substr(4, 2), &time_exploded.month))
    return false;
  if (!StringToInt(text.substr(6, 2), &time_exploded.day_of_month))
    return false;
  if (!StringToInt(text.substr(8, 2), &time_exploded.hour))
    return false;
  if (!StringToInt(text.substr(10, 2), &time_exploded.minute))
    return false;

  // We don't know the time zone of the server, so just use local time.
  *time = base::Time::FromLocalExploded(time_exploded);
  return true;
}

}  // namespace

namespace net {

FtpDirectoryListingParserMlsd::FtpDirectoryListingParserMlsd() {
}

bool FtpDirectoryListingParserMlsd::ConsumeLine(const string16& line) {
  // The first space indicates where the filename begins.
  string16::size_type first_space_pos = line.find(' ');
  if (first_space_pos == string16::npos || first_space_pos < 1)
    return false;

  string16 facts_string = line.substr(0, first_space_pos - 1);
  string16 filename = line.substr(first_space_pos + 1);
  std::vector<string16> facts_split;
  SplitString(facts_string, ';', &facts_split);

  const char* keys[] = {
      "modify",
      "size",
      "type",
  };

  std::map<std::string, string16> facts;
  for (std::vector<string16>::const_iterator i = facts_split.begin();
       i != facts_split.end(); ++i) {
    string16::size_type equal_sign_pos = i->find('=');
    if (equal_sign_pos == string16::npos)
      return false;
    string16 key = i->substr(0, equal_sign_pos);
    string16 value = i->substr(equal_sign_pos + 1);

    // If we're interested in a key, record its value. Note that we don't detect
    // a case when the server is sending duplicate keys. We're not validating
    // the input, just parsing it.
    for (size_t j = 0; j < arraysize(keys); j++)
      if (LowerCaseEqualsASCII(key, keys[j]))
        facts[keys[j]] = value;
  }
  if (!ContainsKey(facts, "type"))
    return false;

  FtpDirectoryListingEntry entry;
  entry.name = filename;

  if (LowerCaseEqualsASCII(facts["type"], "dir")) {
    entry.type = FtpDirectoryListingEntry::DIRECTORY;
    entry.size = -1;
  } else if (LowerCaseEqualsASCII(facts["type"], "file")) {
    entry.type = FtpDirectoryListingEntry::FILE;
    if (!ContainsKey(facts, "size"))
      return false;
    if (!StringToInt64(facts["size"], &entry.size))
      return false;
  } else {
    // Ignore other types of entries. They are either not interesting for us
    // (cdir, pdir), or not regular files (OS-specific types). There is no
    // specific type for symlink. Symlinks get a type of their target.
    return true;
  }

  if (!ContainsKey(facts, "modify"))
    return false;
  if (!MlsdDateListingToTime(facts["modify"], &entry.last_modified))
    return false;

  entries_.push(entry);
  return true;
}

bool FtpDirectoryListingParserMlsd::OnEndOfInput() {
  return true;
}

bool FtpDirectoryListingParserMlsd::EntryAvailable() const {
  return !entries_.empty();
}

FtpDirectoryListingEntry FtpDirectoryListingParserMlsd::PopEntry() {
  DCHECK(EntryAvailable());
  FtpDirectoryListingEntry entry = entries_.front();
  entries_.pop();
  return entry;
}

}  // namespace net
