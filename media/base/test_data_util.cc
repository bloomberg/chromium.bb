// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_data_util.h"

#include <stdint.h>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "media/base/decoder_buffer.h"

namespace media {

namespace {

// Mime types for test files. Sorted in alphabetical order.
const char kAacAdtsAudioOnly[] = "audio/aac";
const char kMp2tAudioVideo[] = "video/mp2t; codecs=\"mp4a.40.2, avc1.42E01E\"";
const char kMp4AacAudioOnly[] = "audio/mp4; codecs=\"mp4a.40.2\"";
const char kMp4AudioOnly[] = "audio/mp4; codecs=\"mp4a.40.2\"'";
const char kMp4Av110bitVideoOnly[] = "video/mp4; codecs=\"av01.0.04M.10\"";
const char kMp4Av1VideoOnly[] = "video/mp4; codecs=\"av01.0.04M.08\"";
const char kMp4Avc1VideoOnly[] = "video/mp4; codecs=\"avc1.64001E\"";
const char kMp4FlacAudioOnly[] = "audio/mp4; codecs=\"flac\"";
const char kMp4VideoOnly[] = "video/mp4; codecs=\"avc1.4D4041\"'";
const char kMp4Vp9Profile2VideoOnly[] =
    "video/mp4; codecs=\"vp09.02.10.10.01.02.02.02.00\"";
const char kMp4Vp9VideoOnly[] =
    "video/mp4; codecs=\"vp09.00.10.08.01.02.02.02.00\"";
const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";
const char kWebMAv110bitVideoOnly[] = "video/webm; codecs=\"av01.0.04M.10\"";
const char kWebMAv1VideoOnly[] = "video/webm; codecs=\"av01.0.04M.08\"";
const char kWebMOpusAudioOnly[] = "audio/webm; codecs=\"opus\"";
const char kWebMOpusAudioVp9Video[] = "video/webm; codecs=\"opus, vp9\"";
const char kWebMVorbisAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
const char kWebMVorbisAudioVp8Video[] = "video/webm; codecs=\"vorbis, vp8\"";
const char kWebMVp8VideoOnly[] = "video/webm; codecs=\"vp8\"";
const char kWebMVp9Profile2VideoOnly[] =
    "video/webm; codecs=\"vp09.02.10.10.01.02.02.02.00\"";
const char kWebMVp9VideoOnly[] = "video/webm; codecs=\"vp9\"";

// A map from a test file name to its mime type. The file is located at
// media/test/data.
using FileToMimeTypeMap = base::flat_map<std::string, std::string>;

// Wrapped to avoid static initializer startup cost. The list is sorted in
// alphabetical order.
const FileToMimeTypeMap& GetFileToMimeTypeMap() {
  static const base::NoDestructor<FileToMimeTypeMap> kFileToMimeTypeMap({
      {"bear-1280x720.ts", kMp2tAudioVideo},
      {"bear-320x240-audio-only.webm", kWebMVorbisAudioOnly},
      {"bear-320x240-av_enc-a.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-av_enc-av.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-av_enc-v.webm", kWebMVorbisAudioVp8Video},
      {"bear-320x240-opus-a_enc-a.webm", kWebMOpusAudioOnly},
      {"bear-320x240-opus-av_enc-av.webm", kWebMOpusAudioVp9Video},
      {"bear-320x240-opus-av_enc-v.webm", kWebMOpusAudioVp9Video},
      {"bear-320x240-v-vp9_fullsample_enc-v.webm", kWebMVp9VideoOnly},
      {"bear-320x240-v-vp9_profile2_subsample_cenc-v.mp4",
       kMp4Vp9Profile2VideoOnly},
      {"bear-320x240-v-vp9_profile2_subsample_cenc-v.webm",
       kWebMVp9Profile2VideoOnly},
      {"bear-320x240-v-vp9_subsample_enc-v.webm", kWebMVp9VideoOnly},
      {"bear-320x240-v_enc-v.webm", kWebMVp8VideoOnly},
      {"bear-320x240-v_frag-vp9-cenc.mp4", kMp4Vp9VideoOnly},
      {"bear-320x240-video-only.webm", kWebMVp8VideoOnly},
      {"bear-320x240.webm", kWebMAudioVideo},
      {"bear-640x360-a_frag-cbcs.mp4", kMp4AacAudioOnly},
      {"bear-640x360-a_frag-cenc.mp4", kMp4AacAudioOnly},
      {"bear-640x360-a_frag.mp4", kMp4AudioOnly},
      {"bear-640x360-v_frag-cbc1.mp4", kMp4Avc1VideoOnly},
      {"bear-640x360-v_frag-cbcs.mp4", kMp4Avc1VideoOnly},
      {"bear-640x360-v_frag-cenc-mdat.mp4", kMp4Avc1VideoOnly},
      {"bear-640x360-v_frag-cenc.mp4", kMp4Avc1VideoOnly},
      {"bear-640x360-v_frag-cens.mp4", kMp4Avc1VideoOnly},
      {"bear-640x360-v_frag.mp4", kMp4VideoOnly},
      {"bear-a_enc-a.webm", kWebMVorbisAudioOnly},
      {"bear-av1-320x180-10bit-cenc.mp4", kMp4Av110bitVideoOnly},
      {"bear-av1-320x180-10bit-cenc.webm", kWebMAv110bitVideoOnly},
      {"bear-av1-cenc.mp4", kMp4Av1VideoOnly},
      {"bear-av1-cenc.webm", kWebMAv1VideoOnly},
      {"bear-flac-cenc.mp4", kMp4FlacAudioOnly},
      {"bear-flac_frag.mp4", kMp4FlacAudioOnly},
      {"bear-opus.webm", kWebMOpusAudioOnly},
      {"frame_size_change-av_enc-v.webm", kWebMVorbisAudioVp8Video},
      {"sfx.adts", kAacAdtsAudioOnly},
  });

  return *kFileToMimeTypeMap;
}

// Key used to encrypt test files.
const uint8_t kSecretKey[] = {0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
                              0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c};

// The key ID for all encrypted files.
const uint8_t kKeyId[] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                          0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};

}  // namespace

// TODO(sandersd): Change the tests to use a more unique message.
// See http://crbug.com/592067

// Common test results.
const char kFailed[] = "FAILED";

// Upper case event name set by Utils.installTitleEventHandler().
const char kEnded[] = "ENDED";
const char kErrorEvent[] = "ERROR";

// Lower case event name as set by Utils.failTest().
const char kError[] = "error";

const base::FilePath::CharType kTestDataPath[] =
    FILE_PATH_LITERAL("media/test/data");

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
  return file_path.Append(GetTestDataPath()).AppendASCII(name);
}

base::FilePath GetTestDataPath() {
  return base::FilePath(kTestDataPath);
}

std::string GetMimeTypeForFile(const std::string& file_name) {
  const auto& map = GetFileToMimeTypeMap();
  auto itr = map.find(file_name);
  CHECK(itr != map.end()) << ": file_name = " << file_name;
  return itr->second;
}

std::string GetURLQueryString(const base::StringPairs& query_params) {
  std::string query = "";
  auto itr = query_params.begin();
  for (; itr != query_params.end(); ++itr) {
    if (itr != query_params.begin())
      query.append("&");
    query.append(itr->first + "=" + itr->second);
  }
  return query;
}

scoped_refptr<DecoderBuffer> ReadTestDataFile(const std::string& name) {
  base::FilePath file_path = GetTestDataFilePath(name);

  int64_t tmp = 0;
  CHECK(base::GetFileSize(file_path, &tmp))
      << "Failed to get file size for '" << name << "'";

  int file_size = base::checked_cast<int>(tmp);

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(file_size));
  auto* data = reinterpret_cast<char*>(buffer->writable_data());
  CHECK_EQ(file_size, base::ReadFile(file_path, data, file_size))
      << "Failed to read '" << name << "'";

  return buffer;
}

bool LookupTestKeyVector(const std::vector<uint8_t>& key_id,
                         bool allow_rotation,
                         std::vector<uint8_t>* key) {
  std::vector<uint8_t> starting_key_id(kKeyId, kKeyId + arraysize(kKeyId));
  size_t rotate_limit = allow_rotation ? starting_key_id.size() : 1;
  for (size_t pos = 0; pos < rotate_limit; ++pos) {
    std::rotate(starting_key_id.begin(), starting_key_id.begin() + pos,
                starting_key_id.end());
    if (key_id == starting_key_id) {
      key->assign(kSecretKey, kSecretKey + arraysize(kSecretKey));
      std::rotate(key->begin(), key->begin() + pos, key->end());
      return true;
    }
  }
  return false;
}

bool LookupTestKeyString(const std::string& key_id,
                         bool allow_rotation,
                         std::string* key) {
  std::vector<uint8_t> key_vector;
  bool result =
      LookupTestKeyVector(std::vector<uint8_t>(key_id.begin(), key_id.end()),
                          allow_rotation, &key_vector);
  if (result)
    *key = std::string(key_vector.begin(), key_vector.end());
  return result;
}

}  // namespace media
