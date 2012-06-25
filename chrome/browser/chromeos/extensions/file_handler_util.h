// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_HANDLER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_HANDLER_UTIL_H_
#pragma once

#include <vector>

#include "base/platform_file.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/url_pattern_set.h"

class Browser;
class ExtensionHost;
class FileBrowserHandler;
class GURL;
class Profile;

namespace file_handler_util {

void UpdateFileHandlerUsageStats(Profile* profile, const std::string& task_id);

// Gets read-write file access permission flags.
int GetReadWritePermissions();
// Gets read-only file access permission flags.
int GetReadOnlyPermissions();

// Generates file task id for the file action specified by the extension.
std::string MakeTaskID(const std::string& extension_id,
                       const std::string& action_id);

// Make a task id specific to drive apps instead of extensions.
std::string MakeDriveTaskID(const std::string& app_id,
                            const std::string& action_id);

// Extracts action and extension id bound to the file task.
bool CrackTaskID(const std::string& task_id,
                 std::string* target_extension_id,
                 std::string* action_id);

// Struct that keeps track of when a handler was `last used.
// It is used to determine default file action for a file.
struct LastUsedHandler {
  LastUsedHandler(int t, const FileBrowserHandler* h, URLPatternSet p)
      : timestamp(t),
        handler(h),
        patterns(p) {
  }

  int timestamp;
  const FileBrowserHandler* handler;
  URLPatternSet patterns;
};

typedef std::vector<LastUsedHandler> LastUsedHandlerList;

// Generates list of tasks common for all files in |file_list|.
// The resulting list is sorted by last usage time.
bool FindCommonTasks(Profile* profile,
                     const std::vector<GURL>& files_list,
                     LastUsedHandlerList* named_action_list);

// Find the default task for a file whose url is |url|. (The default task is the
// task that is assigned to file browser task button by default).
bool GetDefaultTask(Profile* profile,
                    const GURL& url,
                    const FileBrowserHandler** handler);

// Used for returning success or failure from task executions.
typedef base::Callback<void(bool)> FileTaskFinishedCallback;

// Helper class for executing file browser file action.
class FileTaskExecutor : public base::RefCountedThreadSafe<FileTaskExecutor> {
 public:
  static const char kDriveTaskExtensionPrefix[];
  static const size_t kDriveTaskExtensionPrefixLength;

  // Creates the appropriate FileTaskExecutor for the given |extension_id|.
  static FileTaskExecutor* Create(Profile* profile,
                                  const GURL source_url,
                                  const std::string& extension_id,
                                  const std::string& action_id);

  // Same as ExecuteAndNotify, but no notification is performed.
  virtual bool Execute(const std::vector<GURL>& file_urls);

  // Initiates execution of file handler task for each element of |file_urls|.
  // Return |false| if the execution cannot be initiated. Otherwise returns
  // |true| and then eventually calls |done| when all the files have
  // been handled. If there is an error during processing the list of files, the
  // caller will be informed of the failure via |done|, and the rest of
  // the files will not be processed.
  virtual bool ExecuteAndNotify(const std::vector<GURL>& file_urls,
                                const FileTaskFinishedCallback& done) = 0;

 protected:
  explicit FileTaskExecutor(Profile* profile);
  virtual ~FileTaskExecutor();

  // Returns the profile that this task was created with.
  Profile* profile() { return profile_; }

  // Returns a browser to use for the current browser.
  Browser* GetBrowser() const;
 private:
  friend class base::RefCountedThreadSafe<FileTaskExecutor>;

  Profile* profile_;
};

}  // namespace file_handler_util

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_HANDLER_UTIL_H_
