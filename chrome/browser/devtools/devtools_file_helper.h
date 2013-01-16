// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

class FilePath;
class Profile;

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

  DevToolsFileHelper(content::WebContents* web_contents, Profile* profile);
  ~DevToolsFileHelper();

  typedef base::Callback<void(void)> SaveCallback;
  typedef base::Callback<void(void)> AppendCallback;
  typedef base::Callback<
      void(const std::vector<DevToolsFileHelper::FileSystem>&)>
      RequestFileSystemsCallback;
  typedef base::Callback<
      void(std::string, const DevToolsFileHelper::FileSystem&)>
      AddFileSystemCallback;

  // Saves |content| to the file and associates its path with given |url|.
  // If client is calling this method with given |url| for the first time
  // or |save_as| is true, confirmation dialog is shown to the user.
  void Save(const std::string& url,
            const std::string& content,
            bool save_as,
            const SaveCallback& callback);

  // Append |content| to the file that has been associated with given |url|.
  // The |url| can be associated with a file via calling Save method.
  // If the Save method has not been called for this |url|, then
  // Append method does nothing.
  void Append(const std::string& url,
              const std::string& content,
              const AppendCallback& callback);

  // Shows select folder dialog.
  // If user cancels folder selection, passes empty FileSystem struct to
  // |callback|.
  // If selected folder contains magic file, grants renderer read/write
  // permissions, registers isolated file system for it and passes FileSystem
  // struct to |callback|. Saves file system path to prefs.
  // If selected folder does not contain magic file, passes error string to
  // |callback|.
  void AddFileSystem(const AddFileSystemCallback& callback);

  // Loads file system paths from prefs, grants permissions and registers
  // isolated file system for those of them that contain magic file and passes
  // FileSystem structs for registered file systems to |callback|.
  void RequestFileSystems(const RequestFileSystemsCallback& callback);

  // Removes isolated file system for given |file_system_path|.
  void RemoveFileSystem(const std::string& file_system_path);

 private:
  void SaveAsFileSelected(const std::string& url,
                          const std::string& content,
                          const SaveCallback& callback,
                          const FilePath& path);
  void SaveAsFileSelectionCanceled();
  void InnerAddFileSystem(const AddFileSystemCallback& callback,
                          const FilePath& path);
  void AddValidatedFileSystem(const AddFileSystemCallback& callback,
                              const std::vector<FilePath>& permitted_paths);
  void RestoreValidatedFileSystems(const RequestFileSystemsCallback& callback,
                                   const std::vector<FilePath>& file_paths);

  content::WebContents* web_contents_;
  Profile* profile_;
  base::WeakPtrFactory<DevToolsFileHelper> weak_factory_;
  typedef std::map<std::string, FilePath> PathsMap;
  PathsMap saved_files_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsFileHelper);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_
