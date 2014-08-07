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

class PhotosXmlDictReader : public iapps::XmlDictReader {
 public:
  PhotosXmlDictReader(XmlReader* reader, PhotoInfo* photo_info)
    : iapps::XmlDictReader(reader), photo_info_(photo_info) {}

  virtual bool HandleKeyImpl(const std::string& key) OVERRIDE {
    if (key == "ImagePath") {
      std::string value;
      if (!iapps::ReadString(reader_, &value))
        return false;
      photo_info_->location = base::FilePath(value);
    } else if (key == "OriginalPath") {
      std::string value;
      if (!iapps::ReadString(reader_, &value))
        return false;
      photo_info_->original_location = base::FilePath(value);
    } else if (!SkipToNext()) {
      return false;
    }
    return true;
  }

  virtual bool FinishedOk() OVERRIDE {
    return Found("ImagePath");
  }

 private:
  PhotoInfo* photo_info_;
};

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

class AlbumXmlDictReader : public iapps::XmlDictReader {
 public:
  AlbumXmlDictReader(XmlReader* reader, AlbumInfo* album_info)
    : iapps::XmlDictReader(reader), album_info_(album_info) {}

  virtual bool ShouldLoop() OVERRIDE {
    return !(Found("AlbumId") && Found("AlbumName") && Found("KeyList"));
  }

  virtual bool HandleKeyImpl(const std::string& key) OVERRIDE {
    if (key == "AlbumId") {
      if (!iapps::ReadInteger(reader_, &album_info_->id))
        return false;
    } else if (key == "AlbumName") {
      if (!iapps::ReadString(reader_, &album_info_->name))
        return false;
    } else if (key == "KeyList") {
      if (!iapps::SeekToNodeAtCurrentDepth(reader_, "array"))
        return false;
      if (!ReadStringArray(reader_, &album_info_->photo_ids))
        return false;
    } else if (!SkipToNext()) {
      return false;
    }
    return true;
  }

  virtual bool FinishedOk() OVERRIDE {
    return !ShouldLoop();
  }

 private:
  AlbumInfo* album_info_;
};

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
    // Walk through a dictionary filling in |result| with photo information.
    // Return true if at least the location was found.
    // In either case, the cursor is advanced out of the dictionary.
    PhotosXmlDictReader dict_reader(reader, &photo_info);
    if (!dict_reader.Read()) {
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

class IPhotoLibraryXmlDictReader : public iapps::XmlDictReader {
 public:
  IPhotoLibraryXmlDictReader(XmlReader* reader, parser::Library* library)
    : iapps::XmlDictReader(reader), library_(library), ok_(true) {}

  virtual bool ShouldLoop() OVERRIDE {
    return !(Found("List of Albums") && Found("Master Image List"));
  }

  virtual bool HandleKeyImpl(const std::string& key) OVERRIDE {
    if (key == "List of Albums") {
      if (!iapps::SeekToNodeAtCurrentDepth(reader_, "array") ||
          !reader_->Read()) {
        return true;
      }
      while (iapps::SeekToNodeAtCurrentDepth(reader_, "dict")) {
        AlbumInfo album_info;
        AlbumXmlDictReader dict_reader(reader_, &album_info);
        if (dict_reader.Read()) {
          parser::Album album;
          album = album_info.photo_ids;
          // Strip / from album name and dedupe any collisions.
          std::string name;
          base::ReplaceChars(album_info.name, "//", " ", &name);
          if (ContainsKey(library_->albums, name))
            name = name + "("+base::Uint64ToString(album_info.id)+")";
          library_->albums[name] = album;
        }
      }
    } else if (key == "Master Image List") {
      if (!ParseAllPhotos(reader_, &library_->all_photos)) {
        ok_ = false;
        return false;
      }
    }
    return true;
  }

  virtual bool FinishedOk() OVERRIDE {
    return ok_;
  }

  // The IPhotoLibrary allows duplicate "List of Albums" and
  // "Master Image List" keys (although that seems odd.)
  virtual bool AllowRepeats() OVERRIDE {
    return true;
  }

 private:
  parser::Library* library_;

  // The base class bails when we request, and then calls |FinishedOk()|
  // to decide what to return. We need to remember that we bailed because
  // of an error. That's what |ok_| does.
  bool ok_;
};

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

  IPhotoLibraryXmlDictReader dict_reader(&reader, &library_);
  return dict_reader.Read();
}

}  // namespace iphoto
