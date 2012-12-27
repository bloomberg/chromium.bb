// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

class FilePath;
class Profile;

class DevToolsFileHelper {
 public:
  explicit DevToolsFileHelper(Profile* profile);
  ~DevToolsFileHelper();

  typedef base::Callback<void(void)> SaveCallback;
  typedef base::Callback<void(void)> AppendCallback;

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

 private:
  void SaveAsFileSelected(const std::string& url,
                    const std::string& content,
                    const SaveCallback& callback,
                    const FilePath& path);
  void SaveAsFileSelectionCanceled();

  Profile* profile_;
  base::WeakPtrFactory<DevToolsFileHelper> weak_factory_;
  typedef std::map<std::string, FilePath> PathsMap;
  PathsMap saved_files_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsFileHelper);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_
