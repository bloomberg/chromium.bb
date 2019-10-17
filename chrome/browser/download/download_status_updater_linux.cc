// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include <dlfcn.h>

#include <memory>
#include <string>

#include "base/environment.h"
#include "base/memory/protected_memory.h"
#include "base/memory/protected_memory_cfi.h"
#include "base/nix/xdg_util.h"
#include "chrome/common/channel_info.h"
#include "ui/base/glib/glib_integers.h"

// Unity data typedefs.
typedef struct _UnityInspector UnityInspector;
typedef UnityInspector* (*unity_inspector_get_default_func)(void);
typedef gboolean (*unity_inspector_get_unity_running_func)(
    UnityInspector* self);

typedef struct _UnityLauncherEntry UnityLauncherEntry;
typedef UnityLauncherEntry* (*unity_launcher_entry_get_for_desktop_id_func)(
    const gchar* desktop_id);
typedef void (*unity_launcher_entry_set_count_func)(UnityLauncherEntry* self,
                                                    gint64 value);
typedef void (*unity_launcher_entry_set_count_visible_func)(
    UnityLauncherEntry* self,
    gboolean value);
typedef void (*unity_launcher_entry_set_progress_func)(UnityLauncherEntry* self,
                                                       gdouble value);
typedef void (*unity_launcher_entry_set_progress_visible_func)(
    UnityLauncherEntry* self,
    gboolean value);

namespace {

bool attempted_load = false;

// Unity has a singleton object that we can ask whether the unity is running.
UnityInspector* inspector = nullptr;

// A link to the desktop entry in the panel.
UnityLauncherEntry* chrome_entry = nullptr;

// Retrieved functions from libunity.
PROTECTED_MEMORY_SECTION
base::ProtectedMemory<unity_inspector_get_default_func> inspector_get_default;
PROTECTED_MEMORY_SECTION
base::ProtectedMemory<unity_inspector_get_unity_running_func> get_unity_running;
PROTECTED_MEMORY_SECTION
base::ProtectedMemory<unity_launcher_entry_get_for_desktop_id_func>
    entry_get_for_desktop_id;
PROTECTED_MEMORY_SECTION
base::ProtectedMemory<unity_launcher_entry_set_count_func> entry_set_count;
PROTECTED_MEMORY_SECTION
base::ProtectedMemory<unity_launcher_entry_set_count_visible_func>
    entry_set_count_visible;
PROTECTED_MEMORY_SECTION
base::ProtectedMemory<unity_launcher_entry_set_progress_func>
    entry_set_progress;
PROTECTED_MEMORY_SECTION
base::ProtectedMemory<unity_launcher_entry_set_progress_visible_func>
    entry_set_progress_visible;

void EnsureLibUnityLoaded() {
  using base::nix::GetDesktopEnvironment;

  if (attempted_load)
    return;
  attempted_load = true;

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop_env = GetDesktopEnvironment(env.get());

  // The "icon-tasks" KDE task manager also honors Unity Launcher API.
  if (desktop_env != base::nix::DESKTOP_ENVIRONMENT_UNITY &&
      desktop_env != base::nix::DESKTOP_ENVIRONMENT_KDE4 &&
      desktop_env != base::nix::DESKTOP_ENVIRONMENT_KDE5)
    return;

  // Ubuntu still hasn't given us a nice libunity.so symlink.
  void* unity_lib = dlopen("libunity.so.4", RTLD_LAZY);
  if (!unity_lib)
    unity_lib = dlopen("libunity.so.6", RTLD_LAZY);
  if (!unity_lib)
    unity_lib = dlopen("libunity.so.9", RTLD_LAZY);
  if (!unity_lib)
    return;

  static base::ProtectedMemory<unity_inspector_get_default_func>::Initializer
      inspector_get_default_init(
          &inspector_get_default,
          reinterpret_cast<unity_inspector_get_default_func>(
              dlsym(unity_lib, "unity_inspector_get_default")));
  if (*inspector_get_default) {
    inspector = UnsanitizedCfiCall(inspector_get_default)();

    static base::ProtectedMemory<unity_inspector_get_unity_running_func>::
        Initializer get_unity_running_init(
            &get_unity_running,
            reinterpret_cast<unity_inspector_get_unity_running_func>(
                dlsym(unity_lib, "unity_inspector_get_unity_running")));
  }

  static base::ProtectedMemory<unity_launcher_entry_get_for_desktop_id_func>::
      Initializer entry_get_for_desktop_id_init(
          &entry_get_for_desktop_id,
          reinterpret_cast<unity_launcher_entry_get_for_desktop_id_func>(
              dlsym(unity_lib, "unity_launcher_entry_get_for_desktop_id")));
  if (*entry_get_for_desktop_id) {
    std::string desktop_id = chrome::GetDesktopName(env.get());
    chrome_entry =
        UnsanitizedCfiCall(entry_get_for_desktop_id)(desktop_id.c_str());

    static base::ProtectedMemory<unity_launcher_entry_set_count_func>::
        Initializer entry_set_count_init(
            &entry_set_count,
            reinterpret_cast<unity_launcher_entry_set_count_func>(
                dlsym(unity_lib, "unity_launcher_entry_set_count")));

    static base::ProtectedMemory<unity_launcher_entry_set_count_visible_func>::
        Initializer entry_set_count_visible_init(
            &entry_set_count_visible,
            reinterpret_cast<unity_launcher_entry_set_count_visible_func>(
                dlsym(unity_lib, "unity_launcher_entry_set_count_visible")));

    static base::ProtectedMemory<unity_launcher_entry_set_progress_func>::
        Initializer entry_set_progress_init(
            &entry_set_progress,
            reinterpret_cast<unity_launcher_entry_set_progress_func>(
                dlsym(unity_lib, "unity_launcher_entry_set_progress")));

    static base::ProtectedMemory<
        unity_launcher_entry_set_progress_visible_func>::Initializer
        entry_set_progress_visible_init(
            &entry_set_progress_visible,
            reinterpret_cast<unity_launcher_entry_set_progress_visible_func>(
                dlsym(unity_lib, "unity_launcher_entry_set_progress_visible")));
  }
}

bool IsRunning() {
  return inspector && *get_unity_running &&
         UnsanitizedCfiCall(get_unity_running)(inspector);
}

void SetDownloadCount(int count) {
  if (chrome_entry && *entry_set_count && *entry_set_count_visible) {
    UnsanitizedCfiCall(entry_set_count)(chrome_entry, count);
    UnsanitizedCfiCall(entry_set_count_visible)(chrome_entry, count != 0);
  }
}

void SetProgressFraction(float percentage) {
  if (chrome_entry && *entry_set_progress && *entry_set_progress_visible) {
    UnsanitizedCfiCall(entry_set_progress)(chrome_entry, percentage);
    UnsanitizedCfiCall(entry_set_progress_visible)(
        chrome_entry, percentage > 0.0 && percentage < 1.0);
  }
}

}  // namespace

void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    download::DownloadItem* download) {
  // Only implemented on Unity for now.
  EnsureLibUnityLoaded();
  if (!IsRunning())
    return;
  float progress = 0;
  int download_count = 0;
  GetProgress(&progress, &download_count);
  SetDownloadCount(download_count);
  SetProgressFraction(progress);
}
