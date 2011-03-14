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
                                  BuildCallback* callback) {
  DCHECK(callback);

  if (url.empty()) {
    callback->Run(media::PIPELINE_ERROR_URL_NOT_FOUND,
                  static_cast<media::DataSource*>(NULL));
    delete callback;
    return;
  }

  scoped_refptr<FileDataSource> file_data_source = new FileDataSource();

  PipelineError error = file_data_source->Initialize(url);
  DataSource* data_source =
      (error == PIPELINE_OK) ? file_data_source.get() : NULL;
  callback->Run(error, data_source);
  delete callback;
}

DataSourceFactory* FileDataSourceFactory::Clone() const {
  return new FileDataSourceFactory();
}

}  // namespace media
