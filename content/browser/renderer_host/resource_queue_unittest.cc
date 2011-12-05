// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/mock_resource_context.h"
#include "content/browser/renderer_host/dummy_resource_handler.h"
#include "content/browser/renderer_host/global_request_id.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/renderer_host/resource_queue.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::BrowserThreadImpl;
using content::DummyResourceHandler;

namespace {

const char kTestUrl[] = "data:text/plain,Hello World!";

ResourceDispatcherHostRequestInfo* GetRequestInfo(int request_id) {
  return new ResourceDispatcherHostRequestInfo(
      new DummyResourceHandler(), content::PROCESS_TYPE_RENDERER, 0, 0, 0,
      request_id, false, -1, false, -1, ResourceType::MAIN_FRAME,
      content::PAGE_TRANSITION_LINK, 0, false, false, false,
      WebKit::WebReferrerPolicyDefault,
      content::MockResourceContext::GetInstance());
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

  virtual void Initialize(ResourceQueue* resource_queue) {
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
  AlwaysDelayingDelegate() : resource_queue_(NULL) {
  }

  virtual void Initialize(ResourceQueue* resource_queue) {
    resource_queue_ = resource_queue;
  }

  virtual bool ShouldDelayRequest(
      net::URLRequest* request,
      const ResourceDispatcherHostRequestInfo& request_info,
      const GlobalRequestID& request_id) {
    return true;
  }

  virtual void WillShutdownResourceQueue() {
    resource_queue_ = NULL;
  }

  void StartDelayedRequests() {
    if (resource_queue_)
      resource_queue_->StartDelayedRequests(this);
  }

 private:
  typedef std::vector<GlobalRequestID> RequestList;

  ResourceQueue* resource_queue_;

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
  BrowserThreadImpl ui_thread_;
  BrowserThreadImpl io_thread_;
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

  AlwaysDelayingDelegate delegate;
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

  AlwaysDelayingDelegate delegate;
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

  AlwaysDelayingDelegate always_delaying_delegate;
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

TEST_F(ResourceQueueTest, TwoDelayingDelegates) {
  ResourceQueue queue;

  AlwaysDelayingDelegate always_delaying_delegate1;
  AlwaysDelayingDelegate always_delaying_delegate2;
  InitializeQueue(
      &queue, &always_delaying_delegate1, &always_delaying_delegate2);

  net::URLRequest request(GURL(kTestUrl), this);
  scoped_ptr<ResourceDispatcherHostRequestInfo> request_info(GetRequestInfo(0));
  EXPECT_EQ(0, response_started_count_);
  queue.AddRequest(&request, *request_info.get());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, response_started_count_);

  always_delaying_delegate1.StartDelayedRequests();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, response_started_count_);

  always_delaying_delegate2.StartDelayedRequests();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, response_started_count_);

  queue.Shutdown();
}

TEST_F(ResourceQueueTest, RemoveRequest) {
  ResourceQueue queue;

  AlwaysDelayingDelegate delegate;
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
