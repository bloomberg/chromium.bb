// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/iphoto_library_parser.h"

#include <string>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/utility/media_galleries/iapps_xml_utils.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace iphoto {

namespace {

struct PhotoInfo {
  uint64 id;
  base::FilePath location;
  base::FilePath original_location;
};

struct AlbumInfo {
  std::set<uint64> photo_ids;
  std::string name;
  uint64 id;
};

// Walk through a dictionary filling in |result| with photo information. Return
// true if at least the id and location were found.
// In either case, the cursor is advanced out of the dictionary.
bool GetPhotoInfoFromDict(XmlReader* reader, PhotoInfo* result) {
  DCHECK(result);
  if (reader->NodeName() != "dict")
    return false;

  int dict_content_depth = reader->Depth() + 1;
  // Advance past the dict node and into the body of the dictionary.
  if (!reader->Read())
    return false;

  bool found_location = false;
  while (reader->Depth() >= dict_content_depth) {
    if (!iapps::SeekToNodeAtCurrentDepth(reader, "key"))
      break;
    std::string found_key;
    if (!reader->ReadElementContent(&found_key))
      break;
    DCHECK_EQ(dict_content_depth, reader->Depth());

    if (found_key == "ImagePath") {
      if (found_location)
        break;
      std::string value;
      if (!iapps::ReadString(reader, &value))
        break;
      result->location = base::FilePath(value);
      found_location = true;
    } else if (found_key == "OriginalPath") {
      std::string value;
      if (!iapps::ReadString(reader, &value))
        break;
      result->original_location = base::FilePath(value);
    } else {
      if (!iapps::SkipToNextElement(reader))
        break;
      if (!reader->Next())
        break;
    }
  }

  // Seek to the end of the dictionary
  while (reader->Depth() >= dict_content_depth)
    reader->Next();

  return found_location;
}

// Contents of the album 'KeyList' key are
// <array>
//  <string>1</string>
//  <string>2</string>
//  <string>3</string>
// </array>
bool ReadStringArray(XmlReader* reader, std::set<uint64>* photo_ids) {
  if (reader->NodeName() != "array")
    return false;

  // Advance past the array node and into the body of the array.
  if (!reader->Read())
    return false;

  int array_content_depth = reader->Depth();

  while (iapps::SeekToNodeAtCurrentDepth(reader, "string")) {
    if (reader->Depth() != array_content_depth)
      return false;
    std::string photo_id;
    if (!iapps::ReadString(reader, &photo_id))
      continue;
    uint64 id;
    if (!base::StringToUint64(photo_id, &id))
      continue;
    photo_ids->insert(id);
  }

  return true;
}

bool GetAlbumInfoFromDict(XmlReader* reader, AlbumInfo* result) {
  DCHECK(result);
  if (reader->NodeName() != "dict")
    return false;

  int dict_content_depth = reader->Depth() + 1;
  // Advance past the dict node and into the body of the dictionary.
  if (!reader->Read())
    return false;

  bool found_id = false;
  bool found_name = false;
  bool found_contents = false;
  while (reader->Depth() >= dict_content_depth &&
         !(found_id && found_name && found_contents)) {
    if (!iapps::SeekToNodeAtCurrentDepth(reader, "key"))
      break;
    std::string found_key;
    if (!reader->ReadElementContent(&found_key))
      break;
    DCHECK_EQ(dict_content_depth, reader->Depth());

    if (found_key == "AlbumId") {
      if (found_id)
        break;
      if (!iapps::ReadInteger(reader, &result->id))
        break;
      found_id = true;
    } else if (found_key == "AlbumName") {
      if (found_name)
        break;
      if (!iapps::ReadString(reader, &result->name))
        break;
      found_name = true;
    } else if (found_key == "KeyList") {
      if (found_contents)
        break;
      if (!iapps::SeekToNodeAtCurrentDepth(reader, "array"))
        break;
      if (!ReadStringArray(reader, &result->photo_ids))
        break;
      found_contents = true;
    } else {
      if (!iapps::SkipToNextElement(reader))
        break;
      if (!reader->Next())
        break;
    }
  }

  // Seek to the end of the dictionary
  while (reader->Depth() >= dict_content_depth)
    reader->Next();

  return found_id && found_name && found_contents;
}

// Inside the master image list, we expect photos to be arranged as
//  <dict>
//   <key>$PHOTO_ID</key>
//   <dict>
//     $photo properties
//   </dict>
//   <key>$PHOTO_ID</key>
//   <dict>
//     $photo properties
//   </dict>
//   ...
//  </dict>
// Returns true on success, false on error.
bool ParseAllPhotos(XmlReader* reader,
                    std::set<iphoto::parser::Photo>* all_photos) {
  if (!iapps::SeekToNodeAtCurrentDepth(reader, "dict"))
    return false;
  int photos_dict_depth = reader->Depth() + 1;
  if (!reader->Read())
    return false;

  bool errors = false;
  while (reader->Depth() >= photos_dict_depth) {
    if (!iapps::SeekToNodeAtCurrentDepth(reader, "key"))
      break;

    std::string key;
    if (!reader->ReadElementContent(&key)) {
      errors = true;
      break;
    }
    uint64 id;
    bool id_valid = base::StringToUint64(key, &id);

    if (!id_valid ||
        reader->Depth() != photos_dict_depth) {
      errors = true;
      break;
    }
    if (!iapps::SeekToNodeAtCurrentDepth(reader, "dict")) {
      errors = true;
      break;
    }

    PhotoInfo photo_info;
    photo_info.id = id;
    if (!GetPhotoInfoFromDict(reader, &photo_info)) {
      errors = true;
      break;
    }

    parser::Photo photo(photo_info.id, photo_info.location,
                        photo_info.original_location);
    all_photos->insert(photo);
  }

  return !errors;
}

}  // namespace

IPhotoLibraryParser::IPhotoLibraryParser() {}
IPhotoLibraryParser::~IPhotoLibraryParser() {}

bool IPhotoLibraryParser::Parse(const std::string& library_xml) {
  XmlReader reader;
  if (!reader.Load(library_xml))
    return false;

  // Find the plist node and then search within that tag.
  if (!iapps::SeekToNodeAtCurrentDepth(&reader, "plist"))
    return false;
  if (!reader.Read())
    return false;

  if (!iapps::SeekToNodeAtCurrentDepth(&reader, "dict"))
    return false;

  int dict_content_depth = reader.Depth() + 1;
  // Advance past the dict node and into the body of the dictionary.
  if (!reader.Read())
    return false;

  bool found_photos = false;
  bool found_albums = false;
  while (reader.Depth() >= dict_content_depth &&
         !(found_photos && found_albums)) {
    if (!iapps::SeekToNodeAtCurrentDepth(&reader, "key"))
      break;
    std::string found_key;
    if (!reader.ReadElementContent(&found_key))
      break;
    DCHECK_EQ(dict_content_depth, reader.Depth());

    if (found_key == "List of Albums") {
      if (found_albums)
        continue;
      found_albums = true;

      if (!iapps::SeekToNodeAtCurrentDepth(&reader, "array") ||
          !reader.Read()) {
        continue;
      }

      while (iapps::SeekToNodeAtCurrentDepth(&reader, "dict")) {
        AlbumInfo album_info;
        if (GetAlbumInfoFromDict(&reader, &album_info)) {
          parser::Album album;
          album = album_info.photo_ids;
          // Strip / from album name and dedupe any collisions.
          std::string name;
          base::ReplaceChars(album_info.name, "//", " ", &name);
          if (!ContainsKey(library_.albums, name)) {
            library_.albums[name] = album;
          } else {
            library_.albums[name+"("+base::Uint64ToString(album_info.id)+")"] =
                album;
          }
        }
      }
    } else if (found_key == "Master Image List") {
      if (found_photos)
        continue;
      found_photos = true;
      if (!ParseAllPhotos(&reader, &library_.all_photos))
        return false;
    }
  }

  return true;
}

}  // namespace iphoto
