// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/renderer_host/global_request_id.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/browser/renderer_host/resource_handler.h"
#include "chrome/browser/renderer_host/resource_queue.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestUrl[] = "data:text/plain,Hello World!";

class DummyResourceHandler : public ResourceHandler {
 public:
  DummyResourceHandler() {
  }

  bool OnUploadProgress(int request_id, uint64 position, uint64 size) {
    NOTREACHED();
    return true;
  }

  virtual bool OnRequestRedirected(int request_id, const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) {
    NOTREACHED();
    return true;
  }

  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response) {
    NOTREACHED();
    return true;
  }

  virtual bool OnWillStart(int request_id, const GURL& url, bool* defer) {
    NOTREACHED();
    return true;
  }

  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) {
    NOTREACHED();
    return true;
  }

  virtual bool OnReadCompleted(int request_id, int* bytes_read) {
    NOTREACHED();
    return true;
  }

  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) {
    NOTREACHED();
    return true;
  }

  virtual void OnRequestClosed() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyResourceHandler);
};

ResourceDispatcherHostRequestInfo* GetRequestInfo(int request_id) {
  return new ResourceDispatcherHostRequestInfo(
      new DummyResourceHandler(), ChildProcessInfo::RENDER_PROCESS, 0, 0,
      request_id, ResourceType::MAIN_FRAME, 0, false, false, false, -1, -1);
}

void InitializeQueue(ResourceQueue* queue, ResourceQueueDelegate* delegate) {
  ResourceQueue::DelegateSet delegate_set;
  delegate_set.insert(delegate);
  queue->Initialize(delegate_set);
}

void InitializeQueue(ResourceQueue* queue,
                     ResourceQueueDelegate* delegate1,
                     ResourceQueueDelegate* delegate2) {
  ResourceQueue::DelegateSet delegate_set;
  delegate_set.insert(delegate1);
  delegate_set.insert(delegate2);
  queue->Initialize(delegate_set);
}

class NeverDelayingDelegate : public ResourceQueueDelegate {
 public:
  NeverDelayingDelegate() {
  }

  virtual bool ShouldDelayRequest(
      net::URLRequest* request,
      const ResourceDispatcherHostRequestInfo& request_info,
      const GlobalRequestID& request_id) {
    return false;
  }

  virtual void WillShutdownResourceQueue() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NeverDelayingDelegate);
};

class AlwaysDelayingDelegate : public ResourceQueueDelegate {
 public:
  explicit AlwaysDelayingDelegate(ResourceQueue* resource_queue)
      : resource_queue_(resource_queue) {
  }

  virtual bool ShouldDelayRequest(
      net::URLRequest* request,
      const ResourceDispatcherHostRequestInfo& request_info,
      const GlobalRequestID& request_id) {
    delayed_requests_.push_back(request_id);
    return true;
  }

  virtual void WillShutdownResourceQueue() {
    resource_queue_ = NULL;
  }

  void StartDelayedRequests() {
    if (!resource_queue_)
      return;

    for (RequestList::iterator i = delayed_requests_.begin();
         i != delayed_requests_.end(); ++i) {
      resource_queue_->StartDelayedRequest(this, *i);
    }
  }

 private:
  typedef std::vector<GlobalRequestID> RequestList;

  ResourceQueue* resource_queue_;

  RequestList delayed_requests_;

  DISALLOW_COPY_AND_ASSIGN(AlwaysDelayingDelegate);
};

class ResourceQueueTest : public testing::Test,
                          public net::URLRequest::Delegate {
 public:
  ResourceQueueTest()
      : response_started_count_(0),
        message_loop_(MessageLoop::TYPE_IO),
        ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }

  virtual void OnResponseStarted(net::URLRequest* request) {
    response_started_count_++;
    // We're not going to do anything more with the request. Cancel it now
    // to avoid leaking net::URLRequestJob.
    request->Cancel();
  }

  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read) {
  }

 protected:
  int response_started_count_;

 private:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
};

TEST_F(ResourceQueueTest, Basic) {
  // Test the simplest lifycycle of ResourceQueue.
  ResourceQueue queue;
  queue.Initialize(ResourceQueue::DelegateSet());
  queue.Shutdown();
}

TEST_F(ResourceQueueTest, NeverDelayingDelegate) {
  ResourceQueue queue;

  NeverDelayingDelegate delegate;
  InitializeQueue(&queue, &delegate);

  net::URLRequest request(GURL(kTestUrl), this);
  scoped_ptr<ResourceDispatcherHostRequestInfo> request_info(GetRequestInfo(0));
  EXPECT_EQ(0, response_started_count_);
  queue.AddRequest(&request, *request_info.get());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, response_started_count_);

  queue.Shutdown();
}

TEST_F(ResourceQueueTest, AlwaysDelayingDelegate) {
  ResourceQueue queue;

  AlwaysDelayingDelegate delegate(&queue);
  InitializeQueue(&queue, &delegate);

  net::URLRequest request(GURL(kTestUrl), this);
  scoped_ptr<ResourceDispatcherHostRequestInfo> request_info(GetRequestInfo(0));
  EXPECT_EQ(0, response_started_count_);
  queue.AddRequest(&request, *request_info.get());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, response_started_count_);
  delegate.StartDelayedRequests();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, response_started_count_);

  queue.Shutdown();
}

TEST_F(ResourceQueueTest, AlwaysDelayingDelegateAfterShutdown) {
  ResourceQueue queue;

  AlwaysDelayingDelegate delegate(&queue);
  InitializeQueue(&queue, &delegate);

  net::URLRequest request(GURL(kTestUrl), this);
  scoped_ptr<ResourceDispatcherHostRequestInfo> request_info(GetRequestInfo(0));
  EXPECT_EQ(0, response_started_count_);
  queue.AddRequest(&request, *request_info.get());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, response_started_count_);

  queue.Shutdown();

  delegate.StartDelayedRequests();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, response_started_count_);
}

TEST_F(ResourceQueueTest, TwoDelegates) {
  ResourceQueue queue;

  AlwaysDelayingDelegate always_delaying_delegate(&queue);
  NeverDelayingDelegate never_delaying_delegate;
  InitializeQueue(&queue, &always_delaying_delegate, &never_delaying_delegate);

  net::URLRequest request(GURL(kTestUrl), this);
  scoped_ptr<ResourceDispatcherHostRequestInfo> request_info(GetRequestInfo(0));
  EXPECT_EQ(0, response_started_count_);
  queue.AddRequest(&request, *request_info.get());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, response_started_count_);
  always_delaying_delegate.StartDelayedRequests();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, response_started_count_);

  queue.Shutdown();
}

TEST_F(ResourceQueueTest, RemoveRequest) {
  ResourceQueue queue;

  AlwaysDelayingDelegate delegate(&queue);
  InitializeQueue(&queue, &delegate);

  net::URLRequest request(GURL(kTestUrl), this);
  scoped_ptr<ResourceDispatcherHostRequestInfo> request_info(GetRequestInfo(0));
  GlobalRequestID request_id(request_info->child_id(),
                             request_info->request_id());
  EXPECT_EQ(0, response_started_count_);
  queue.AddRequest(&request, *request_info.get());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, response_started_count_);
  queue.RemoveRequest(request_id);
  delegate.StartDelayedRequests();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, response_started_count_);

  queue.Shutdown();

  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, response_started_count_);
}

}  // namespace
