// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/chrome_content_plugin_client.h"

#if defined(ENABLE_REMOTING)
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/common/content_paths.h"
#include "media/base/media.h"
#if defined(OS_WIN)
#include "base/logging.h"
#include "base/native_library.h"
#elif defined(OS_POSIX) && !defined(OS_MACOSX) && defined(USE_NSS)
#include "crypto/nss_util.h"
#endif
#endif

namespace chrome {

void ChromeContentPluginClient::PreSandboxInitialization() {
#if defined(ENABLE_REMOTING)

  // Load crypto libraries for the Chromoting client plugin.
#if defined(OS_POSIX) && !defined(OS_MACOSX) && defined(USE_NSS)
  // On platforms where we use system NSS libraries, the .so's must be loaded
  // before the sandbox is initialized.
  crypto::ForceNSSNoDBInit();
  crypto::EnsureNSSInit();
#elif defined(OS_WIN)
  // crypt32.dll is used to decode X509 certificates for Chromoting.
  base::NativeLibraryLoadError error;
  if (base::LoadNativeLibrary(base::FilePath(L"crypt32.dll"), &error) == NULL)
    LOG(ERROR) << "Failed to load crypto32.dll: " << error.ToString();
#endif // defined(OS_WIN)

  // Load media libraries for the Chromoting client plugin.
  base::FilePath media_path;
  PathService::Get(content::DIR_MEDIA_LIBS, &media_path);
  if (!media_path.empty())
    media::InitializeMediaLibrary(media_path);

#endif // defined(ENABLE_REMOTING)
}

}  // namespace chrome
