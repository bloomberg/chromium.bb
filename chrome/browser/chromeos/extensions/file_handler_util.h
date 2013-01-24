// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_HANDLER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_HANDLER_UTIL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/platform_file.h"
#include "chrome/common/extensions/extension.h"
#include "extensions/common/url_pattern_set.h"

class Browser;
class FileBrowserHandler;
class GURL;
class Profile;

namespace extensions {
class Extension;
}  // namespace extensions

namespace fileapi {
class FileSystemURL;
}

namespace file_handler_util {

// Specifies the task type for a task id that represents some file action, Drive
// action, or Web Intent action.
extern const char kTaskFile[];
extern const char kTaskDrive[];
extern const char kTaskWebIntent[];
extern const char kTaskApp[];

void UpdateFileHandlerUsageStats(Profile* profile, const std::string& task_id);

// Update the default file handler for the given sets of suffixes and MIME
// types.
void UpdateDefaultTask(Profile* profile,
                       const std::string& task_id,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types);

// Returns the task ID of the default task for the given |mime_type|/|suffix|
// combination. If it finds a MIME type match, then it prefers that over a
// suffix match. If it a default can't be found, then it returns the empty
// string.
std::string GetDefaultTaskIdFromPrefs(Profile* profile,
                                      const std::string& mime_type,
                                      const std::string& suffix);

// Gets read-write file access permission flags.
int GetReadWritePermissions();
// Gets read-only file access permission flags.
int GetReadOnlyPermissions();

// Generates task id for the action specified by the extension. The |task_type|
// must be one of kTaskFile, kTaskDrive, or kTaskWebIntent.
std::string MakeTaskID(const std::string& extension_id,
                       const std::string& task_type,
                       const std::string& action_id);

// Extracts action, type and extension id bound to the file task. Either
// |target_extension_id| or |action_id| are allowed to be NULL if caller isn't
// interested in those values.  Returns false on failure to parse.
bool CrackTaskID(const std::string& task_id,
                 std::string* target_extension_id,
                 std::string* task_type,
                 std::string* action_id);

// This generates a list of default tasks (tasks set as default by the user in
// prefs) from the |common_tasks|.
void FindDefaultTasks(Profile* profile,
                      const std::vector<FilePath>& files_list,
                      const std::set<const FileBrowserHandler*>& common_tasks,
                      std::set<const FileBrowserHandler*>* default_tasks);

// This generates list of tasks common for all files in |file_list|.
bool FindCommonTasks(Profile* profile,
                     const std::vector<GURL>& files_list,
                     std::set<const FileBrowserHandler*>* common_tasks);

// Finds a task for a file whose URL is |url| and whose path is |path|.
// Returns default task if one is defined (The default task is the task that is
// assigned to file browser task button by default). If default task is not
// found, tries to match the url with one of the builtin tasks.
bool GetTaskForURLAndPath(Profile* profile,
                          const GURL& url,
                          const FilePath& path,
                          const FileBrowserHandler** handler);

// Used for returning success or failure from task executions.
typedef base::Callback<void(bool)> FileTaskFinishedCallback;

// Helper class for executing file browser file action.
class FileTaskExecutor : public base::RefCountedThreadSafe<FileTaskExecutor> {
 public:
  // Creates the appropriate FileTaskExecutor for the given |extension_id|.
  static FileTaskExecutor* Create(Profile* profile,
                                  const GURL& source_url,
                                  const std::string& file_browser_id,
                                  int32 tab_id,
                                  const std::string& extension_id,
                                  const std::string& task_type,
                                  const std::string& action_id);

  // Same as ExecuteAndNotify, but no notification is performed.
  virtual bool Execute(const std::vector<fileapi::FileSystemURL>& file_urls);

  // Initiates execution of file handler task for each element of |file_urls|.
  // Return |false| if the execution cannot be initiated. Otherwise returns
  // |true| and then eventually calls |done| when all the files have
  // been handled. If there is an error during processing the list of files, the
  // caller will be informed of the failure via |done|, and the rest of
  // the files will not be processed.
  virtual bool ExecuteAndNotify(
      const std::vector<fileapi::FileSystemURL>& file_urls,
      const FileTaskFinishedCallback& done) = 0;

 protected:
  explicit FileTaskExecutor(Profile* profile,
                            const GURL& source_url,
                            const std::string& file_browser_id,
                            const std::string& extension_id);
  virtual ~FileTaskExecutor();

  // Checks if the file browser extension had file access permissions for the
  // list of files.
  bool FileBrowserHasAccessPermissionForFiles(
      const std::vector<fileapi::FileSystemURL>& files);

  // Returns the profile that this task was created with.
  Profile* profile() { return profile_; }

  // Returns a browser to use for the current browser.
  Browser* GetBrowser() const;

  // Returns the extension for this profile.
  const extensions::Extension* GetExtension();

  // Returns the extension ID set for this FileTaskExecutor.
  const std::string& extension_id() { return extension_id_; }

 private:
  friend class base::RefCountedThreadSafe<FileTaskExecutor>;

  Profile* profile_;
  const GURL source_url_;
  const std::string file_browser_id_;
  const std::string extension_id_;
};

}  // namespace file_handler_util

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_HANDLER_UTIL_H_
