// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/itunes_library_parser.h"

#include <string>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/media_galleries/itunes_xml_utils.h"
#include "third_party/libxml/chromium/libxml_utils.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace itunes {

namespace {

struct TrackInfo {
  uint64 id;
  base::FilePath location;
  std::string artist;
  std::string album;
};

// Seek to the start of a tag and read the value into |result| if the node's
// name is |node_name|.
bool ReadSimpleValue(XmlReader* reader, const std::string& node_name,
                     std::string* result) {
  if (!SkipToNextElement(reader))
      return false;
  if (reader->NodeName() != node_name)
    return false;
  return reader->ReadElementContent(result);
}

// Get the value out of a string node.
bool ReadString(XmlReader* reader, std::string* result) {
  return ReadSimpleValue(reader, "string", result);
}

// Get the value out of an integer node.
bool ReadInteger(XmlReader* reader, uint64* result) {
  std::string value;
  if (!ReadSimpleValue(reader, "integer", &value))
    return false;
  return base::StringToUint64(value, result);
}

// Walk through a dictionary filling in |result| with track information. Return
// true if at least the id and location where found (artist and album may be
// empty).  In either case, the cursor is advanced out of the dictionary.
bool GetTrackInfoFromDict(XmlReader* reader, TrackInfo* result) {
  DCHECK(result);
  if (reader->NodeName() != "dict")
    return false;

  int dict_content_depth = reader->Depth() + 1;
  // Advance past the dict node and into the body of the dictionary.
  if (!reader->Read())
    return false;

  bool found_id = false;
  bool found_location = false;
  bool found_artist = false;
  bool found_album_artist = false;
  bool found_album = false;
  while (reader->Depth() >= dict_content_depth &&
         !(found_id && found_location && found_album_artist && found_album)) {
    if (!SeekToNodeAtCurrentDepth(reader, "key"))
      break;
    std::string found_key;
    if (!reader->ReadElementContent(&found_key))
      break;
    DCHECK_EQ(dict_content_depth, reader->Depth());

    if (found_key == "Track ID") {
      if (found_id)
        break;
      if (!ReadInteger(reader, &result->id))
        break;
      found_id = true;
    } else if (found_key == "Location") {
      if (found_location)
        break;
      std::string value;
      if (!ReadString(reader, &value))
        break;
      GURL url(value);
      if (!url.SchemeIsFile())
        break;
      url_canon::RawCanonOutputW<1024> decoded_location;
      url_util::DecodeURLEscapeSequences(url.path().c_str() + 1,  // Strip /.
                                         url.path().length() - 1,
                                         &decoded_location);
#if defined(OS_WIN)
      string16 location(decoded_location.data(), decoded_location.length());
#else
      string16 location16(decoded_location.data(), decoded_location.length());
      std::string location = "/" + UTF16ToUTF8(location16);
#endif
      result->location = base::FilePath(location);
      found_location = true;
    } else if (found_key == "Artist") {
      if (found_artist || found_album_artist)
        break;
      if (!ReadString(reader, &result->artist))
        break;
      found_artist = true;
    } else if (found_key == "Album Artist") {
      if (found_album_artist)
        break;
      result->artist.clear();
      if (!ReadString(reader, &result->artist))
        break;
      found_album_artist = true;
    } else if (found_key == "Album") {
      if (found_album)
        break;
      if (!ReadString(reader, &result->album))
        break;
      found_album = true;
    } else {
      if (!SkipToNextElement(reader))
        break;
      if (!reader->Next())
        break;
    }
  }

  // Seek to the end of the dictionary
  while (reader->Depth() >= dict_content_depth)
    reader->Next();

  return found_id && found_location;
}

}  // namespace

ITunesLibraryParser::ITunesLibraryParser() {}
ITunesLibraryParser::~ITunesLibraryParser() {}

// static
std::string ITunesLibraryParser::ReadITunesLibraryXmlFile(
    const base::PlatformFile file) {
  std::string result;
  if (file == base::kInvalidPlatformFileValue)
    return result;

  // A "reasonable" artificial limit.
  // TODO(vandebo): Add a UMA to figure out what common values are.
  const int64 kMaxLibraryFileSize = 150 * 1024 * 1024;
  base::PlatformFileInfo file_info;
  if (!base::GetPlatformFileInfo(file, &file_info) ||
      file_info.size > kMaxLibraryFileSize) {
    base::ClosePlatformFile(file);
    return result;
  }

  result.resize(file_info.size);
  int bytes_read =
      base::ReadPlatformFile(file, 0, string_as_array(&result), file_info.size);
  if (bytes_read != file_info.size)
    result.clear();

  base::ClosePlatformFile(file);
  return result;
}

bool ITunesLibraryParser::Parse(const std::string& library_xml) {
  XmlReader reader;

  if (!reader.Load(library_xml))
    return false;

  // Find the plist node and then search within that tag.
  if (!SeekToNodeAtCurrentDepth(&reader, "plist"))
    return false;
  if (!reader.Read())
    return false;

  if (!SeekToNodeAtCurrentDepth(&reader, "dict"))
    return false;

  if (!SeekInDict(&reader, "Tracks"))
    return false;

  // Once inside the Tracks dict, we expect track dictionaries keyed by id. i.e.
  //   <key>Tracks</key>
  //   <dict>
  //     <key>160</key>
  //     <dict>
  //       <key>Track ID</key><integer>160</integer>
  if (!SeekToNodeAtCurrentDepth(&reader, "dict"))
    return false;
  int tracks_dict_depth = reader.Depth() + 1;
  if (!reader.Read())
    return false;

  // Once parsing has gotten this far, return whatever is found, even if
  // some of the data isn't extracted just right.
  bool no_errors = true;
  bool track_found = false;
  while (reader.Depth() >= tracks_dict_depth) {
    if (!SeekToNodeAtCurrentDepth(&reader, "key"))
      return track_found;
    std::string key;  // Should match track id below.
    if (!reader.ReadElementContent(&key))
      return track_found;
    uint64 id;
    bool id_valid = base::StringToUint64(key, &id);
    if (!reader.SkipToElement())
      return track_found;

    TrackInfo track_info;
    if (GetTrackInfoFromDict(&reader, &track_info) &&
        id_valid &&
        id == track_info.id) {
      if (track_info.artist.empty())
        track_info.artist = "Unknown Artist";
      if (track_info.album.empty())
        track_info.album = "Unknown Album";
      parser::Track track(track_info.id, track_info.location);
      library_[track_info.artist][track_info.album].insert(track);
      track_found = true;
    } else {
      no_errors = false;
    }
  }

  return track_found || no_errors;
}

}  // namespace itunes
