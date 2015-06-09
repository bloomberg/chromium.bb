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
#elif defined(OS_POSIX) && !defined(OS_MACOSX) && defined(USE_NSS_CERTS)
#include "crypto/nss_util.h"
#endif
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"
#endif

namespace chrome {

void ChromeContentPluginClient::PreSandboxInitialization() {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
#endif

#if defined(ENABLE_REMOTING)

  // Load crypto libraries for the Chromoting client plugin.
#if defined(OS_POSIX) && !defined(OS_MACOSX) && defined(USE_NSS_CERTS)
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

  // Initialize media libraries for the Chromoting client plugin.
  media::InitializeMediaLibrary();

#endif // defined(ENABLE_REMOTING)
}

}  // namespace chrome
