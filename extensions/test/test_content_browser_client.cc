// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_content_browser_client.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/values.h"
#include "content/public/common/service_names.mojom.h"
#include "extensions/common/extension_paths.h"

namespace extensions {

TestContentBrowserClient::TestContentBrowserClient() = default;

TestContentBrowserClient::~TestContentBrowserClient() = default;

std::unique_ptr<base::Value>
TestContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  if (name != content::mojom::kUtilityServiceName)
    return nullptr;

  base::FilePath extensions_test_data_dir;
  CHECK(base::PathService::Get(DIR_TEST_DATA, &extensions_test_data_dir));

  std::string contents;
  CHECK(base::ReadFileToString(
      extensions_test_data_dir.AppendASCII(
          "extensions_test_content_utility_manifest_overlay.json"),
      &contents));

  return base::JSONReader::Read(contents);
}

}  // namespace extensions
