// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_content_client.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"

namespace content {

TestContentClient::TestContentClient()
    : data_pack_(ui::SCALE_FACTOR_100P) {
  // content_resources.pak is not built on iOS as it is not required.
#if !defined(OS_IOS)
  base::FilePath content_resources_pack_path;
#if defined(OS_ANDROID)
  // on Android all pak files are inside the paks folder.
  PathService::Get(base::DIR_ANDROID_APP_DATA, &content_resources_pack_path);
  content_resources_pack_path = content_resources_pack_path.Append(
      FILE_PATH_LITERAL("paks"));
#else
  PathService::Get(base::DIR_MODULE, &content_resources_pack_path);
#endif
  content_resources_pack_path = content_resources_pack_path.Append(
      FILE_PATH_LITERAL("content_resources.pak"));
  data_pack_.LoadFromPath(content_resources_pack_path);
#endif
}

TestContentClient::~TestContentClient() {
}

std::string TestContentClient::GetUserAgent() const {
  return std::string("TestContentClient");
}

base::StringPiece TestContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  base::StringPiece resource;
  data_pack_.GetStringPiece(resource_id, &resource);
  return resource;
}

}  // namespace content
