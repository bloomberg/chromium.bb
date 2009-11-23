// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSERS_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSERS_H_

#include <queue>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/time.h"
#include "net/ftp/ftp_server_type_histograms.h"

namespace net {

struct FtpDirectoryListingEntry {
  enum Type {
    FILE,
    DIRECTORY,
    SYMLINK,
  };

  Type type;
  string16 name;
  int64 size;  // File size, in bytes. -1 if not applicable.

  // Last modified time, in local time zone.
  base::Time last_modified;
};

class FtpDirectoryListingParser {
 public:
  virtual ~FtpDirectoryListingParser();

  virtual FtpServerType GetServerType() const = 0;

  // Adds |line| to the internal parsing buffer. Returns true on success.
  virtual bool ConsumeLine(const string16& line) = 0;

  // Called after all input has been consumed. Returns true if the parser
  // recognizes all received data as a valid listing.
  virtual bool OnEndOfInput() = 0;

  // Returns true if there is at least one FtpDirectoryListingEntry available.
  virtual bool EntryAvailable() const = 0;

  // Returns the next entry. It is an error to call this function unless
  // EntryAvailable returns true.
  virtual FtpDirectoryListingEntry PopEntry() = 0;
};

// Parser for "ls -l"-style directory listing.
class FtpLsDirectoryListingParser : public FtpDirectoryListingParser {
 public:
  FtpLsDirectoryListingParser();

  // FtpDirectoryListingParser methods:
  virtual FtpServerType GetServerType() const { return SERVER_LS; }
  virtual bool ConsumeLine(const string16& line);
  virtual bool OnEndOfInput();
  virtual bool EntryAvailable() const;
  virtual FtpDirectoryListingEntry PopEntry();

 private:
  bool received_nonempty_line_;

  std::queue<FtpDirectoryListingEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(FtpLsDirectoryListingParser);
};

class FtpWindowsDirectoryListingParser : public FtpDirectoryListingParser {
 public:
  FtpWindowsDirectoryListingParser();

  // FtpDirectoryListingParser methods:
  virtual FtpServerType GetServerType() const { return SERVER_WINDOWS; }
  virtual bool ConsumeLine(const string16& line);
  virtual bool OnEndOfInput();
  virtual bool EntryAvailable() const;
  virtual FtpDirectoryListingEntry PopEntry();

 private:
  std::queue<FtpDirectoryListingEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(FtpWindowsDirectoryListingParser);
};

// Parser for VMS-style directory listing (including variants).
class FtpVmsDirectoryListingParser : public FtpDirectoryListingParser {
 public:
  FtpVmsDirectoryListingParser();

  // FtpDirectoryListingParser methods:
  virtual FtpServerType GetServerType() const { return SERVER_VMS; }
  virtual bool ConsumeLine(const string16& line);
  virtual bool OnEndOfInput();
  virtual bool EntryAvailable() const;
  virtual FtpDirectoryListingEntry PopEntry();

 private:
  // Consumes listing line which is expected to be a directory listing entry
  // (and not a comment etc). Returns true on success.
  bool ConsumeEntryLine(const string16& line);

  enum State {
    STATE_INITIAL,

    // Indicates that we have received the header, like this:
    // Directory SYS$SYSDEVICE:[ANONYMOUS]
    STATE_RECEIVED_HEADER,

    // Indicates that we have received the first listing entry, like this:
    // MADGOAT.DIR;1              2   9-MAY-2001 22:23:44.85
    STATE_ENTRIES,

    // Indicates that we have received the last listing entry.
    STATE_RECEIVED_LAST_ENTRY,

    // Indicates that we have successfully received all parts of the listing.
    STATE_END,
  } state_;

  // VMS can use two physical lines if the filename is long. The first line will
  // contain the filename, and the second line everything else. Store the
  // filename until we receive the next line.
  string16 last_filename_;
  bool last_is_directory_;

  std::queue<FtpDirectoryListingEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(FtpVmsDirectoryListingParser);
};

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSERS_H_
