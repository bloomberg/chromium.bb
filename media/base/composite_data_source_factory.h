// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_COMPOSITE_DATA_SOURCE_FACTORY_H_
#define MEDIA_BASE_COMPOSITE_DATA_SOURCE_FACTORY_H_

#include <list>
#include <set>

#include "base/synchronization/lock.h"
#include "media/base/async_filter_factory_base.h"

namespace media {

class CompositeDataSourceFactory : public AsyncDataSourceFactoryBase {
 public:
  CompositeDataSourceFactory();
  virtual ~CompositeDataSourceFactory();

  // Add factory to this composite. Ownership is transferred here.
  void AddFactory(DataSourceFactory* factory);

  // DataSourceFactory method.
  virtual DataSourceFactory* Clone() const;

 protected:
  // AsyncDataSourceFactoryBase methods.
  virtual bool AllowRequests() const;
  virtual AsyncDataSourceFactoryBase::BuildRequest* CreateRequest(
      const std::string& url, BuildCallback* callback);

 private:
  class BuildRequest;

  typedef std::list<DataSourceFactory*> FactoryList;
  FactoryList factories_;

  DISALLOW_COPY_AND_ASSIGN(CompositeDataSourceFactory);
};

}  // namespace media

#endif  // MEDIA_BASE_COMPOSITE_DATA_SOURCE_FACTORY_H_
