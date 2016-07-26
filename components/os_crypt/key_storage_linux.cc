// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_linux.h"

#include "base/environment.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"

#if defined(USE_LIBSECRET)
#include "components/os_crypt/key_storage_libsecret.h"
#endif

#if defined(USE_KWALLET)
#include "components/os_crypt/key_storage_kwallet.h"
#endif

#if defined(OFFICIAL_BUILD)
const char KeyStorageLinux::kFolderName[] = "Chrome Keys";
const char KeyStorageLinux::kKey[] = "Chrome Safe Storage";
#else
const char KeyStorageLinux::kFolderName[] = "Chromium Keys";
const char KeyStorageLinux::kKey[] = "Chromium Safe Storage";
#endif

base::LazyInstance<std::string> g_store = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::string> g_product_name = LAZY_INSTANCE_INITIALIZER;

// static
void KeyStorageLinux::SetStore(const std::string& store_type) {
  g_store.Get() = store_type;
  VLOG(1) << "OSCrypt store set to " << store_type;
}

// static
void KeyStorageLinux::SetProductName(const std::string& product_name) {
  g_product_name.Get() = product_name;
}

// static
std::unique_ptr<KeyStorageLinux> KeyStorageLinux::CreateService() {
  base::nix::DesktopEnvironment used_desktop_env;
  if (g_store.Get() == "kwallet") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_KDE4;
  } else if (g_store.Get() == "kwallet5") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_KDE5;
  } else if (g_store.Get() == "gnome") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_GNOME;
  } else if (g_store.Get() == "basic") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_OTHER;
  } else {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    used_desktop_env = base::nix::GetDesktopEnvironment(env.get());
  }

  // Try initializing the appropriate store for our environment.
  std::unique_ptr<KeyStorageLinux> key_storage;
  if (used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_GNOME ||
      used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_UNITY ||
      used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_XFCE) {
#if defined(USE_LIBSECRET)
    key_storage.reset(new KeyStorageLibsecret());
    if (key_storage->Init()) {
      VLOG(1) << "OSCrypt using Libsecret as backend.";
      return key_storage;
    }
#endif
  } else if (used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE4 ||
             used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_KDE5) {
#if defined(USE_KWALLET)
    DCHECK(!g_product_name.Get().empty());
    key_storage.reset(
        new KeyStorageKWallet(used_desktop_env, g_product_name.Get()));
    if (key_storage->Init()) {
      VLOG(1) << "OSCrypt using KWallet as backend.";
      return key_storage;
    }
#endif
  }

  // The appropriate store was not available.
  VLOG(1) << "OSCrypt could not initialize a backend.";
  return nullptr;
}
