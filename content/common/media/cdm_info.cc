// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/cdm_info.h"

#include "base/guid.h"
#include "base/logging.h"

namespace content {

CdmInfo::CdmInfo(const std::string& name,
                 const std::string& guid,
                 const base::Version& version,
                 const base::FilePath& path,
                 const std::vector<std::string>& supported_codecs,
                 const std::string& supported_key_system,
                 bool supports_sub_key_systems)
    : name(name),
      guid(guid),
      version(version),
      path(path),
      supported_codecs(supported_codecs),
      supported_key_system(supported_key_system),
      supports_sub_key_systems(supports_sub_key_systems) {
  DCHECK(base::IsValidGUID(guid));
}

CdmInfo::CdmInfo(const CdmInfo& other) = default;

CdmInfo::~CdmInfo() {}

}  // namespace content
