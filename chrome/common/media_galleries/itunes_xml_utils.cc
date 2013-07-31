// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_galleries/itunes_xml_utils.h"

#include <string>

#include "base/logging.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace itunes {

bool SkipToNextElement(XmlReader* reader) {
  if (!reader->SkipToElement()) {
    // SkipToElement returns false if the current node is an end element,
    // try to advance to the next element and then try again.
    if (!reader->Read() || !reader->SkipToElement())
      return false;
  }
  return true;
}

bool SeekToNodeAtCurrentDepth(XmlReader* reader, const std::string& name) {
  int depth = reader->Depth();
  do {
    if (!SkipToNextElement(reader) || reader->Depth() < depth)
      return false;
    DCHECK_EQ(depth, reader->Depth());
    if (reader->NodeName() == name)
      return true;
  } while (reader->Next());

  return false;
}

bool SeekInDict(XmlReader* reader, const std::string& key) {
  DCHECK_EQ("dict", reader->NodeName());

  int dict_content_depth = reader->Depth() + 1;
  // Advance past the dict node and into the body of the dictionary.
  if (!reader->Read())
    return false;

  while (reader->Depth() >= dict_content_depth) {
    if (!SeekToNodeAtCurrentDepth(reader, "key"))
      return false;
    std::string found_key;
    if (!reader->ReadElementContent(&found_key))
      return false;
    DCHECK_EQ(dict_content_depth, reader->Depth());
    if (found_key == key)
      return true;
  }
  return false;
}

}  // namespace itunes
