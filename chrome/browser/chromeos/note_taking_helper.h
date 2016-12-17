// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTE_TAKING_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_NOTE_TAKING_HELPER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class Extension;
namespace api {
namespace app_runtime {
struct ActionData;
}  // namespace app_runtime
}  // namespace api
}  // namespace extensions

namespace chromeos {

// Information about an installed note-taking app.
struct NoteTakingAppInfo {
  // Application name to display to user.
  std::string name;

  // Either an extension ID (in the case of a Chrome app) or a package name (in
  // the case of an Android app).
  std::string app_id;

  // True if this is the preferred note-taking app.
  bool preferred;
};

using NoteTakingAppInfos = std::vector<NoteTakingAppInfo>;

// Singleton class used to launch a note-taking app.
class NoteTakingHelper {
 public:
  // Callback used to launch a Chrome app.
  using LaunchChromeAppCallback = base::Callback<void(
      Profile*,
      const extensions::Extension*,
      std::unique_ptr<extensions::api::app_runtime::ActionData>,
      const base::FilePath&)>;

  // Extension IDs for the development and released versions of the Google Keep
  // Chrome app.
  static const char kDevKeepExtensionId[];
  static const char kProdKeepExtensionId[];

  static void Initialize();
  static void Shutdown();
  static NoteTakingHelper* Get();

  void set_launch_chrome_app_callback_for_test(
      const LaunchChromeAppCallback& callback) {
    launch_chrome_app_callback_ = callback;
  }

  // Returns a list of available note-taking apps.
  std::vector<NoteTakingAppInfo> GetAvailableApps(Profile* profile);

  // Sets the preferred note-taking app. |app_id| is a value from a
  // NoteTakingAppInfo object.
  void SetPreferredApp(Profile* profile, const std::string& app_id);

  // Returns true if an app that can be used to take notes is available. UI
  // surfaces that call LaunchAppForNewNote() should be hidden otherwise.
  bool IsAppAvailable(Profile* profile);

  // Launches the note-taking app to create a new note, optionally additionally
  // passing a file (|path| may be empty). IsAppAvailable() must be called
  // first.
  void LaunchAppForNewNote(Profile* profile, const base::FilePath& path);

 private:
  NoteTakingHelper();
  ~NoteTakingHelper();

  // Helper method that launches |app_id| (either an Android package name or a
  // Chrome extension ID) to create a new note with an optional attached file at
  // |path|. Returns false if the app couldn't be launched.
  bool LaunchAppInternal(Profile* profile,
                         const std::string& app_id,
                         const base::FilePath& path);

  // Callback used to launch Chrome apps. Can be overridden for tests.
  LaunchChromeAppCallback launch_chrome_app_callback_;

  DISALLOW_COPY_AND_ASSIGN(NoteTakingHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTE_TAKING_HELPER_H_
