// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_CONTENT_BROWSER_CLIENT_H_
#define EXTENSIONS_TEST_TEST_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "base/strings/string_piece.h"
#include "content/public/browser/content_browser_client.h"

namespace base {
class Value;
}

namespace extensions {

class TestContentBrowserClient : public content::ContentBrowserClient {
 public:
  TestContentBrowserClient();
  ~TestContentBrowserClient() override;

  // content::ContentBrowserClient:
  std::unique_ptr<base::Value> GetServiceManifestOverlay(
      base::StringPiece name) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_CONTENT_BROWSER_CLIENT_H_
