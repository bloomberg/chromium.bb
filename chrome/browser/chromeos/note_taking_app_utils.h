// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTE_TAKING_APP_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_NOTE_TAKING_APP_UTILS_H_

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {

// Returns true if an app that can be used to take notes is available.
bool IsNoteTakingAppAvailable(Profile* profile);

// Launches the note-taking app to create a new note, optionally additionally
// passing a file (|path| may be empty). IsNoteTakingAppAvailable() must be
// called first.
void LaunchNoteTakingAppForNewNote(Profile* profile,
                                   const base::FilePath& path);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTE_TAKING_APP_UTILS_H_
