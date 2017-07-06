// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_linux.h"

#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/single_thread_task_runner.h"
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

namespace {

// Parameters to OSCrypt, which are set before the first call to OSCrypt, are
// stored here.
struct Configuration {
  std::string store;
  std::string product_name;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner;
  bool should_use_preference;
  base::FilePath user_data_path;
};

base::LazyInstance<Configuration>::DestructorAtExit g_config =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void KeyStorageLinux::SetStore(const std::string& store_type) {
  g_config.Get().store = store_type;
  VLOG(1) << "OSCrypt store set to " << store_type;
}

// static
void KeyStorageLinux::SetProductName(const std::string& product_name) {
  g_config.Get().product_name = product_name;
}

// static
void KeyStorageLinux::SetMainThreadRunner(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner) {
  g_config.Get().main_thread_runner = main_thread_runner;
}

// static
void KeyStorageLinux::ShouldUsePreference(bool should_use_preference) {
  g_config.Get().should_use_preference = should_use_preference;
}

// static
void KeyStorageLinux::SetUserDataPath(const base::FilePath& path) {
  g_config.Get().user_data_path = path;
}

// static
std::unique_ptr<KeyStorageLinux> KeyStorageLinux::CreateService() {
#if defined(USE_LIBSECRET) || defined(USE_KEYRING) || defined(USE_KWALLET)
  // Select a backend.
  bool use_backend = !g_config.Get().should_use_preference ||
                     os_crypt::GetBackendUse(g_config.Get().user_data_path);
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop_env =
      base::nix::GetDesktopEnvironment(env.get());
  os_crypt::SelectedLinuxBackend selected_backend =
      os_crypt::SelectBackend(g_config.Get().store, use_backend, desktop_env);

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
    key_storage.reset(new KeyStorageKeyring(g_config.Get().main_thread_runner));
    if (key_storage->Init()) {
      VLOG(1) << "OSCrypt using Keyring as backend.";
      return key_storage;
    }
  }
#endif  // defined(USE_KEYRING)

#if defined(USE_KWALLET)
  if (selected_backend == os_crypt::SelectedLinuxBackend::KWALLET ||
      selected_backend == os_crypt::SelectedLinuxBackend::KWALLET5) {
    DCHECK(!g_config.Get().product_name.empty());
    base::nix::DesktopEnvironment used_desktop_env =
        selected_backend == os_crypt::SelectedLinuxBackend::KWALLET
            ? base::nix::DESKTOP_ENVIRONMENT_KDE4
            : base::nix::DESKTOP_ENVIRONMENT_KDE5;
    key_storage.reset(
        new KeyStorageKWallet(used_desktop_env, g_config.Get().product_name));
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
