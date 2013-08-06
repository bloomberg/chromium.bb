// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_WATCHER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_WATCHER_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/weak_ptr.h"

namespace file_manager {

// This class is used to watch changes in the given virtual path, remember
// what extensions are watching the path.
//
// For local files, the class maintains a FilePathWatcher instance and
// remembers what extensions are watching the path.
//
// For remote files (ex. files on Drive), the class just remembers what
// extensions are watching the path. The actual file watching for remote
// files is handled differently in EventRouter.
class FileWatcher {
 public:
  typedef std::map<std::string, int> ExtensionUsageRegistry;
  typedef base::Callback<void(bool success)> BoolCallback;

  // AddExtension() is internally called for |extension_id|.
  // TODO(satorux): Remove |extension_id| and stop calling AddExtension().
  FileWatcher(const base::FilePath& virtual_path,
              const std::string& extension_id,
              bool is_remote_file_system);

  ~FileWatcher();

  // Remembers that the extension of |extension_id| is watching the virtual
  // path.
  //
  // If this function is called more than once with the same extension ID,
  // the class increments the counter internally, and RemoveExtension()
  // decrements the counter, and forgets the extension when the counter
  // becomes zero.
  void AddExtension(const std::string& extension_id);

  // Forgets that the extension of |extension_id| is watching the virtual path,
  // or just decrements the internal counter for the extension ID. See the
  // comment at AddExtension() for details.
  void RemoveExtension(const std::string& extension_id);

  // Returns IDs of the extensions watching virtual_path.
  // TODO(satorux): Should just return a list of extension IDs rather than a
  // map.
  const ExtensionUsageRegistry& extensions() const { return extensions_; }

  // Returns 0 when no extensions are watching the virtual path.
  // TODO(satorux): Should be replaced with extensions().empty().
  int ref_count() const { return ref_count_; }

  // Returns the path being watched.
  const base::FilePath& virtual_path() const { return virtual_path_; }

  // Starts watching |local_path|. For a local file, a base::FilePathWatcher
  // will be created and |file_watcher_callback| will be called when changes
  // are notified.
  //
  // For a remote file, this function actually does nothing but
  // runs |callback| with true. See also the class comment.
  // TODO(satorux): This function shouldn't be called for remote files.
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

  base::FilePathWatcher* local_file_watcher_;
  base::FilePath local_path_;
  base::FilePath virtual_path_;
  ExtensionUsageRegistry extensions_;
  int ref_count_;
  bool is_remote_file_system_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileWatcher> weak_ptr_factory_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_WATCHER_H_
