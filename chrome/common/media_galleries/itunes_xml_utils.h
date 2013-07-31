// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_GALLERIES_ITUNES_XML_UTILS_H_
#define CHROME_COMMON_MEDIA_GALLERIES_ITUNES_XML_UTILS_H_

#include <string>

class XmlReader;

namespace itunes {

// Like XmlReader::SkipToElement, but will advance to the next open tag if the
// cursor is on a close tag.
bool SkipToNextElement(XmlReader* reader);

// Traverse |reader| looking for a node named |name| at the current depth
// of |reader|.
bool SeekToNodeAtCurrentDepth(XmlReader* reader, const std::string& name);

// Search within a "dict" node for |key|. The cursor must be on the starting
// "dict" node when entering this function.
bool SeekInDict(XmlReader* reader, const std::string& key);

}  // namespace itunes

#endif  // CHROME_COMMON_MEDIA_GALLERIES_ITUNES_XML_UTILS_H_
