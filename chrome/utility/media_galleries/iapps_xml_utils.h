// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_GALLERIES_IAPPS_XML_UTILS_H_
#define CHROME_UTILITY_MEDIA_GALLERIES_IAPPS_XML_UTILS_H_

#include <set>
#include <string>

#include "base/files/file.h"
#include "base/stl_util.h"

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

// Contains the common code and main loop for reading the key/values
// of an XML dict. The derived class must implement |HandleKeyImpl()|
// which is called with each key, and may re-implement |ShouldLoop()|,
// |FinishedOk()| and/or |AllowRepeats()|.
class XmlDictReader {
 public:
  explicit XmlDictReader(XmlReader* reader);
  virtual ~XmlDictReader();

  // The main loop of this class. Reads all the keys in the
  // current element and calls |HandleKey()| with each.
  bool Read();

  // Re-implemented by derived class if it should bail from the
  // loop earlier, such as if it encountered all required fields.
  virtual bool ShouldLoop();

  // Called by |Read()| with each key. Calls derived |HandleKeyImpl()|.
  bool HandleKey(const std::string& key);

  virtual bool HandleKeyImpl(const std::string& key) = 0;

  // Re-implemented by the derived class (to return true) if
  // it should allow fields to be repeated, but skipped.
  virtual bool AllowRepeats();

  // Re-implemented by derived class if it should test for required
  // fields instead of just returning true.
  virtual bool FinishedOk();

  // A convenience function for the derived classes.
  // Skips to next element.
  bool SkipToNext();

  // A convenience function for the derived classes.
  // Used to test if all required keys have been encountered.
  bool Found(const std::string& key) const;

 protected:
  XmlReader* reader_;

 private:
  // The keys that the reader has run into in this element.
  std::set<std::string> found_;

  DISALLOW_COPY_AND_ASSIGN(XmlDictReader);
};

}  // namespace iapps

#endif  // CHROME_UTILITY_MEDIA_GALLERIES_IAPPS_XML_UTILS_H_
