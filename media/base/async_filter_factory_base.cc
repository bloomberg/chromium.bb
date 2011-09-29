// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/async_filter_factory_base.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"

namespace media {

AsyncDataSourceFactoryBase::AsyncDataSourceFactoryBase() {}

AsyncDataSourceFactoryBase::~AsyncDataSourceFactoryBase() {
  base::AutoLock auto_lock(lock_);
  STLDeleteElements(&outstanding_requests_);
}

void AsyncDataSourceFactoryBase::Build(const std::string& url,
                                       const BuildCallback& callback) {
  DCHECK(!callback.is_null());
  BuildRequest* request = NULL;
  {
    base::AutoLock auto_lock(lock_);

    if (url.empty()) {
      ReportError(PIPELINE_ERROR_URL_NOT_FOUND, callback);
      return;
    }

    if (!AllowRequests()) {
      ReportError(DATASOURCE_ERROR_URL_NOT_SUPPORTED, callback);
      return;
    }

    request = CreateRequest(url, callback);

    if (!request) {
      ReportError(DATASOURCE_ERROR_URL_NOT_SUPPORTED, callback);
      return;
    }

    outstanding_requests_.insert(request);
  }

  request->Start(base::Bind(
      &AsyncDataSourceFactoryBase::BuildRequestDone, base::Unretained(this)));
}

void AsyncDataSourceFactoryBase::ReportError(
    PipelineStatus error,
    const BuildCallback& callback) const {
  DCHECK_NE(error, PIPELINE_OK);
  DCHECK(!callback.is_null());

  callback.Run(error, static_cast<DataSource*>(NULL));
}

void AsyncDataSourceFactoryBase::BuildRequestDone(BuildRequest* request) {
  base::AutoLock auto_lock(lock_);
  outstanding_requests_.erase(request);
  delete request;
}

AsyncDataSourceFactoryBase::BuildRequest::BuildRequest(
    const std::string& url, const BuildCallback& callback)
    : url_(url),
      callback_(callback) {
}

AsyncDataSourceFactoryBase::BuildRequest::~BuildRequest() {}

void AsyncDataSourceFactoryBase::BuildRequest::Start(
    const RequestDoneCallback& done_callback) {
  DCHECK(!done_callback.is_null());
  DCHECK(done_callback_.is_null());

  done_callback_ = done_callback;
  DoStart();
  // Don't do anything after this line since the object could
  // have been deleted at this point if the request was completed
  // inside the call.
}

void AsyncDataSourceFactoryBase::BuildRequest::RequestComplete(
    PipelineStatus status,
    DataSource* data_source) {
  DCHECK(!callback_.is_null());
  DCHECK(!done_callback_.is_null());

  // Transfer ownership to local variables just in case the
  // request object gets deleted by one of the callbacks.
  RequestDoneCallback done_callback;
  std::swap(done_callback, done_callback_);
  BuildCallback callback;
  std::swap(callback, callback_);

  // Notify factory that this request has completed. We do this before
  // calling |callback| so the factory doesn't consider this request
  // pending if |callback| happens to destroy the factory.
  //
  // NOTE: This BuildRequest object is destroyed inside this callback so
  //       no modifications should be made to this object after this call.
  done_callback.Run(this);

  callback.Run(status, data_source);
}

const std::string& AsyncDataSourceFactoryBase::BuildRequest::url() const {
  return url_;
}

}  // namespace media
