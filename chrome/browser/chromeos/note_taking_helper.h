// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTE_TAKING_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_NOTE_TAKING_HELPER_H_

#include <base/macros.h>

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {

// Singleton class used to launch a note-taking app.
class NoteTakingHelper {
 public:
  static void Initialize();
  static void Shutdown();
  static NoteTakingHelper* Get();

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

  DISALLOW_COPY_AND_ASSIGN(NoteTakingHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTE_TAKING_HELPER_H_
