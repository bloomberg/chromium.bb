// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_HANDLER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_HANDLER_UTIL_H_
#pragma once

#include "base/platform_file.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/url_pattern_set.h"

class Browser;
class FileBrowserHandler;
class GURL;
class Profile;

namespace file_handler_util {

int GetReadWritePermissions();

std::string MakeTaskID(const std::string& extension_id,
                       const std::string& action_id);

bool CrackTaskID(const std::string& task_id,
                 std::string* target_extension_id,
                 std::string* action_id);

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

bool FindCommonTasks(Profile* profile,
                     const std::vector<GURL>& files_list,
                     LastUsedHandlerList* named_action_list);

bool GetDefaultTask(Profile* profile,
                    const GURL& url,
                    const FileBrowserHandler** handler);

class FileTaskExecutor : public base::RefCountedThreadSafe<FileTaskExecutor> {
 public:

  FileTaskExecutor(Profile* profile,
                   const GURL source_url,
                   const std::string& extension_id,
                   const std::string& action_id);

  virtual ~FileTaskExecutor();

  // Initiates execution of file handler task for each element of |file_urls|.
  // Return |false| if the execution cannot be initiated.
  // Otherwise returns |true| and then eventually calls |Done|.
  bool Execute(const std::vector<GURL>& file_urls);

 protected:
   virtual Browser* browser() = 0;
   virtual void Done(bool success) = 0;

 private:
  struct FileDefinition {
    GURL target_file_url;
    FilePath virtual_path;
    bool is_directory;
  };
  typedef std::vector<FileDefinition> FileDefinitionList;
  class ExecuteTasksFileSystemCallbackDispatcher;
  void RequestFileEntryOnFileThread(
      const GURL& handler_base_url,
      const scoped_refptr<const Extension>& handler,
      int handler_pid,
      const std::vector<GURL>& file_urls);
  void RespondFailedOnUIThread(base::PlatformFileError error_code);
  void ExecuteFileActionsOnUIThread(const std::string& file_system_name,
                                    const GURL& file_system_root,
                                    const FileDefinitionList& file_list);
  void ExecuteFailedOnUIThread();

  Profile* profile_;
  const GURL source_url_;
  const std::string extension_id_;
  const std::string action_id_;
};

}  // namespace file_handler_util

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_HANDLER_UTIL_H_
