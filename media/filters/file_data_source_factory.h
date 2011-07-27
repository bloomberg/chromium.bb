// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FILE_DATA_SOURCE_FACTORY_H_
#define MEDIA_FILTERS_FILE_DATA_SOURCE_FACTORY_H_

#include "media/base/filter_factories.h"

namespace media {

class FileDataSourceFactory : public DataSourceFactory {
 public:
  FileDataSourceFactory();
  virtual ~FileDataSourceFactory();

  // DataSourceFactory methods.
  virtual void Build(const std::string& url, BuildCallback* callback);
  virtual DataSourceFactory* Clone() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileDataSourceFactory);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FILE_DATA_SOURCE_FACTORY_H_
