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
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/common/media_galleries/picasa_types.h"

namespace picasa {

class SafePicasaAlbumTableReader;

// Created and owned by ImportedMediaGalleryRegistryTaskRunnerValues
class PicasaDataProvider {
 public:
  explicit PicasaDataProvider(const base::FilePath& database_path);
  virtual ~PicasaDataProvider();

  // Ask the data provider to refresh the data if necessary. |ready_callback|
  // will be called when the data is up to date
  // TODO(tommycli): Investigate having the callback return a bool indicating
  // success or failure - and handling it intelligently in PicasaFileUtil.
  virtual void RefreshData(const base::Closure& ready_callback);

  scoped_ptr<AlbumMap> GetAlbums();
  scoped_ptr<AlbumMap> GetFolders();
  // TODO(tommycli): Implement album contents. GetAlbumContents(...)

 protected:
  // Protected for test class usage.
  void OnDataRefreshed(const base::Closure& ready_callback,
                       bool parse_success, const std::vector<AlbumInfo>& albums,
                       const std::vector<AlbumInfo>& folder);

 private:
  friend class PicasaFileUtilTest;
  friend class TestPicasaDataProvider;

  static std::string DateToPathString(const base::Time& time);
  static void UniquifyNames(const std::vector<AlbumInfo>& info_list,
                            AlbumMap* result_map);

  AlbumMap album_map_;
  AlbumMap folder_map_;

  base::FilePath database_path_;
  bool needs_refresh_;

  scoped_refptr<SafePicasaAlbumTableReader> album_table_reader_;

  base::WeakPtrFactory<PicasaDataProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PicasaDataProvider);
};

}  // namespace picasa

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_PICASA_DATA_PROVIDER_H_
