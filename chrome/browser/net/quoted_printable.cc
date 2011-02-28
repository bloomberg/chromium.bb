// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/quoted_printable.h"

#include "base/logging.h"
#include "base/string_util.h"

namespace {

const int kMaxCharPerLine = 76;
const char* const kEOL = "\r\n";

const char kHexTable[] = "0123456789ABCDEF";

}  // namespace

namespace chrome {
namespace browser {
namespace net {

void QuotedPrintableEncode(const std::string& input, std::string* output) {
  // The number of characters in the current line.
  int char_count = 0;
  for (std::string::const_iterator iter = input.begin();
       iter != input.end(); ++iter) {
    bool last_char = (iter + 1 == input.end());
    char c = *iter;
    // Whether this character can be inserted without encoding.
    bool as_is = false;
    // All printable ASCII characters can be included as is (but for =).
    if (c >= '!' && c <= '~' && c != '=') {
      as_is = true;
    }

    // Space and tab characters can be included as is if they don't appear at
    // the end of a line or at then end of the input.
    if (!as_is && (c == '\t' || c == ' ') && !last_char &&
        !IsEOL(iter + 1, input)) {
      as_is = true;
    }

    // End of line should be converted to CR-LF sequences.
    if (!last_char) {
      int eol_len = IsEOL(iter, input);
      if (eol_len > 0) {
        output->append(kEOL);
        char_count = 0;
        iter += (eol_len - 1);  // -1 because we'll ++ in the for() above.
        continue;
      }
    }

    // Insert a soft line break if necessary.
    int min_chars_needed = as_is ? kMaxCharPerLine - 2 : kMaxCharPerLine - 4;
    if (!last_char && char_count > min_chars_needed) {
      output->append("=");
      output->append(kEOL);
      char_count = 0;
    }

    // Finally, insert the actual character(s).
    if (as_is) {
      output->append(1, c);
      char_count++;
    } else {
      output->append("=");
      output->append(1, kHexTable[static_cast<int>((c >> 4) & 0xF)]);
      output->append(1, kHexTable[static_cast<int>(c & 0x0F)]);
      char_count += 3;
    }
  }
}

bool QuotedPrintableDecode(const std::string& input, std::string* output) {
  bool success = true;
  for (std::string::const_iterator iter = input.begin();
       iter!= input.end(); ++iter) {
    char c = *iter;
    if (c != '=') {
      output->append(1, c);
      continue;
    }
    if (input.end() - iter < 3) {
      LOG(ERROR) << "unfinished = sequence in input string.";
      success = false;
      output->append(1, c);
      continue;
    }
    char c2 = *(++iter);
    char c3 = *(++iter);
    if (c2 == '\r' && c3 == '\n') {
      // Soft line break, ignored.
      continue;
    }

    if (!IsHexDigit(c2) || !IsHexDigit(c3)) {
      LOG(ERROR) << "invalid = sequence, = followed by non hexa digit " <<
          "chars: " << c2 << " " << c3;
      success = false;
      // Just insert the chars as is.
      output->append("=");
      output->append(1, c2);
      output->append(1, c3);
      continue;
    }

    int i1 = HexDigitToInt(c2);
    int i2 = HexDigitToInt(c3);
    char r = static_cast<char>(((i1 << 4) & 0xF0) | (i2 & 0x0F));
    output->append(1, r);
  }
  return success;
}

int IsEOL(const std::string::const_iterator& iter, const std::string& input) {
  if (*iter == '\n')
    return 1;  // Single LF.

  if (*iter == '\r') {
    if ((iter + 1) == input.end() || *(iter + 1) != '\n')
      return 1;  // Single CR (Commodore and Old Macs).
    return 2;  // CR-LF.
  }

  return 0;
}

}  // namespace net
}  // namespace browser
}  // namespace chrome
