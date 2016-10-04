// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/prefs/pref_change_registrar.h"

class DevToolsFileWatcher;
class Profile;

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

class DevToolsFileHelper {
 public:
  struct FileSystem {
    FileSystem();
    FileSystem(const std::string& file_system_name,
               const std::string& root_url,
               const std::string& file_system_path);

    std::string file_system_name;
    std::string root_url;
    std::string file_system_path;
  };

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void FileSystemAdded(const FileSystem& file_system) = 0;
    virtual void FileSystemRemoved(const std::string& file_system_path) = 0;
    virtual void FilePathsChanged(
        const std::vector<std::string>& changed_paths,
        const std::vector<std::string>& added_paths,
        const std::vector<std::string>& removed_paths) = 0;
  };

  DevToolsFileHelper(content::WebContents* web_contents, Profile* profile,
                     Delegate* delegate);
  ~DevToolsFileHelper();

  typedef base::Callback<void(void)> SaveCallback;
  typedef base::Callback<void(void)> AppendCallback;
  typedef base::Callback<void(const base::string16&,
                              const base::Callback<void(bool)>&)>
      ShowInfoBarCallback;

  // Saves |content| to the file and associates its path with given |url|.
  // If client is calling this method with given |url| for the first time
  // or |save_as| is true, confirmation dialog is shown to the user.
  void Save(const std::string& url,
            const std::string& content,
            bool save_as,
            const SaveCallback& saveCallback,
            const SaveCallback& cancelCallback);

  // Append |content| to the file that has been associated with given |url|.
  // The |url| can be associated with a file via calling Save method.
  // If the Save method has not been called for this |url|, then
  // Append method does nothing.
  void Append(const std::string& url,
              const std::string& content,
              const AppendCallback& callback);

  // 1. If empty |file_system_path| is passed, shows select folder dialog.
  //    If user cancels folder selection, passes empty FileSystem struct to
  //    |callback|.
  //    If non-empty |file_system_path| is passed, verifies that corresponding
  //    folder contains the ".devtools" project file, if not, passes empty
  //    FileSystem struct to |callback|.
  // 2. Shows infobar by means of |show_info_bar_callback| to let the user
  //    decide whether to grant security permissions or not.
  //    If user allows adding file system in infobar, grants renderer
  //    read/write permissions, registers isolated file system for it and
  //    passes FileSystem struct to |callback|. Saves file system path to prefs.
  //    If user denies adding file system in infobar, passes error string to
  //    |callback|.
  void AddFileSystem(const std::string& file_system_path,
                     const ShowInfoBarCallback& show_info_bar_callback);

  // Upgrades dragged file system permissions to a read-write access.
  // Shows infobar by means of |show_info_bar_callback| to let the user decide
  // whether to grant security permissions or not.
  // If user allows adding file system in infobar, grants renderer read/write
  // permissions, registers isolated file system for it and passes FileSystem
  // struct to |callback|. Saves file system path to prefs.
  // If user denies adding file system in infobar, passes error string to
  // |callback|.
  void UpgradeDraggedFileSystemPermissions(
      const std::string& file_system_url,
      const ShowInfoBarCallback& show_info_bar_callback);

  // Loads file system paths from prefs, grants permissions and registers
  // isolated file system for those of them that contain magic file and passes
  // FileSystem structs for registered file systems to |callback|.
  std::vector<FileSystem> GetFileSystems();

  // Removes isolated file system for given |file_system_path|.
  void RemoveFileSystem(const std::string& file_system_path);

  // Returns whether access to the folder on given |file_system_path| was
  // granted.
  bool IsFileSystemAdded(const std::string& file_system_path);

 private:
  void SaveAsFileSelected(const std::string& url,
                          const std::string& content,
                          const SaveCallback& callback,
                          const base::FilePath& path);
  void InnerAddFileSystem(
      const ShowInfoBarCallback& show_info_bar_callback,
      const base::FilePath& path);
  void CheckProjectFileExistsAndAddFileSystem(
      const ShowInfoBarCallback& show_info_bar_callback,
      const base::FilePath& path);
  void AddUserConfirmedFileSystem(
      const base::FilePath& path,
      bool allowed);
  void FileSystemPathsSettingChanged();
  void FilePathsChanged(const std::vector<std::string>& changed_paths,
                        const std::vector<std::string>& added_paths,
                        const std::vector<std::string>& removed_paths);

  content::WebContents* web_contents_;
  Profile* profile_;
  DevToolsFileHelper::Delegate* delegate_;
  typedef std::map<std::string, base::FilePath> PathsMap;
  PathsMap saved_files_;
  PrefChangeRegistrar pref_change_registrar_;
  std::set<std::string> file_system_paths_;
  std::unique_ptr<DevToolsFileWatcher> file_watcher_;
  base::WeakPtrFactory<DevToolsFileHelper> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsFileHelper);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_
