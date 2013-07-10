// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa/picasa_data_provider.h"

#include <utility>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/fileapi/safe_picasa_album_table_reader.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using chrome::MediaFileSystemBackend;

namespace picasa {

PicasaDataProvider::PicasaDataProvider(const base::FilePath& database_path)
    : database_path_(database_path),
      needs_refresh_(true),
      weak_factory_(this) {
}

PicasaDataProvider::~PicasaDataProvider() {}

void PicasaDataProvider::RefreshData(const base::Closure& ready_callback) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  // TODO(tommycli): Need to watch the database_path_ folder and handle
  // rereading the data when it changes.
  if (!needs_refresh_) {
    ready_callback.Run();
    return;
  }

  needs_refresh_ = false;
  album_table_reader_ = new SafePicasaAlbumTableReader(
      AlbumTableFiles(database_path_),
      base::Bind(&PicasaDataProvider::OnDataRefreshed,
                 weak_factory_.GetWeakPtr(),
                 ready_callback));
  album_table_reader_->Start();
}

scoped_ptr<AlbumMap> PicasaDataProvider::GetFolders() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  return make_scoped_ptr(new AlbumMap(folder_map_));
}

scoped_ptr<AlbumMap> PicasaDataProvider::GetAlbums() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  return make_scoped_ptr(new AlbumMap(album_map_));
}

void PicasaDataProvider::OnDataRefreshed(
    const base::Closure& ready_callback,
    bool parse_success,
    const std::vector<AlbumInfo>& albums,
    const std::vector<AlbumInfo>& folders) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  if (parse_success) {
    UniquifyNames(albums, &album_map_);
    UniquifyNames(folders, &folder_map_);
  }
  ready_callback.Run();
}

// static
std::string PicasaDataProvider::DateToPathString(const base::Time& time) {
  base::Time::Exploded exploded_time;
  time.LocalExplode(&exploded_time);

  // TODO(tommycli): Investigate better localization and persisting which locale
  // we use to generate these unique names.
  return base::StringPrintf("%04d-%02d-%02d", exploded_time.year,
                            exploded_time.month, exploded_time.day_of_month);
}

// static
void PicasaDataProvider::UniquifyNames(const std::vector<AlbumInfo>& info_list,
                                       AlbumMap* result_map) {
  // TODO(tommycli): We should persist the uniquified names.
  std::vector<std::string> desired_names;

  std::map<std::string, int> total_counts;
  std::map<std::string, int> current_counts;

  for (std::vector<AlbumInfo>::const_iterator it = info_list.begin();
       it != info_list.end(); ++it) {
    std::string desired_name =
        it->name + " " + DateToPathString(it->timestamp);
    desired_names.push_back(desired_name);
    ++total_counts[desired_name];
  }

  for (unsigned int i = 0; i < info_list.size(); i++) {
    std::string name = desired_names[i];

    if (total_counts[name] != 1) {
      name = base::StringPrintf("%s (%d)", name.c_str(),
                                ++current_counts[name]);
    }

    result_map->insert(std::pair<std::string, AlbumInfo>(name, info_list[i]));
  }
}

}  // namespace picasa
