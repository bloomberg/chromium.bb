// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/resource_response.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/renderer_host/resource_handler.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "net/base/net_errors.h"
#include "net/base/upload_data.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache_interfaces.h"

// TODO(eroman): Write unit tests for SafeBrowsing that exercise
//               SafeBrowsingResourceHandler.

namespace {

// Returns the resource response header structure for this request.
void GetResponseHead(const std::vector<IPC::Message>& messages,
                     ResourceResponseHead* response_head) {
  ASSERT_GE(messages.size(), 2U);

  // The first messages should be received response.
  ASSERT_EQ(ViewMsg_Resource_ReceivedResponse::ID, messages[0].type());

  void* iter = NULL;
  int request_id;
  ASSERT_TRUE(IPC::ReadParam(&messages[0], &iter, &request_id));
  ASSERT_TRUE(IPC::ReadParam(&messages[0], &iter, response_head));
}

}  // namespace

static int RequestIDForMessage(const IPC::Message& msg) {
  int request_id = -1;
  switch (msg.type()) {
    case ViewMsg_Resource_UploadProgress::ID:
    case ViewMsg_Resource_ReceivedResponse::ID:
    case ViewMsg_Resource_ReceivedRedirect::ID:
    case ViewMsg_Resource_DataReceived::ID:
    case ViewMsg_Resource_RequestComplete::ID:
      request_id = IPC::MessageIterator(msg).NextInt();
      break;
  }
  return request_id;
}

static ViewHostMsg_Resource_Request CreateResourceRequest(
    const char* method,
    ResourceType::Type type,
    const GURL& url) {
  ViewHostMsg_Resource_Request request;
  request.method = std::string(method);
  request.url = url;
  request.first_party_for_cookies = url;  // bypass third-party cookie blocking
  request.load_flags = 0;
  request.origin_pid = 0;
  request.resource_type = type;
  request.request_context = 0;
  request.appcache_host_id = appcache::kNoHostId;
  request.download_to_file = false;
  request.host_renderer_id = -1;
  request.host_render_view_id = -1;
  return request;
}

// Spin up the message loop to kick off the request.
static void KickOffRequest() {
  MessageLoop::current()->RunAllPending();
}

// We may want to move this to a shared space if it is useful for something else
class ResourceIPCAccumulator {
 public:
  void AddMessage(const IPC::Message& msg) {
    messages_.push_back(msg);
  }

  // This groups the messages by their request ID. The groups will be in order
  // that the first message for each request ID was received, and the messages
  // within the groups will be in the order that they appeared.
  // Note that this clears messages_.
  typedef std::vector< std::vector<IPC::Message> > ClassifiedMessages;
  void GetClassifiedMessages(ClassifiedMessages* msgs);

  std::vector<IPC::Message> messages_;
};

// This is very inefficient as a result of repeatedly extracting the ID, use
// only for tests!
void ResourceIPCAccumulator::GetClassifiedMessages(ClassifiedMessages* msgs) {
  while (!messages_.empty()) {
    std::vector<IPC::Message> cur_requests;
    cur_requests.push_back(messages_[0]);
    int cur_id = RequestIDForMessage(messages_[0]);

    // find all other messages with this ID
    for (int i = 1; i < static_cast<int>(messages_.size()); i++) {
      int id = RequestIDForMessage(messages_[i]);
      if (id == cur_id) {
        cur_requests.push_back(messages_[i]);
        messages_.erase(messages_.begin() + i);
        i--;
      }
    }
    messages_.erase(messages_.begin());
    msgs->push_back(cur_requests);
  }
}

// This class forwards the incoming messages to the ResourceDispatcherHostTest.
// This is used to emulate different sub-processes, since this filter will
// have a different ID than the original. For the test, we want all the incoming
// messages to go to the same place, which is why this forwards.
class ForwardingFilter : public ResourceMessageFilter {
 public:
  explicit ForwardingFilter(IPC::Message::Sender* dest)
    : ResourceMessageFilter(ChildProcessInfo::GenerateChildProcessUniqueId(),
                            ChildProcessInfo::RENDER_PROCESS,
                            NULL),
      dest_(dest) {
    OnChannelConnected(base::GetCurrentProcId());
  }

  // ResourceMessageFilter override
  virtual bool Send(IPC::Message* msg) {
    if (!dest_)
      return false;
    return dest_->Send(msg);
  }

 private:
  IPC::Message::Sender* dest_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingFilter);
};

class ResourceDispatcherHostTest : public testing::Test,
                                   public IPC::Message::Sender {
 public:
  ResourceDispatcherHostTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(filter_(new ForwardingFilter(this))),
        ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        old_factory_(NULL),
        resource_type_(ResourceType::SUB_RESOURCE) {
  }
  // IPC::Message::Sender implementation
  virtual bool Send(IPC::Message* msg) {
    accum_.AddMessage(*msg);
    delete msg;
    return true;
  }

 protected:
  // testing::Test
  virtual void SetUp() {
    DCHECK(!test_fixture_);
    test_fixture_ = this;
    ChildProcessSecurityPolicy::GetInstance()->Add(0);
    net::URLRequest::RegisterProtocolFactory(
        "test",
        &ResourceDispatcherHostTest::Factory);
    EnsureTestSchemeIsAllowed();
  }

  virtual void TearDown() {
    net::URLRequest::RegisterProtocolFactory("test", NULL);
    if (!scheme_.empty())
      net::URLRequest::RegisterProtocolFactory(scheme_, old_factory_);

    DCHECK(test_fixture_ == this);
    test_fixture_ = NULL;

    host_.Shutdown();

    ChildProcessSecurityPolicy::GetInstance()->Remove(0);

    // The plugin lib is automatically loaded during these test
    // and we want a clean environment for other tests.
    ChromePluginLib::UnloadAllPlugins();

    // Flush the message loop to make Purify happy.
    message_loop_.RunAllPending();
  }

  // Creates a request using the current test object as the filter.
  void MakeTestRequest(int render_view_id,
                       int request_id,
                       const GURL& url);

  // Generates a request using the given filter. This will probably be a
  // ForwardingFilter.
  void MakeTestRequest(ResourceMessageFilter* filter,
                       int render_view_id,
                       int request_id,
                       const GURL& url);

  void MakeCancelRequest(int request_id);

  void EnsureTestSchemeIsAllowed() {
    static bool have_white_listed_test_scheme = false;

    if (!have_white_listed_test_scheme) {
      ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme("test");
      have_white_listed_test_scheme = true;
    }
  }

  // Sets a particular response for any request from now on. To switch back to
  // the default bahavior, pass an empty |headers|. |headers| should be raw-
  // formatted (NULLs instead of EOLs).
  void SetResponse(const std::string& headers, const std::string& data) {
    response_headers_ = headers;
    response_data_ = data;
  }

  // Sets a particular resource type for any request from now on.
  void SetResourceType(ResourceType::Type type) {
    resource_type_ = type;
  }

  // Intercepts requests for the given protocol.
  void HandleScheme(const std::string& scheme) {
    DCHECK(scheme_.empty());
    DCHECK(!old_factory_);
    scheme_ = scheme;
    old_factory_ = net::URLRequest::RegisterProtocolFactory(
        scheme_, &ResourceDispatcherHostTest::Factory);
  }

  // Our own net::URLRequestJob factory.
  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme) {
    if (test_fixture_->response_headers_.empty()) {
      return new net::URLRequestTestJob(request);
    } else {
      return new net::URLRequestTestJob(request,
                                        test_fixture_->response_headers_,
                                        test_fixture_->response_data_,
                                        false);
    }
  }

  scoped_refptr<ForwardingFilter> filter_;
  MessageLoopForIO message_loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
  ResourceDispatcherHost host_;
  ResourceIPCAccumulator accum_;
  std::string response_headers_;
  std::string response_data_;
  std::string scheme_;
  net::URLRequest::ProtocolFactory* old_factory_;
  ResourceType::Type resource_type_;
  static ResourceDispatcherHostTest* test_fixture_;
};
// Static.
ResourceDispatcherHostTest* ResourceDispatcherHostTest::test_fixture_ = NULL;

void ResourceDispatcherHostTest::MakeTestRequest(int render_view_id,
                                                 int request_id,
                                                 const GURL& url) {
  MakeTestRequest(filter_.get(), render_view_id, request_id, url);
}

void ResourceDispatcherHostTest::MakeTestRequest(
    ResourceMessageFilter* filter,
    int render_view_id,
    int request_id,
    const GURL& url) {
  ViewHostMsg_Resource_Request request =
      CreateResourceRequest("GET", resource_type_, url);
  ViewHostMsg_RequestResource msg(render_view_id, request_id, request);
  bool msg_was_ok;
  host_.OnMessageReceived(msg, filter, &msg_was_ok);
  KickOffRequest();
}

void ResourceDispatcherHostTest::MakeCancelRequest(int request_id) {
  host_.CancelRequest(filter_->child_id(), request_id, false);
}

void CheckSuccessfulRequest(const std::vector<IPC::Message>& messages,
                            const std::string& reference_data) {
  // A successful request will have received 4 messages:
  //     ReceivedResponse    (indicates headers received)
  //     DataReceived        (data)
  //    XXX DataReceived        (0 bytes remaining from a read)
  //     RequestComplete     (request is done)
  //
  // This function verifies that we received 4 messages and that they
  // are appropriate.
  ASSERT_EQ(3U, messages.size());

  // The first messages should be received response
  ASSERT_EQ(ViewMsg_Resource_ReceivedResponse::ID, messages[0].type());

  // followed by the data, currently we only do the data in one chunk, but
  // should probably test multiple chunks later
  ASSERT_EQ(ViewMsg_Resource_DataReceived::ID, messages[1].type());

  void* iter = NULL;
  int request_id;
  ASSERT_TRUE(IPC::ReadParam(&messages[1], &iter, &request_id));
  base::SharedMemoryHandle shm_handle;
  ASSERT_TRUE(IPC::ReadParam(&messages[1], &iter, &shm_handle));
  uint32 data_len;
  ASSERT_TRUE(IPC::ReadParam(&messages[1], &iter, &data_len));

  ASSERT_EQ(reference_data.size(), data_len);
  base::SharedMemory shared_mem(shm_handle, true);  // read only
  shared_mem.Map(data_len);
  const char* data = static_cast<char*>(shared_mem.memory());
  ASSERT_EQ(0, memcmp(reference_data.c_str(), data, data_len));

  // followed by a 0-byte read
  //ASSERT_EQ(ViewMsg_Resource_DataReceived::ID, messages[2].type());

  // the last message should be all data received
  ASSERT_EQ(ViewMsg_Resource_RequestComplete::ID, messages[2].type());
}

// Tests whether many messages get dispatched properly.
TEST_F(ResourceDispatcherHostTest, TestMany) {
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 2, net::URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 3, net::URLRequestTestJob::test_url_3());

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  // sorts out all the messages we saw by request
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // there are three requests, so we should have gotten them classified as such
  ASSERT_EQ(3U, msgs.size());

  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[2], net::URLRequestTestJob::test_data_3());
}

// Tests whether messages get canceled properly. We issue three requests,
// cancel one of them, and make sure that each sent the proper notifications.
TEST_F(ResourceDispatcherHostTest, Cancel) {
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 2, net::URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 3, net::URLRequestTestJob::test_url_3());
  MakeCancelRequest(2);

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // there are three requests, so we should have gotten them classified as such
  ASSERT_EQ(3U, msgs.size());

  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[2], net::URLRequestTestJob::test_data_3());

  // Check that request 2 got canceled.
  ASSERT_EQ(2U, msgs[1].size());
  ASSERT_EQ(ViewMsg_Resource_ReceivedResponse::ID, msgs[1][0].type());
  ASSERT_EQ(ViewMsg_Resource_RequestComplete::ID, msgs[1][1].type());

  int request_id;
  net::URLRequestStatus status;

  void* iter = NULL;
  ASSERT_TRUE(IPC::ReadParam(&msgs[1][1], &iter, &request_id));
  ASSERT_TRUE(IPC::ReadParam(&msgs[1][1], &iter, &status));

  EXPECT_EQ(net::URLRequestStatus::CANCELED, status.status());
}

// The host delegate acts as a second one so we can have some requests
// pending and some canceled.
class TestFilter : public ForwardingFilter {
 public:
  TestFilter()
      : ForwardingFilter(NULL),
        has_canceled_(false),
        received_after_canceled_(0) {
  }

  // ForwardingFilter override
  virtual bool Send(IPC::Message* msg) {
    // no messages should be received when the process has been canceled
    if (has_canceled_)
      received_after_canceled_++;
    delete msg;
    return true;
  }
  bool has_canceled_;
  int received_after_canceled_;
};

// Tests CancelRequestsForProcess
TEST_F(ResourceDispatcherHostTest, TestProcessCancel) {
  scoped_refptr<TestFilter> test_filter = new TestFilter();

  // request 1 goes to the test delegate
  ViewHostMsg_Resource_Request request = CreateResourceRequest(
      "GET", ResourceType::SUB_RESOURCE, net::URLRequestTestJob::test_url_1());

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  MakeTestRequest(test_filter.get(), 0, 1,
                  net::URLRequestTestJob::test_url_1());

  // request 2 goes to us
  MakeTestRequest(0, 2, net::URLRequestTestJob::test_url_2());

  // request 3 goes to the test delegate
  MakeTestRequest(test_filter.get(), 0, 3,
                  net::URLRequestTestJob::test_url_3());

  // TODO(mbelshe):
  // Now that the async IO path is in place, the IO always completes on the
  // initial call; so the requests have already completed.  This basically
  // breaks the whole test.
  //EXPECT_EQ(3, host_.pending_requests());

  // Process each request for one level so one callback is called.
  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());

  // Cancel the requests to the test process.
  host_.CancelRequestsForProcess(filter_->child_id());
  test_filter->has_canceled_ = true;

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.pending_requests());
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(filter_->child_id()));

  // The test delegate should not have gotten any messages after being canceled.
  ASSERT_EQ(0, test_filter->received_after_canceled_);

  // We should have gotten exactly one result.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());
  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_2());
}

// Tests blocking and resuming requests.
TEST_F(ResourceDispatcherHostTest, TestBlockingResumingRequests) {
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(filter_->child_id()));

  host_.BlockRequestsForRoute(filter_->child_id(), 1);
  host_.BlockRequestsForRoute(filter_->child_id(), 2);
  host_.BlockRequestsForRoute(filter_->child_id(), 3);

  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1());
  MakeTestRequest(1, 2, net::URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 3, net::URLRequestTestJob::test_url_3());
  MakeTestRequest(1, 4, net::URLRequestTestJob::test_url_1());
  MakeTestRequest(2, 5, net::URLRequestTestJob::test_url_2());
  MakeTestRequest(3, 6, net::URLRequestTestJob::test_url_3());

  // Flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // Sort out all the messages we saw by request
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // All requests but the 2 for the RVH 0 should have been blocked.
  ASSERT_EQ(2U, msgs.size());

  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_3());

  // Resume requests for RVH 1 and flush pending requests.
  host_.ResumeBlockedRequestsForRoute(filter_->child_id(), 1);
  KickOffRequest();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(2U, msgs.size());
  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_1());

  // Test that new requests are not blocked for RVH 1.
  MakeTestRequest(1, 7, net::URLRequestTestJob::test_url_1());
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());
  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());

  // Now resumes requests for all RVH (2 and 3).
  host_.ResumeBlockedRequestsForRoute(filter_->child_id(), 2);
  host_.ResumeBlockedRequestsForRoute(filter_->child_id(), 3);
  KickOffRequest();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(filter_->child_id()));

  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(2U, msgs.size());
  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_3());
}

// Tests blocking and canceling requests.
TEST_F(ResourceDispatcherHostTest, TestBlockingCancelingRequests) {
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(filter_->child_id()));

  host_.BlockRequestsForRoute(filter_->child_id(), 1);

  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1());
  MakeTestRequest(1, 2, net::URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 3, net::URLRequestTestJob::test_url_3());
  MakeTestRequest(1, 4, net::URLRequestTestJob::test_url_1());

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // Sort out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // The 2 requests for the RVH 0 should have been processed.
  ASSERT_EQ(2U, msgs.size());

  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_3());

  // Cancel requests for RVH 1.
  host_.CancelBlockedRequestsForRoute(filter_->child_id(), 1);
  KickOffRequest();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(filter_->child_id()));

  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(0U, msgs.size());
}

// Tests that blocked requests are canceled if their associated process dies.
TEST_F(ResourceDispatcherHostTest, TestBlockedRequestsProcessDies) {
  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = new ForwardingFilter(this);

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(filter_->child_id()));
  EXPECT_EQ(0,
            host_.GetOutstandingRequestsMemoryCost(second_filter->child_id()));

  host_.BlockRequestsForRoute(second_filter->child_id(), 0);

  MakeTestRequest(filter_.get(), 0, 1, net::URLRequestTestJob::test_url_1());
  MakeTestRequest(second_filter.get(), 0, 2,
                  net::URLRequestTestJob::test_url_2());
  MakeTestRequest(filter_.get(), 0, 3, net::URLRequestTestJob::test_url_3());
  MakeTestRequest(second_filter.get(), 0, 4,
                  net::URLRequestTestJob::test_url_1());

  // Simulate process death.
  host_.CancelRequestsForProcess(second_filter->child_id());

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(filter_->child_id()));
  EXPECT_EQ(0,
            host_.GetOutstandingRequestsMemoryCost(second_filter->child_id()));

  // Sort out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // The 2 requests for the RVH 0 should have been processed.
  ASSERT_EQ(2U, msgs.size());

  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_3());

  EXPECT_TRUE(host_.blocked_requests_map_.empty());
}

// Tests that blocked requests don't leak when the ResourceDispatcherHost goes
// away.  Note that we rely on Purify for finding the leaks if any.
// If this test turns the Purify bot red, check the ResourceDispatcherHost
// destructor to make sure the blocked requests are deleted.
TEST_F(ResourceDispatcherHostTest, TestBlockedRequestsDontLeak) {
  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = new ForwardingFilter(this);

  host_.BlockRequestsForRoute(filter_->child_id(), 1);
  host_.BlockRequestsForRoute(filter_->child_id(), 2);
  host_.BlockRequestsForRoute(second_filter->child_id(), 1);

  MakeTestRequest(filter_.get(), 0, 1, net::URLRequestTestJob::test_url_1());
  MakeTestRequest(filter_.get(), 1, 2, net::URLRequestTestJob::test_url_2());
  MakeTestRequest(filter_.get(), 0, 3, net::URLRequestTestJob::test_url_3());
  MakeTestRequest(second_filter.get(), 1, 4,
                  net::URLRequestTestJob::test_url_1());
  MakeTestRequest(filter_.get(), 2, 5, net::URLRequestTestJob::test_url_2());
  MakeTestRequest(filter_.get(), 2, 6, net::URLRequestTestJob::test_url_3());

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
}

// Test the private helper method "CalculateApproximateMemoryCost()".
TEST_F(ResourceDispatcherHostTest, CalculateApproximateMemoryCost) {
  net::URLRequest req(GURL("http://www.google.com"), NULL);
  EXPECT_EQ(4427, ResourceDispatcherHost::CalculateApproximateMemoryCost(&req));

  // Add 9 bytes of referrer.
  req.set_referrer("123456789");
  EXPECT_EQ(4436, ResourceDispatcherHost::CalculateApproximateMemoryCost(&req));

  // Add 33 bytes of upload content.
  std::string upload_content;
  upload_content.resize(33);
  std::fill(upload_content.begin(), upload_content.end(), 'x');
  req.AppendBytesToUpload(upload_content.data(), upload_content.size());

  // Since the upload throttling is disabled, this has no effect on the cost.
  EXPECT_EQ(4436, ResourceDispatcherHost::CalculateApproximateMemoryCost(&req));

  // Add a file upload -- should have no effect.
  req.AppendFileToUpload(FilePath(FILE_PATH_LITERAL("does-not-exist.png")));
  EXPECT_EQ(4436, ResourceDispatcherHost::CalculateApproximateMemoryCost(&req));
}

// Test the private helper method "IncrementOutstandingRequestsMemoryCost()".
TEST_F(ResourceDispatcherHostTest, IncrementOutstandingRequestsMemoryCost) {
  // Add some counts for render_process_host=7
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(7));
  EXPECT_EQ(1, host_.IncrementOutstandingRequestsMemoryCost(1, 7));
  EXPECT_EQ(2, host_.IncrementOutstandingRequestsMemoryCost(1, 7));
  EXPECT_EQ(3, host_.IncrementOutstandingRequestsMemoryCost(1, 7));

  // Add some counts for render_process_host=3
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(3));
  EXPECT_EQ(1, host_.IncrementOutstandingRequestsMemoryCost(1, 3));
  EXPECT_EQ(2, host_.IncrementOutstandingRequestsMemoryCost(1, 3));

  // Remove all the counts for render_process_host=7
  EXPECT_EQ(3, host_.GetOutstandingRequestsMemoryCost(7));
  EXPECT_EQ(2, host_.IncrementOutstandingRequestsMemoryCost(-1, 7));
  EXPECT_EQ(1, host_.IncrementOutstandingRequestsMemoryCost(-1, 7));
  EXPECT_EQ(0, host_.IncrementOutstandingRequestsMemoryCost(-1, 7));
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(7));

  // Remove all the counts for render_process_host=3
  EXPECT_EQ(2, host_.GetOutstandingRequestsMemoryCost(3));
  EXPECT_EQ(1, host_.IncrementOutstandingRequestsMemoryCost(-1, 3));
  EXPECT_EQ(0, host_.IncrementOutstandingRequestsMemoryCost(-1, 3));
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(3));

  // When an entry reaches 0, it should be deleted.
  EXPECT_TRUE(host_.outstanding_requests_memory_cost_map_.end() ==
      host_.outstanding_requests_memory_cost_map_.find(7));
  EXPECT_TRUE(host_.outstanding_requests_memory_cost_map_.end() ==
      host_.outstanding_requests_memory_cost_map_.find(3));
}

// Test that when too many requests are outstanding for a particular
// render_process_host_id, any subsequent request from it fails.
TEST_F(ResourceDispatcherHostTest, TooManyOutstandingRequests) {
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(filter_->child_id()));

  // Expected cost of each request as measured by
  // ResourceDispatcherHost::CalculateApproximateMemoryCost().
  int kMemoryCostOfTest2Req =
      ResourceDispatcherHost::kAvgBytesPerOutstandingRequest +
      std::string("GET").size() +
      net::URLRequestTestJob::test_url_2().spec().size();

  // Tighten the bound on the ResourceDispatcherHost, to speed things up.
  int kMaxCostPerProcess = 440000;
  host_.set_max_outstanding_requests_cost_per_process(kMaxCostPerProcess);

  // Determine how many instance of test_url_2() we can request before
  // throttling kicks in.
  size_t kMaxRequests = kMaxCostPerProcess / kMemoryCostOfTest2Req;

  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = new ForwardingFilter(this);

  // Saturate the number of outstanding requests for our process.
  for (size_t i = 0; i < kMaxRequests; ++i) {
    MakeTestRequest(filter_.get(), 0, i + 1,
                    net::URLRequestTestJob::test_url_2());
  }

  // Issue two more requests for our process -- these should fail immediately.
  MakeTestRequest(filter_.get(), 0, kMaxRequests + 1,
                  net::URLRequestTestJob::test_url_2());
  MakeTestRequest(filter_.get(), 0, kMaxRequests + 2,
                  net::URLRequestTestJob::test_url_2());

  // Issue two requests for the second process -- these should succeed since
  // it is just process 0 that is saturated.
  MakeTestRequest(second_filter.get(), 0, kMaxRequests + 3,
                  net::URLRequestTestJob::test_url_2());
  MakeTestRequest(second_filter.get(), 0, kMaxRequests + 4,
                  net::URLRequestTestJob::test_url_2());

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(filter_->child_id()));

  // Sorts out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // We issued (kMaxRequests + 4) total requests.
  ASSERT_EQ(kMaxRequests + 4, msgs.size());

  // Check that the first kMaxRequests succeeded.
  for (size_t i = 0; i < kMaxRequests; ++i)
    CheckSuccessfulRequest(msgs[i], net::URLRequestTestJob::test_data_2());

  // Check that the subsequent two requests (kMaxRequests + 1) and
  // (kMaxRequests + 2) were failed, since the per-process bound was reached.
  for (int i = 0; i < 2; ++i) {
    // Should have sent a single RequestComplete message.
    int index = kMaxRequests + i;
    EXPECT_EQ(1U, msgs[index].size());
    EXPECT_EQ(ViewMsg_Resource_RequestComplete::ID, msgs[index][0].type());

    // The RequestComplete message should have had status
    // (CANCELLED, ERR_INSUFFICIENT_RESOURCES).
    int request_id;
    net::URLRequestStatus status;

    void* iter = NULL;
    EXPECT_TRUE(IPC::ReadParam(&msgs[index][0], &iter, &request_id));
    EXPECT_TRUE(IPC::ReadParam(&msgs[index][0], &iter, &status));

    EXPECT_EQ(index + 1, request_id);
    EXPECT_EQ(net::URLRequestStatus::CANCELED, status.status());
    EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES, status.os_error());
  }

  // The final 2 requests should have succeeded.
  CheckSuccessfulRequest(msgs[kMaxRequests + 2],
                         net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[kMaxRequests + 3],
                         net::URLRequestTestJob::test_data_2());
}

// Tests that we sniff the mime type for a simple request.
TEST_F(ResourceDispatcherHostTest, MimeSniffed) {
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  std::string response("HTTP/1.1 200 OK\n\n");
  std::string raw_headers(net::HttpUtil::AssembleRawHeaders(response.data(),
                                                            response.size()));
  std::string response_data("<html><title>Test One</title></html>");
  SetResponse(raw_headers, response_data);

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"));

  // Flush all pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  // Sorts out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());

  ResourceResponseHead response_head;
  GetResponseHead(msgs[0], &response_head);
  ASSERT_EQ("text/html", response_head.mime_type);
}

// Tests that we don't sniff the mime type when the server provides one.
TEST_F(ResourceDispatcherHostTest, MimeNotSniffed) {
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  std::string response("HTTP/1.1 200 OK\n"
                       "Content-type: image/jpeg\n\n");
  std::string raw_headers(net::HttpUtil::AssembleRawHeaders(response.data(),
                                                            response.size()));
  std::string response_data("<html><title>Test One</title></html>");
  SetResponse(raw_headers, response_data);

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"));

  // Flush all pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  // Sorts out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());

  ResourceResponseHead response_head;
  GetResponseHead(msgs[0], &response_head);
  ASSERT_EQ("image/jpeg", response_head.mime_type);
}

// Tests that we don't sniff the mime type when there is no message body.
TEST_F(ResourceDispatcherHostTest, MimeNotSniffed2) {
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  std::string response("HTTP/1.1 304 Not Modified\n\n");
  std::string raw_headers(net::HttpUtil::AssembleRawHeaders(response.data(),
                                                            response.size()));
  std::string response_data;
  SetResponse(raw_headers, response_data);

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"));

  // Flush all pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  // Sorts out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());

  ResourceResponseHead response_head;
  GetResponseHead(msgs[0], &response_head);
  ASSERT_EQ("", response_head.mime_type);
}

TEST_F(ResourceDispatcherHostTest, MimeSniff204) {
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  std::string response("HTTP/1.1 204 No Content\n\n");
  std::string raw_headers(net::HttpUtil::AssembleRawHeaders(response.data(),
                                                            response.size()));
  std::string response_data;
  SetResponse(raw_headers, response_data);

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"));

  // Flush all pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  // Sorts out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());

  ResourceResponseHead response_head;
  GetResponseHead(msgs[0], &response_head);
  ASSERT_EQ("text/plain", response_head.mime_type);
}

// Tests for crbug.com/31266 (Non-2xx + application/octet-stream).
TEST_F(ResourceDispatcherHostTest, ForbiddenDownload) {
  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  std::string response("HTTP/1.1 403 Forbidden\n"
                       "Content-disposition: attachment; filename=blah\n"
                       "Content-type: application/octet-stream\n\n");
  std::string raw_headers(net::HttpUtil::AssembleRawHeaders(response.data(),
                                                            response.size()));
  std::string response_data("<html><title>Test One</title></html>");
  SetResponse(raw_headers, response_data);

  // Only MAIN_FRAMEs can trigger a download.
  SetResourceType(ResourceType::MAIN_FRAME);

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"));

  // Flush all pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.GetOutstandingRequestsMemoryCost(0));

  // Sorts out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // We should have gotten one RequestComplete message.
  ASSERT_EQ(1U, msgs[0].size());
  EXPECT_EQ(ViewMsg_Resource_RequestComplete::ID, msgs[0][0].type());

  // The RequestComplete message should have had status
  // (CANCELED, ERR_FILE_NOT_FOUND).
  int request_id;
  net::URLRequestStatus status;

  void* iter = NULL;
  EXPECT_TRUE(IPC::ReadParam(&msgs[0][0], &iter, &request_id));
  EXPECT_TRUE(IPC::ReadParam(&msgs[0][0], &iter, &status));

  EXPECT_EQ(1, request_id);
  EXPECT_EQ(net::URLRequestStatus::CANCELED, status.status());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, status.os_error());
}

class DummyResourceHandler : public ResourceHandler {
 public:
  DummyResourceHandler() {}

  // Called as upload progress is made.
  bool OnUploadProgress(int request_id, uint64 position, uint64 size) {
    return true;
  }

  bool OnRequestRedirected(int request_id, const GURL& url,
                           ResourceResponse* response, bool* defer) {
    return true;
  }

  bool OnResponseStarted(int request_id, ResourceResponse* response) {
    return true;
  }

  bool OnWillStart(int request_id, const GURL& url, bool* defer) {
    return true;
  }

  bool OnWillRead(
      int request_id, net::IOBuffer** buf, int* buf_size, int min_size) {
    return true;
  }

  bool OnReadCompleted(int request_id, int* bytes_read) { return true; }

  bool OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& info) {
    return true;
  }

  void OnRequestClosed() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyResourceHandler);
};

class ApplyExtensionLocalizationFilterTest : public testing::Test {
 protected:
  void SetUp() {
    url_.reset(new GURL(
        "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/popup.html"));
    resource_type_ = ResourceType::STYLESHEET;
    resource_handler_.reset(new DummyResourceHandler());
    request_info_.reset(CreateNewResourceRequestInfo());
  }

  ResourceDispatcherHostRequestInfo* CreateNewResourceRequestInfo() {
    return new ResourceDispatcherHostRequestInfo(
        resource_handler_.get(), ChildProcessInfo::RENDER_PROCESS, 0, 0, 0,
        ResourceType::STYLESHEET, 0U, false, false, false, -1, -1);
  }

  scoped_ptr<GURL> url_;
  ResourceType::Type resource_type_;
  scoped_ptr<DummyResourceHandler> resource_handler_;
  scoped_ptr<ResourceDispatcherHostRequestInfo> request_info_;
};

TEST_F(ApplyExtensionLocalizationFilterTest, WrongScheme) {
  url_.reset(new GURL("html://behllobkkfkfnphdnhnkndlbkcpglgmj/popup.html"));
  ResourceDispatcherHost::ApplyExtensionLocalizationFilter(*url_,
      resource_type_, request_info_.get());

  EXPECT_FALSE(request_info_->replace_extension_localization_templates());
}

TEST_F(ApplyExtensionLocalizationFilterTest, GoodScheme) {
  ResourceDispatcherHost::ApplyExtensionLocalizationFilter(*url_,
      resource_type_, request_info_.get());

  EXPECT_TRUE(request_info_->replace_extension_localization_templates());
}

TEST_F(ApplyExtensionLocalizationFilterTest, GoodSchemeWrongResourceType) {
  resource_type_ = ResourceType::MAIN_FRAME;
  ResourceDispatcherHost::ApplyExtensionLocalizationFilter(*url_,
      resource_type_, request_info_.get());

  EXPECT_FALSE(request_info_->replace_extension_localization_templates());
}
