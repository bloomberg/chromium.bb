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

bool IsStringNonNegativeInteger(const string16& text) {
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

bool WindowsDateListingToTime(const std::vector<string16>& columns,
                              base::Time* time) {
  DCHECK_EQ(4U, columns.size());

  base::Time::Exploded time_exploded = { 0 };

  // Date should be in format MM-DD-YY[YY].
  std::vector<string16> date_parts;
  SplitString(columns[0], '-', &date_parts);
  if (date_parts.size() != 3)
    return false;
  if (!StringToInt(date_parts[0], &time_exploded.month))
    return false;
  if (!StringToInt(date_parts[1], &time_exploded.day_of_month))
    return false;
  if (!StringToInt(date_parts[2], &time_exploded.year))
    return false;
  if (time_exploded.year < 0)
    return false;
  // If year has only two digits then assume that 00-79 is 2000-2079,
  // and 80-99 is 1980-1999.
  if (time_exploded.year < 80)
    time_exploded.year += 2000;
  else if (time_exploded.year < 100)
    time_exploded.year += 1900;

  // Time should be in format HH:MM(AM|PM)
  if (columns[1].length() != 7)
    return false;
  std::vector<string16> time_parts;
  SplitString(columns[1].substr(0, 5), ':', &time_parts);
  if (time_parts.size() != 2)
    return false;
  if (!StringToInt(time_parts[0], &time_exploded.hour))
    return false;
  if (!StringToInt(time_parts[1], &time_exploded.minute))
    return false;
  string16 am_or_pm(columns[1].substr(5, 2));
  if (EqualsASCII(am_or_pm, "PM"))
    time_exploded.hour += 12;
  else if (!EqualsASCII(am_or_pm, "AM"))
    return false;

  // We don't know the time zone of the server, so just use local time.
  *time = base::Time::FromLocalExploded(time_exploded);
  return true;
}

// Converts the filename component in listing to the filename we can display.
// Returns true on success.
bool ParseVmsFilename(const string16& raw_filename, string16* parsed_filename,
                      bool* is_directory) {
  // On VMS, the files and directories are versioned. The version number is
  // separated from the file name by a semicolon. Example: ANNOUNCE.TXT;2.
  std::vector<string16> listing_parts;
  SplitString(raw_filename, ';', &listing_parts);
  if (listing_parts.size() != 2)
    return false;
  if (!IsStringNonNegativeInteger(listing_parts[1]))
    return false;

  // Even directories have extensions in the listings. Don't display extensions
  // for directories; it's awkward for non-VMS users. Also, VMS is
  // case-insensitive, but generally uses uppercase characters. This may look
  // awkward, so we convert them to lower case.
  std::vector<string16> filename_parts;
  SplitString(listing_parts[0], '.', &filename_parts);
  if (filename_parts.size() != 2)
    return false;
  if (EqualsASCII(filename_parts[1], "DIR")) {
    *parsed_filename = StringToLowerASCII(filename_parts[0]);
    *is_directory = true;
  } else {
    *parsed_filename = StringToLowerASCII(listing_parts[0]);
    *is_directory = false;
  }
  return true;
}

bool ParseVmsFilesize(const string16& input, int64* size) {
  // VMS's directory listing gives us file size in blocks. We assume that
  // the block size is 512 bytes. It doesn't give accurate file size, but is the
  // best information we have.
  const int kBlockSize = 512;

  if (StringToInt64(input, size)) {
    *size *= kBlockSize;
    return true;
  }

  std::vector<string16> parts;
  SplitString(input, '/', &parts);
  if (parts.size() != 2)
    return false;

  int64 blocks_used, blocks_allocated;
  if (!StringToInt64(parts[0], &blocks_used))
    return false;
  if (!StringToInt64(parts[1], &blocks_allocated))
    return false;
  if (blocks_used > blocks_allocated)
    return false;

  *size = blocks_used * kBlockSize;
  return true;
}

bool LooksLikeVmsFileProtectionListingPart(const string16& input) {
  if (input.length() > 4)
    return false;

  // On VMS there are four different permission bits: Read, Write, Execute,
  // and Delete. They appear in that order in the permission listing.
  std::string pattern("RWED");
  string16 match(input);
  while (!match.empty() && !pattern.empty()) {
    if (match[0] == pattern[0])
      match = match.substr(1);
    pattern = pattern.substr(1);
  }
  return match.empty();
}

bool LooksLikeVmsFileProtectionListing(const string16& input) {
  if (input.length() < 2)
    return false;
  if (input[0] != '(' || input[input.length() - 1] != ')')
    return false;

  // We expect four parts of the file protection listing: for System, Owner,
  // Group, and World.
  std::vector<string16> parts;
  SplitString(input.substr(1, input.length() - 2), ',', &parts);
  if (parts.size() != 4)
    return false;

  return LooksLikeVmsFileProtectionListingPart(parts[0]) &&
      LooksLikeVmsFileProtectionListingPart(parts[1]) &&
      LooksLikeVmsFileProtectionListingPart(parts[2]) &&
      LooksLikeVmsFileProtectionListingPart(parts[3]);
}

bool LooksLikeVmsUserIdentificationCode(const string16& input) {
  if (input.length() < 2)
    return false;
  return input[0] == '[' && input[input.length() - 1] == ']';
}

bool VmsDateListingToTime(const std::vector<string16>& columns,
                          base::Time* time) {
  DCHECK_EQ(3U, columns.size());

  base::Time::Exploded time_exploded = { 0 };

  // Date should be in format DD-MMM-YYYY.
  std::vector<string16> date_parts;
  SplitString(columns[1], '-', &date_parts);
  if (date_parts.size() != 3)
    return false;
  if (!StringToInt(date_parts[0], &time_exploded.day_of_month))
    return false;
  if (!ThreeLetterMonthToNumber(date_parts[1], &time_exploded.month))
    return false;
  if (!StringToInt(date_parts[2], &time_exploded.year))
    return false;

  // Time can be in format HH:MM, HH:MM:SS, or HH:MM:SS.mm. Try to recognize the
  // last type first. Do not parse the seconds, they will be ignored anyway.
  string16 time_column(columns[2]);
  if (time_column.length() == 11 && time_column[8] == '.')
    time_column = time_column.substr(0, 8);
  if (time_column.length() == 8 && time_column[5] == ':')
    time_column = time_column.substr(0, 5);
  if (time_column.length() != 5)
    return false;
  std::vector<string16> time_parts;
  SplitString(time_column, ':', &time_parts);
  if (time_parts.size() != 2)
    return false;
  if (!StringToInt(time_parts[0], &time_exploded.hour))
    return false;
  if (!StringToInt(time_parts[1], &time_exploded.minute))
    return false;

  // We don't know the time zone of the server, so just use local time.
  *time = base::Time::FromLocalExploded(time_exploded);
  return true;
}

}  // namespace

namespace net {

FtpDirectoryListingParser::~FtpDirectoryListingParser() {
}

FtpLsDirectoryListingParser::FtpLsDirectoryListingParser()
    : received_nonempty_line_(false) {
}

bool FtpLsDirectoryListingParser::ConsumeLine(const string16& line) {
  // Allow empty lines only at the beginning of the listing. For example VMS
  // systems in Unix emulation mode add an empty line before the first listing
  // entry.
  if (line.empty() && !received_nonempty_line_)
    return true;
  received_nonempty_line_ = true;

  std::vector<string16> columns;
  SplitString(CollapseWhitespace(line, false), ' ', &columns);
  if (columns.size() == 11) {
    // Check if it is a symlink.
    if (!EqualsASCII(columns[9], "->"))
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

  if (!IsStringNonNegativeInteger(columns[1]))
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

bool FtpLsDirectoryListingParser::OnEndOfInput() {
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

FtpWindowsDirectoryListingParser::FtpWindowsDirectoryListingParser() {
}

bool FtpWindowsDirectoryListingParser::ConsumeLine(const string16& line) {
  std::vector<string16> columns;
  SplitString(CollapseWhitespace(line, false), ' ', &columns);
  if (columns.size() != 4)
    return false;

  FtpDirectoryListingEntry entry;
  entry.name = columns[3];

  if (EqualsASCII(columns[2], "<DIR>")) {
    entry.type = FtpDirectoryListingEntry::DIRECTORY;
    entry.size = -1;
  } else {
    entry.type = FtpDirectoryListingEntry::FILE;
    if (!StringToInt64(columns[2], &entry.size))
      return false;
    if (entry.size < 0)
      return false;
  }

  if (!WindowsDateListingToTime(columns, &entry.last_modified))
    return false;

  entries_.push(entry);
  return true;
}

bool FtpWindowsDirectoryListingParser::OnEndOfInput() {
  return true;
}

bool FtpWindowsDirectoryListingParser::EntryAvailable() const {
  return !entries_.empty();
}

FtpDirectoryListingEntry FtpWindowsDirectoryListingParser::PopEntry() {
  FtpDirectoryListingEntry entry = entries_.front();
  entries_.pop();
  return entry;
}

FtpVmsDirectoryListingParser::FtpVmsDirectoryListingParser()
    : state_(STATE_INITIAL),
      last_is_directory_(false) {
}

bool FtpVmsDirectoryListingParser::ConsumeLine(const string16& line) {
  switch (state_) {
    case STATE_INITIAL:
      DCHECK(last_filename_.empty());
      if (line.empty())
        return true;
      if (StartsWith(line, ASCIIToUTF16("Total of "), true)) {
        state_ = STATE_END;
        return true;
      }
      // We assume that the first non-empty line is the listing header. It often
      // starts with "Directory ", but not always.
      state_ = STATE_RECEIVED_HEADER;
      return true;
    case STATE_RECEIVED_HEADER:
      DCHECK(last_filename_.empty());
      if (line.empty())
        return true;
      state_ = STATE_ENTRIES;
      return ConsumeEntryLine(line);
    case STATE_ENTRIES:
      if (line.empty()) {
        if (!last_filename_.empty())
          return false;
        state_ = STATE_RECEIVED_LAST_ENTRY;
        return true;
      }
      return ConsumeEntryLine(line);
    case STATE_RECEIVED_LAST_ENTRY:
      DCHECK(last_filename_.empty());
      if (line.empty())
        return true;
      if (!StartsWith(line, ASCIIToUTF16("Total of "), true))
        return false;
      state_ = STATE_END;
      return true;
    case STATE_END:
      DCHECK(last_filename_.empty());
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool FtpVmsDirectoryListingParser::OnEndOfInput() {
  return (state_ == STATE_END);
}

bool FtpVmsDirectoryListingParser::EntryAvailable() const {
  return !entries_.empty();
}

FtpDirectoryListingEntry FtpVmsDirectoryListingParser::PopEntry() {
  FtpDirectoryListingEntry entry = entries_.front();
  entries_.pop();
  return entry;
}

bool FtpVmsDirectoryListingParser::ConsumeEntryLine(const string16& line) {
  std::vector<string16> columns;
  SplitString(CollapseWhitespace(line, false), ' ', &columns);

  if (columns.size() == 1) {
    if (!last_filename_.empty())
      return false;
    return ParseVmsFilename(columns[0], &last_filename_, &last_is_directory_);
  }

  // Recognize listing entries which generate "access denied" message even when
  // trying to list them. We don't display them in the final listing.
  static const char* kAccessDeniedMessages[] = {
    "%RMS-E-PRV",
    "privilege",
  };
  for (size_t i = 0; i < arraysize(kAccessDeniedMessages); i++) {
    if (line.find(ASCIIToUTF16(kAccessDeniedMessages[i])) != string16::npos) {
      last_filename_.clear();
      last_is_directory_ = false;
      return true;
    }
  }

  string16 filename;
  bool is_directory = false;
  if (last_filename_.empty()) {
    if (!ParseVmsFilename(columns[0], &filename, &is_directory))
      return false;
    columns.erase(columns.begin());
  } else {
    filename = last_filename_;
    is_directory = last_is_directory_;
    last_filename_.clear();
    last_is_directory_ = false;
  }

  if (columns.size() > 5)
    return false;

  if (columns.size() == 5) {
    if (!LooksLikeVmsFileProtectionListing(columns[4]))
      return false;
    if (!LooksLikeVmsUserIdentificationCode(columns[3]))
      return false;
    columns.resize(3);
  }

  if (columns.size() != 3)
    return false;

  FtpDirectoryListingEntry entry;
  entry.name = filename;
  entry.type = is_directory ? FtpDirectoryListingEntry::DIRECTORY
                            : FtpDirectoryListingEntry::FILE;
  if (!ParseVmsFilesize(columns[0], &entry.size))
    return false;
  if (entry.size < 0)
    return false;
  if (entry.type != FtpDirectoryListingEntry::FILE)
    entry.size = -1;
  if (!VmsDateListingToTime(columns, &entry.last_modified))
    return false;

  entries_.push(entry);
  return true;
}

}  // namespace net
