// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_linux.h"

#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "components/os_crypt/key_storage_config_linux.h"
#include "components/os_crypt/key_storage_util_linux.h"

#if defined(USE_LIBSECRET)
#include "components/os_crypt/key_storage_libsecret.h"
#endif
#if defined(USE_KEYRING)
#include "components/os_crypt/key_storage_keyring.h"
#endif
#if defined(USE_KWALLET)
#include "components/os_crypt/key_storage_kwallet.h"
#endif

#if defined(GOOGLE_CHROME_BUILD)
const char KeyStorageLinux::kFolderName[] = "Chrome Keys";
const char KeyStorageLinux::kKey[] = "Chrome Safe Storage";
#else
const char KeyStorageLinux::kFolderName[] = "Chromium Keys";
const char KeyStorageLinux::kKey[] = "Chromium Safe Storage";
#endif

// static
std::unique_ptr<KeyStorageLinux> KeyStorageLinux::CreateService(
    const os_crypt::Config& config) {
#if defined(USE_LIBSECRET) || defined(USE_KEYRING) || defined(USE_KWALLET)
  // Select a backend.
  bool use_backend = !config.should_use_preference ||
                     os_crypt::GetBackendUse(config.user_data_path);
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop_env =
      base::nix::GetDesktopEnvironment(env.get());
  os_crypt::SelectedLinuxBackend selected_backend =
      os_crypt::SelectBackend(config.store, use_backend, desktop_env);

  // TODO(crbug.com/782851) Schedule the initialisation on each backend's
  // favourite thread.

  // Try initializing the selected backend.
  // In case of GNOME_ANY, prefer Libsecret
  std::unique_ptr<KeyStorageLinux> key_storage;

#if defined(USE_LIBSECRET)
  if (selected_backend == os_crypt::SelectedLinuxBackend::GNOME_ANY ||
      selected_backend == os_crypt::SelectedLinuxBackend::GNOME_LIBSECRET) {
    key_storage.reset(new KeyStorageLibsecret());
    if (key_storage->Init()) {
      VLOG(1) << "OSCrypt using Libsecret as backend.";
      return key_storage;
    }
  }
#endif  // defined(USE_LIBSECRET)

#if defined(USE_KEYRING)
  if (selected_backend == os_crypt::SelectedLinuxBackend::GNOME_ANY ||
      selected_backend == os_crypt::SelectedLinuxBackend::GNOME_KEYRING) {
    key_storage.reset(new KeyStorageKeyring(config.main_thread_runner));
    if (key_storage->Init()) {
      VLOG(1) << "OSCrypt using Keyring as backend.";
      return key_storage;
    }
  }
#endif  // defined(USE_KEYRING)

#if defined(USE_KWALLET)
  if (selected_backend == os_crypt::SelectedLinuxBackend::KWALLET ||
      selected_backend == os_crypt::SelectedLinuxBackend::KWALLET5) {
    DCHECK(!config.product_name.empty());
    base::nix::DesktopEnvironment used_desktop_env =
        selected_backend == os_crypt::SelectedLinuxBackend::KWALLET
            ? base::nix::DESKTOP_ENVIRONMENT_KDE4
            : base::nix::DESKTOP_ENVIRONMENT_KDE5;
    key_storage.reset(
        new KeyStorageKWallet(used_desktop_env, config.product_name));
    if (key_storage->Init()) {
      VLOG(1) << "OSCrypt using KWallet as backend.";
      return key_storage;
    }
  }
#endif  // defined(USE_KWALLET)
#endif  // defined(USE_LIBSECRET) || defined(USE_KEYRING) ||
        // defined(USE_KWALLET)

  // The appropriate store was not available.
  VLOG(1) << "OSCrypt did not initialize a backend.";
  return nullptr;
}

std::string KeyStorageLinux::GetKey() {
  // TODO(crbug.com/782851) Schedule this operation on the backend's favourite
  // thread.
  return GetKeyImpl();
}
