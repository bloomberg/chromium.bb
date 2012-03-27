// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_GDATA_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_GDATA_SOURCE_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"

class Profile;

namespace gdata {

class GDataSystemService;
struct FileReadContext;

// GDataSource is the gateway between network-level chrome:
// requests for gdata resources and GDataFileSyte. It will expose content
// URLs formatted as chrome://gdata/<resource-id>/<file_name>.
class GDataSource : public ChromeURLDataManager::DataSource {
 public:
  explicit GDataSource(Profile* profile);

  // ChromeURLDataManager::DataSource overrides:
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE;

 private:
  // Helper callback for handling async responses from
  // GDataFileSystem::GetFileForResourceId().
  void OnGetFileForResourceId(int request_id,
                              base::PlatformFileError error,
                              const FilePath& file_path,
                              const std::string& mime_type,
                              GDataFileType file_type);

  // Starts reading file content.
  void StartFileRead(int request_id,
                     const FilePath& file_path,
                     int64 *file_size);

  // Helper callback for handling async responses from FileStream::Open().
  void OnFileOpen(scoped_refptr<FileReadContext> context,
                  int open_result);

    // Helper callback for handling async responses from FileStream::Read().
  void OnFileRead(scoped_refptr<FileReadContext> context,
                  int bytes_read);

  virtual ~GDataSource();

  Profile* profile_;
  gdata::GDataSystemService* system_service_;

  DISALLOW_COPY_AND_ASSIGN(GDataSource);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_GDATA_SOURCE_H_
