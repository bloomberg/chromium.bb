// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/async_filter_factory_base.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"

namespace media {

AsyncDataSourceFactoryBase::AsyncDataSourceFactoryBase() {}

AsyncDataSourceFactoryBase::~AsyncDataSourceFactoryBase() {
  base::AutoLock auto_lock(lock_);
  STLDeleteElements(&outstanding_requests_);
}

void AsyncDataSourceFactoryBase::Build(const std::string& url,
                                       BuildCallback* callback) {
  DCHECK(callback);
  BuildRequest* request = NULL;
  {
    base::AutoLock auto_lock(lock_);

    if (url.empty()) {
      RunAndDestroyCallback(PIPELINE_ERROR_URL_NOT_FOUND, callback);
      return;
    }

    if (!AllowRequests()) {
      RunAndDestroyCallback(DATASOURCE_ERROR_URL_NOT_SUPPORTED, callback);
      return;
    }

    request = CreateRequest(url, callback);

    if (!request) {
      RunAndDestroyCallback(DATASOURCE_ERROR_URL_NOT_SUPPORTED, callback);
      return;
    }

    outstanding_requests_.insert(request);
  }

  request->Start(NewCallback(this,
                             &AsyncDataSourceFactoryBase::BuildRequestDone));
}

void AsyncDataSourceFactoryBase::RunAndDestroyCallback(
    PipelineError error,
    BuildCallback* callback) const {
  DCHECK_NE(error, PIPELINE_OK);
  DCHECK(callback);

  callback->Run(error, static_cast<DataSource*>(NULL));
  delete callback;
}

void AsyncDataSourceFactoryBase::BuildRequestDone(BuildRequest* request) {
  base::AutoLock auto_lock(lock_);
  outstanding_requests_.erase(request);
  delete request;
}

AsyncDataSourceFactoryBase::BuildRequest::BuildRequest(const std::string& url,
                                                       BuildCallback* callback)
    : url_(url),
      callback_(callback) {
}

AsyncDataSourceFactoryBase::BuildRequest::~BuildRequest() {}

void AsyncDataSourceFactoryBase::BuildRequest::Start(
    RequestDoneCallback* done_callback) {
  DCHECK(done_callback);
  DCHECK(!done_callback_.get());

  done_callback_.reset(done_callback);
  DoStart();
  // Don't do anything after this line since the object could
  // have been deleted at this point if the request was completed
  // inside the call.
}

void AsyncDataSourceFactoryBase::BuildRequest::RequestComplete(
    PipelineError error,
    DataSource* data_source) {
  DCHECK(callback_.get());
  DCHECK(done_callback_.get());

  // Transfer ownership to local variables just in case the
  // request object gets deleted by one of the callbacks.
  scoped_ptr<RequestDoneCallback> done_callback(done_callback_.release());
  scoped_ptr<BuildCallback> callback(callback_.release());

  // Notify factory that this request has completed. We do this before
  // calling |callback| so the factory doesn't consider this request
  // pending if |callback| happens to destroy the factory.
  //
  // NOTE: This BuildRequest object is destroyed inside this callback so
  //       no modifications should be made to this object after this call.
  done_callback->Run(this);

  callback->Run(error, data_source);
}

const std::string& AsyncDataSourceFactoryBase::BuildRequest::url() const {
  return url_;
}

}  // namespace media
