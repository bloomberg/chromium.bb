// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CDM_INFO_H_
#define CONTENT_PUBLIC_COMMON_CDM_INFO_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/version.h"
#include "content/common/content_export.h"
#include "media/base/video_codecs.h"

namespace content {

// Represents a Content Decryption Module implementation and its capabilities.
struct CONTENT_EXPORT CdmInfo {
  CdmInfo(const std::string& name,
          const std::string& guid,
          const base::Version& version,
          const base::FilePath& path,
          const std::string& file_system_id,
          const std::vector<media::VideoCodec>& supported_video_codecs,
          bool supports_persistent_license,
          const std::string& supported_key_system,
          bool supports_sub_key_systems);
  CdmInfo(const CdmInfo& other);
  ~CdmInfo();

  // Display name of the CDM (e.g. Widevine Content Decryption Module).
  std::string name;

  // A version 4 GUID to uniquely identify this type of CDM.
  std::string guid;

  // Version of the CDM. May be empty if the version is not known.
  base::Version version;

  // Path to the library implementing the CDM. May be empty if the
  // CDM is not a separate library (e.g. Widevine on Android).
  base::FilePath path;

  // Identifier used by the PluginPrivateFileSystem to identify the files
  // stored by this CDM. Valid identifiers only contain letters (A-Za-z),
  // digits(0-9), or "._-".
  std::string file_system_id;

  // List of video codecs supported by the CDM (e.g. vp8). This is the set of
  // codecs that can be decrypted and decoded by the CDM. As this is generic,
  // not all profiles or levels of the specified codecs may actually be
  // supported.
  // TODO(crbug.com/796725) Find a way to include profiles and levels.
  std::vector<media::VideoCodec> supported_video_codecs;

  // Whether this CDM supports persistent licenses.
  bool supports_persistent_license;

  // The key system supported by this CDM.
  std::string supported_key_system;

  // Whether we also support sub key systems of the |supported_key_system|.
  // A sub key system to a key system is like a sub domain to a domain.
  // For example, com.example.somekeysystem.a and com.example.somekeysystem.b
  // are both sub key systems of com.example.somekeysystem.
  bool supports_sub_key_systems = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CDM_INFO_H_
