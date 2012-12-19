// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

class FilePath;
class Profile;

class DevToolsFileHelper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void FileSavedAs(const std::string& url) = 0;
    virtual void AppendedTo(const std::string& url) = 0;
  };

  DevToolsFileHelper(Profile* profile, Delegate* delegate);
  ~DevToolsFileHelper();

  // Saves |content| to the file and associates its path with given |url|.
  // If client is calling this method with given |url| for the first time
  // or |save_as| is true, confirmation dialog is shown to the user.
  void Save(const std::string& url,
            const std::string& content,
            bool save_as);

  // Append |content| to the file that has been associated with given |url|.
  // The |url| can be associated with a file via calling Save method.
  // If the Save method has not been called for this |url|, then
  // Append method does nothing.
  void Append(const std::string& url, const std::string& content);

  void FileSelected(const std::string& url,
                    const FilePath& path,
                    const std::string& content);

 private:
  static void WriteFile(const FilePath& path, const std::string& content);
  static void AppendToFile(const FilePath& path, const std::string& content);

  class SaveAsDialog;

  Profile* profile_;
  Delegate* delegate_;
  scoped_refptr<SaveAsDialog> save_as_dialog_;
  typedef std::map<std::string, FilePath> PathsMap;
  PathsMap saved_files_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsFileHelper);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_HELPER_H_
