// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/extensions/extensions_handler.h"

#include "chrome/common/extensions/chrome_extensions_client.h"
#include "media/base/media.h"

namespace extensions {

// static
void InitExtensionsClient() {
  ExtensionsClient::Set(ChromeExtensionsClient::GetInstance());
}

// static
void PreSandboxStartup() {
  media::InitializeMediaLibrary();  // Used for media file validation.
}

}  // namespace extensions
