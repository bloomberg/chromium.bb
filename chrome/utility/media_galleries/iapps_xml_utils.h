// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_GALLERIES_IAPPS_XML_UTILS_H_
#define CHROME_UTILITY_MEDIA_GALLERIES_IAPPS_XML_UTILS_H_

#include <string>

#include "base/files/file.h"

class XmlReader;

namespace iapps {

// Like XmlReader::SkipToElement, but will advance to the next open tag if the
// cursor is on a close tag.
bool SkipToNextElement(XmlReader* reader);

// Traverse |reader| looking for a node named |name| at the current depth
// of |reader|.
bool SeekToNodeAtCurrentDepth(XmlReader* reader, const std::string& name);

// Search within a "dict" node for |key|. The cursor must be on the starting
// "dict" node when entering this function.
bool SeekInDict(XmlReader* reader, const std::string& key);

// Get the value out of a string node.
bool ReadString(XmlReader* reader, std::string* result);

// Get the value out of an integer node.
bool ReadInteger(XmlReader* reader, uint64* result);

// Read in the contents of the given library xml |file| and return as a string.
std::string ReadFileAsString(base::File file);

}  // namespace iapps

#endif  // CHROME_UTILITY_MEDIA_GALLERIES_IAPPS_XML_UTILS_H_
