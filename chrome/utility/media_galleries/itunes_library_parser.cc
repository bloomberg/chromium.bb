// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/itunes_library_parser.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/utility/media_galleries/iapps_xml_utils.h"
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

class TrackInfoXmlDictReader : public iapps::XmlDictReader {
 public:
  TrackInfoXmlDictReader(XmlReader* reader, TrackInfo* track_info) :
    iapps::XmlDictReader(reader), track_info_(track_info) {}

  virtual bool ShouldLoop() OVERRIDE {
    return !(Found("Track ID") && Found("Location") &&
             Found("Album Artist") && Found("Album"));
  }

  virtual bool HandleKeyImpl(const std::string& key) OVERRIDE {
    if (key == "Track ID") {
      return iapps::ReadInteger(reader_, &track_info_->id);
    } else if (key == "Location") {
      std::string value;
      if (!iapps::ReadString(reader_, &value))
        return false;
      GURL url(value);
      if (!url.SchemeIsFile())
        return false;
      url::RawCanonOutputW<1024> decoded_location;
      url::DecodeURLEscapeSequences(url.path().c_str() + 1,  // Strip /.
                                    url.path().length() - 1,
                                    &decoded_location);
#if defined(OS_WIN)
      base::string16 location(decoded_location.data(),
                              decoded_location.length());
#else
      base::string16 location16(decoded_location.data(),
                                decoded_location.length());
      std::string location = "/" + base::UTF16ToUTF8(location16);
#endif
      track_info_->location = base::FilePath(location);
    } else if (key == "Artist") {
      if (Found("Album Artist"))
        return false;
      return iapps::ReadString(reader_, &track_info_->artist);
    } else if (key == "Album Artist") {
      track_info_->artist.clear();
      return iapps::ReadString(reader_, &track_info_->artist);
    } else if (key == "Album") {
      return iapps::ReadString(reader_, &track_info_->album);
    } else if (!SkipToNext()) {
      return false;
    }
    return true;
  }

  virtual bool FinishedOk() OVERRIDE {
    return Found("Track ID") && Found("Location");
  }

 private:
  TrackInfo* track_info_;
};

// Walk through a dictionary filling in |result| with track information. Return
// true if at least the id and location where found (artist and album may be
// empty).  In either case, the cursor is advanced out of the dictionary.
bool GetTrackInfoFromDict(XmlReader* reader, TrackInfo* result) {
  DCHECK(result);
  TrackInfoXmlDictReader dict_reader(reader, result);
  return dict_reader.Read();
}

}  // namespace

ITunesLibraryParser::ITunesLibraryParser() {}
ITunesLibraryParser::~ITunesLibraryParser() {}

bool ITunesLibraryParser::Parse(const std::string& library_xml) {
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

  if (!iapps::SeekInDict(&reader, "Tracks"))
    return false;

  // Once inside the Tracks dict, we expect track dictionaries keyed by id. i.e.
  //   <key>Tracks</key>
  //   <dict>
  //     <key>160</key>
  //     <dict>
  //       <key>Track ID</key><integer>160</integer>
  if (!iapps::SeekToNodeAtCurrentDepth(&reader, "dict"))
    return false;
  int tracks_dict_depth = reader.Depth() + 1;
  if (!reader.Read())
    return false;

  // Once parsing has gotten this far, return whatever is found, even if
  // some of the data isn't extracted just right.
  bool no_errors = true;
  bool track_found = false;
  while (reader.Depth() >= tracks_dict_depth) {
    if (!iapps::SeekToNodeAtCurrentDepth(&reader, "key"))
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
