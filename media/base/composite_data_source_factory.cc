// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/composite_data_source_factory.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"

namespace media {

class CompositeDataSourceFactory::BuildRequest
    : public AsyncDataSourceFactoryBase::BuildRequest {
 public:
  BuildRequest(const std::string& url, BuildCallback* callback,
               const FactoryList& factories);
  ~BuildRequest();

 protected:
  // AsyncDataSourceFactoryBase::BuildRequest method.
  virtual void DoStart();

 private:
  void CallNextFactory();
  void OnBuildDone(PipelineStatus status, DataSource* data_source);

  FactoryList factories_;
};

CompositeDataSourceFactory::CompositeDataSourceFactory() {}

CompositeDataSourceFactory::~CompositeDataSourceFactory() {
  STLDeleteElements(&factories_);
}

void CompositeDataSourceFactory::AddFactory(DataSourceFactory* factory) {
  DCHECK(factory);
  factories_.push_back(factory);
}

DataSourceFactory* CompositeDataSourceFactory::Clone() const {
  CompositeDataSourceFactory* new_factory = new CompositeDataSourceFactory();

  for (FactoryList::const_iterator itr = factories_.begin();
       itr != factories_.end();
       ++itr) {
    new_factory->AddFactory((*itr)->Clone());
  }

  return new_factory;
}

bool CompositeDataSourceFactory::AllowRequests() const {
  return !factories_.empty();
}

AsyncDataSourceFactoryBase::BuildRequest*
CompositeDataSourceFactory::CreateRequest(const std::string& url,
                                          BuildCallback* callback) {
  return new BuildRequest(url, callback, factories_);
}

CompositeDataSourceFactory::BuildRequest::BuildRequest(
    const std::string& url,
    BuildCallback* callback,
    const FactoryList& factories)
    : AsyncDataSourceFactoryBase::BuildRequest(url, callback),
      factories_(factories){
  DCHECK(!factories.empty());
}

CompositeDataSourceFactory::BuildRequest::~BuildRequest() {}

void CompositeDataSourceFactory::BuildRequest::DoStart() {
  CallNextFactory();
}

void CompositeDataSourceFactory::BuildRequest::CallNextFactory() {
  DCHECK(!factories_.empty());

  DataSourceFactory* factory = factories_.front();
  factories_.pop_front();

  factory->Build(url(), NewCallback(this, &BuildRequest::OnBuildDone));
}

void CompositeDataSourceFactory::BuildRequest::OnBuildDone(
    PipelineStatus status,
    DataSource* data_source) {

  if (status == PIPELINE_OK) {
    DCHECK(data_source);
    RequestComplete(status, data_source);
    return;
  }

  DCHECK(!data_source);
  if ((status == DATASOURCE_ERROR_URL_NOT_SUPPORTED) && !factories_.empty()) {
    CallNextFactory();
    return;
  }

  RequestComplete(status, data_source);
}

}  // namespace media
