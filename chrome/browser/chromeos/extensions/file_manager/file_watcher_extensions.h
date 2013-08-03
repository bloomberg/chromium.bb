// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_WATCHER_EXTENSIONS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_WATCHER_EXTENSIONS_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/weak_ptr.h"

namespace file_manager {

// This class is used to remember what extensions are watching |virtual_path|.
class FileWatcherExtensions {
 public:
  typedef std::map<std::string, int> ExtensionUsageRegistry;
  typedef base::Callback<void(bool success)> BoolCallback;

  FileWatcherExtensions(const base::FilePath& virtual_path,
                        const std::string& extension_id,
                        bool is_remote_file_system);

  ~FileWatcherExtensions();

  void AddExtension(const std::string& extension_id);

  void RemoveExtension(const std::string& extension_id);

  const ExtensionUsageRegistry& extensions() const { return extensions_; }

  int ref_count() const { return ref_count_; }

  const base::FilePath& virtual_path() const { return virtual_path_; }

  // Starts a file watch at |local_path|. |file_watcher_callback| will be
  // called when changes are notified.
  //
  // |callback| will be called with true, if the file watch is started
  // successfully, or false if failed. |callback| must not be null.
  void Watch(const base::FilePath& local_path,
             const base::FilePathWatcher::Callback& file_watcher_callback,
             const BoolCallback& callback);

 private:
  // Called when a FilePathWatcher is created and started.
  // |file_path_watcher| is NULL, if the watcher wasn't started successfully.
  void OnWatcherStarted(const BoolCallback& callback,
                        base::FilePathWatcher* file_path_watcher);

  base::FilePathWatcher* file_watcher_;
  base::FilePath local_path_;
  base::FilePath virtual_path_;
  ExtensionUsageRegistry extensions_;
  int ref_count_;
  bool is_remote_file_system_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileWatcherExtensions> weak_ptr_factory_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_WATCHER_EXTENSIONS_H_
