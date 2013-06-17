// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PICASA_DATA_PROVIDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PICASA_DATA_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"

namespace picasa {

struct AlbumInfo;

typedef std::map<std::string, AlbumInfo> AlbumMap;

// Created and owned by ImportedMediaGalleryRegistryTaskRunnerValues
class PicasaDataProvider {
 public:
  explicit PicasaDataProvider(const base::FilePath& database_path);
  virtual ~PicasaDataProvider();

  // Ask the data provider to refresh the data if necessary. |ready_callback|
  // will be called when the data is up to date
  void RefreshData(const base::Closure& ready_callback);

  scoped_ptr<AlbumMap> GetAlbums();
  scoped_ptr<AlbumMap> GetFolders();
  // TODO(tommycli): Implement album contents. GetAlbumContents(...)

 protected:
  void InitializeWith(const std::vector<AlbumInfo>& albums,
                      const std::vector<AlbumInfo>& folder);

 private:
  friend class PicasaFileUtilTest;

  static std::string DateToPathString(const base::Time& time);
  static void UniquifyNames(const std::vector<AlbumInfo>& info_list,
                            AlbumMap* result_map);

  // This can be overidden by a test version of this class.
  virtual bool ReadData();

  AlbumMap album_map_;
  AlbumMap folder_map_;

  base::FilePath database_path_;
  bool needs_refresh_;

  DISALLOW_COPY_AND_ASSIGN(PicasaDataProvider);
};

}  // namespace picasa

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PICASA_DATA_PROVIDER_H_
