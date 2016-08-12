// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/iapps_xml_utils.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace iapps {

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

// Seek to the start of a tag and read the value into |result| if the node's
// name is |node_name|.
bool ReadSimpleValue(XmlReader* reader, const std::string& node_name,
                     std::string* result) {
  if (!iapps::SkipToNextElement(reader))
      return false;
  if (reader->NodeName() != node_name)
    return false;
  return reader->ReadElementContent(result);
}

bool ReadString(XmlReader* reader, std::string* result) {
  return ReadSimpleValue(reader, "string", result);
}

bool ReadInteger(XmlReader* reader, uint64_t* result) {
  std::string value;
  if (!ReadSimpleValue(reader, "integer", &value))
    return false;
  return base::StringToUint64(value, result);
}

std::string ReadFileAsString(base::File file) {
  std::string result;
  if (!file.IsValid())
    return result;

  // A "reasonable" artificial limit.
  // TODO(vandebo): Add a UMA to figure out what common values are.
  const int64_t kMaxLibraryFileSize = 150 * 1024 * 1024;
  base::File::Info file_info;
  if (!file.GetInfo(&file_info) || file_info.size > kMaxLibraryFileSize)
    return result;

  result.resize(file_info.size);
  int bytes_read = file.Read(0, base::string_as_array(&result), file_info.size);
  if (bytes_read != file_info.size)
    result.clear();

  return result;
}

XmlDictReader::XmlDictReader(XmlReader* reader) : reader_(reader) {}

XmlDictReader::~XmlDictReader() {}

bool XmlDictReader::Read() {
  if (reader_->NodeName() != "dict")
    return false;

  int dict_content_depth = reader_->Depth() + 1;
  // Advance past the dict node and into the body of the dictionary.
  if (!reader_->Read())
    return false;

  while (reader_->Depth() >= dict_content_depth && ShouldLoop()) {
    if (!iapps::SeekToNodeAtCurrentDepth(reader_, "key"))
      break;
    std::string found_key;
    if (!reader_->ReadElementContent(&found_key))
      break;
    DCHECK_EQ(dict_content_depth, reader_->Depth());

    if (!HandleKey(found_key))
      break;
  }
  // Seek to the end of the dictionary. Bail on end or error.
  while (reader_->Depth() >= dict_content_depth && reader_->Next()) {
  }
  return FinishedOk();
}

bool XmlDictReader::ShouldLoop() {
  return true;
}

bool XmlDictReader::HandleKey(const std::string& key) {
  if (Found(key))
    return AllowRepeats();
  if (HandleKeyImpl(key)) {
    found_.insert(key);
    return true;
  }
  return false;
}

bool XmlDictReader::AllowRepeats() {
  return false;
}

bool XmlDictReader::FinishedOk() {
  return true;
}

bool XmlDictReader::SkipToNext() {
  return SkipToNextElement(reader_) && reader_->Next();
}

bool XmlDictReader::Found(const std::string& key) const {
  return base::ContainsKey(found_, key);
}

}  // namespace iapps
