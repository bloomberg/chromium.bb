// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/file_data_source_factory.h"

#include "base/logging.h"
#include "media/filters/file_data_source.h"

namespace media {

FileDataSourceFactory::FileDataSourceFactory() {}

FileDataSourceFactory::~FileDataSourceFactory() {}

void FileDataSourceFactory::Build(const std::string& url,
                                  const BuildCallback& callback) {
  DCHECK(!callback.is_null());

  if (url.empty()) {
    callback.Run(media::PIPELINE_ERROR_URL_NOT_FOUND,
                 static_cast<media::DataSource*>(NULL));
    return;
  }

  scoped_refptr<FileDataSource> file_data_source = new FileDataSource();

  PipelineStatus status = file_data_source->Initialize(url);
  DataSource* data_source =
      (status == PIPELINE_OK) ? file_data_source.get() : NULL;
  callback.Run(status, data_source);
}

DataSourceFactory* FileDataSourceFactory::Clone() const {
  return new FileDataSourceFactory();
}

}  // namespace media
