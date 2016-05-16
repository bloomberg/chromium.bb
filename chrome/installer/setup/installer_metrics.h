// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_INSTALLER_METRICS_H_
#define CHROME_INSTALLER_SETUP_INSTALLER_METRICS_H_

namespace base {
class FilePath;
}  // namespace base

namespace installer {

// Returns the directory in which persistent histograms will be saved. The
// install process should create this when appropriate and remove it on
// uninstall/rollback. If this directory does not exist when End...Storage()
// is called then no histograms will be persisted on disk.
base::FilePath GetPersistentHistogramStorageDir(
    const base::FilePath& target_dir);

// Creates a persistent space for any histograms that can be reported later
// by the browser process. It should be called as early as possible in the
// process lifetime and never called again.
void BeginPersistentHistogramStorage();

// Saves all the persistent histograms to a single file on disk for reading
// by the browser so it can be reported. It is generally called once during
// process exit. Multiple calls to this are possible if "snapshots" are
// desired; they are cumulative in what is saved, not just what has changed
// since the previous call. The |target_dir| is the install location of the
// browser.
void EndPersistentHistogramStorage(const base::FilePath& target_dir,
                                   bool system_install);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALLER_METRICS_H_
