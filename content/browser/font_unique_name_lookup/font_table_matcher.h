// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_TABLE_MATCHER_H_
#define CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_TABLE_MATCHER_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "content/browser/font_unique_name_lookup/font_unique_name_table.pb.h"
#include "content/common/content_export.h"

#include <stddef.h>
#include <stdint.h>

namespace content {

// Parses a protobuf received in memory_mapping to build a font lookup
// structure. Allows case-insensitively matching full font names or postscript
// font names aginst the parsed table by calling MatchName. Used in Blink for
// looking up
// @font-face { src: local(<font_name>) } CSS font face src references.
class CONTENT_EXPORT FontTableMatcher {
 public:
  // Constructs a FontTableMatcher from a ReadOnlySharedMemoryMapping returned
  // by FontUniqueNameLookup.  Internally parses the Protobuf structure in
  // memory_mapping to build a list of unique font names, which can then be
  // matched using the MatchName method. The ReadOnlySharedMemoryMapping passed
  // in memory_mapping only needs to be alive for the initial construction of
  // FontTableMatcher. After that, FontTableMatcher no longer accesses it.
  explicit FontTableMatcher(
      const base::ReadOnlySharedMemoryMapping& memory_mapping);

  // Takes a FontUniqueNameTable protobuf and serializes it into a newly created
  // ReadonlySharedMemoryMapping. Used only for testing.
  static base::ReadOnlySharedMemoryMapping MemoryMappingFromFontUniqueNameTable(
      const FontUniqueNameTable& font_unique_name_table);

  struct MatchResult {
    std::string font_path;
    uint32_t ttc_index;
  };

  // Given a font full name or font potscript name, match case insensitively
  // against the internal list of unique font names.
  // Return a font filesystem path and a TrueType collection index to identify a
  // font binary to uniquely identify instantiate a font.
  base::Optional<MatchResult> MatchName(const std::string& name_request) const;

  // Returns the number of fonts available after parsing the
  // ReadOnlySharedMemoryMapping.
  size_t AvailableFonts() const;

  // Compares this FontTableMatcher to other for whether
  // their internal list of fonts is disjoint. Used only for testing.
  bool FontListIsDisjointFrom(const FontTableMatcher& other) const;

 private:
  FontUniqueNameTable font_table_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_TABLE_MATCHER_H_
