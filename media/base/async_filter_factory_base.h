// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ASYNC_FILTER_FACTORY_BASE_H_
#define MEDIA_BASE_ASYNC_FILTER_FACTORY_BASE_H_

#include <set>

#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/filter_factories.h"

namespace media {

// This is a helper base class for DataSourceFactory implementation that
// actually require asynchronous operations to build a data source. It
// provides a common framework for dealing with an asychronous build.
// Factories are expected to derive from this object and implement
// AllowRequests(), CreateRequest(), and an custom implementation that derives
// from BuildRequest.
//
// AllowRequests() just checks to see if the factory is in a state where it can
// accept Build() requests. If it returns false, this base class will signal an
// error through the BuildCallback and not call any of the other code. If
// AllowRequests() returns true then this class will continue with the build
// process by calling CreateRequest().
//
// CreateRequest() allows the derived implementation to create an instance of
// their custom BuildRequest implementation that is specific to its asynchronous
// build process. The custom BuildRequest should contain all of the state that
// needs to be maintained for a particular build request. This state could
// consist of partially initialized objects that require asynchronous operations
// to complete before the build completes. This implementation MUST derive from
// AsyncDataSourceFactoryBase::BuildRequest.
//
// Once CreateRequest() returns a BuildRequest implementation, this class adds
// the object to its request list and then calls Start() on the BuildRequest
// instance. BuildRequest::Start() manages storing the |done_callback| passed to
// it and then call DoStart() on the derived object. DoStart() is where this
// framework expects any neccesary asynchronous operations to be initiated.
//
// Once all asynchronous operations are completed and a fully initialized
// DataSource object has been created, the BuildRequest instance should call
// the RequestComplete() method. This call signals the end of the request and
// the BuildRequest should be in a state where it can be deleted from inside
// this call. If an error occurs during the build process, RequestComplete()
// can also be called to signal the error.
class AsyncDataSourceFactoryBase : public DataSourceFactory {
 public:
  AsyncDataSourceFactoryBase();
  virtual ~AsyncDataSourceFactoryBase();

  // DataSourceFactory method.
  // Derived classes should not overload this Build() method. AllowRequests() &
  // CreateRequest() should be implemented instead.
  virtual void Build(const std::string& url, BuildCallback* callback);

  // DataSourceFactory method.
  // Clone() must be implemented by derived classes.
  // NOTE: Nothing in this base class needs to be cloned because this class
  // only keeps track of pending requests, which are not part of the cloning
  // process.
  virtual DataSourceFactory* Clone() const = 0;

 protected:
  class BuildRequest {
   public:
    BuildRequest(const std::string& url, BuildCallback* callback);
    virtual ~BuildRequest();

    typedef Callback1<BuildRequest*>::Type RequestDoneCallback;
    // Starts the build request.
    void Start(RequestDoneCallback* done_callback);

    // Derived objects call this method to indicate that the build request
    // has completed. If the build was successful |status| should be set to
    // PIPELINE_OK and |data_source| should contain the DataSource object
    // that was built by this request. Ownership of |data_source| is being
    // passed in this call. If an error occurs during the build process, this
    // method should be called with |status| set to an appropriate status code
    // and |data_source| set to NULL.
    //
    // The derived object should be in a state where it can be deleted from
    // within this call. This class as well AsyncDataSourceFactoryBase use this
    // method to cleanup state associated with this request.
    void RequestComplete(media::PipelineStatus status, DataSource* data_source);

   protected:
    // Implemented by the derived object to start the build. Called by Start().
    virtual void DoStart() = 0;

    // Gets the requested URL.
    const std::string& url() const;

   private:
    std::string url_;
    scoped_ptr<BuildCallback> callback_;
    scoped_ptr<RequestDoneCallback> done_callback_;

    DISALLOW_COPY_AND_ASSIGN(BuildRequest);
  };

  // Implemented by derived class. Called by Build() to check if the
  // factory is in a state where it can accept requests.
  virtual bool AllowRequests() const = 0;

  // Implemented by derived class. Called by Build() to allow derived objects
  // to create their own custom BuildRequest implementations.
  virtual BuildRequest* CreateRequest(const std::string& url,
                                      BuildCallback* callback) = 0;

 private:
  void RunAndDestroyCallback(PipelineStatus status,
                             BuildCallback* callback) const;

  typedef Callback1<BuildRequest*>::Type RequestDoneCallback;
  void BuildRequestDone(BuildRequest* request);

  base::Lock lock_;
  typedef std::set<BuildRequest*> RequestSet;
  RequestSet outstanding_requests_;

  DISALLOW_COPY_AND_ASSIGN(AsyncDataSourceFactoryBase);
};

}  // namespace media

#endif  // MEDIA_BASE_ASYNC_FILTER_FACTORY_BASE_H_
