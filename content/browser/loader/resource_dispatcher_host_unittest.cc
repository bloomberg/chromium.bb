// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/downloaded_temp_file_impl.h"
#include "content/browser/loader/navigation_resource_throttle.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_loader.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader_delegate_impl.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/navigation_params.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/process_type.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_response.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_navigation_url_loader_delegate.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(eroman): Write unit tests for SafeBrowsing that exercise
//               SafeBrowsingResourceHandler.

using storage::ShareableFileReference;

namespace content {

namespace {

// Returns the resource response header structure for this request.
void GetResponseHead(const std::vector<IPC::Message>& messages,
                     ResourceResponseHead* response_head) {
  ASSERT_GE(messages.size(), 2U);

  // The first messages should be received response.
  ASSERT_EQ(ResourceMsg_ReceivedResponse::ID, messages[0].type());

  base::PickleIterator iter(messages[0]);
  int request_id;
  ASSERT_TRUE(IPC::ReadParam(&messages[0], &iter, &request_id));
  ASSERT_TRUE(IPC::ReadParam(&messages[0], &iter, response_head));
}

void GenerateIPCMessage(scoped_refptr<ResourceRequesterInfo> requester_info,
                        std::unique_ptr<IPC::Message> message) {
  ResourceDispatcherHostImpl::Get()->OnMessageReceived(*message,
                                                       requester_info.get());
}

// On Windows, ResourceMsg_SetDataBuffer supplies a HANDLE which is not
// automatically released.
//
// See ResourceDispatcher::ReleaseResourcesInDataMessage.
//
// TODO(davidben): It would be nice if the behavior for base::SharedMemoryHandle
// were more like it is in POSIX where the received fds are tracked in a
// ref-counted core that closes them if not extracted.
void ReleaseHandlesInMessage(const IPC::Message& message) {
  if (message.type() == ResourceMsg_SetDataBuffer::ID) {
    base::PickleIterator iter(message);
    int request_id;
    CHECK(iter.ReadInt(&request_id));
    base::SharedMemoryHandle shm_handle;
    if (IPC::ParamTraits<base::SharedMemoryHandle>::Read(&message,
                                                         &iter,
                                                         &shm_handle)) {
      if (base::SharedMemory::IsHandleValid(shm_handle))
        base::SharedMemory::CloseHandle(shm_handle);
    }
  }
}

}  // namespace

static int RequestIDForMessage(const IPC::Message& msg) {
  int request_id = -1;
  switch (msg.type()) {
    case ResourceMsg_UploadProgress::ID:
    case ResourceMsg_ReceivedResponse::ID:
    case ResourceMsg_ReceivedRedirect::ID:
    case ResourceMsg_SetDataBuffer::ID:
    case ResourceMsg_DataReceived::ID:
    case ResourceMsg_DataDownloaded::ID:
    case ResourceMsg_RequestComplete::ID: {
      bool result = base::PickleIterator(msg).ReadInt(&request_id);
      DCHECK(result);
      break;
    }
  }
  return request_id;
}

static ResourceRequest CreateResourceRequest(const char* method,
                                             ResourceType type,
                                             const GURL& url) {
  ResourceRequest request;
  request.method = std::string(method);
  request.url = url;
  request.site_for_cookies = url;  // bypass third-party cookie blocking
  request.request_initiator = url::Origin(url);  // ensure initiator is set
  request.referrer_policy = blink::kWebReferrerPolicyDefault;
  request.load_flags = 0;
  request.origin_pid = 0;
  request.resource_type = type;
  request.request_context = 0;
  request.appcache_host_id = kAppCacheNoHostId;
  request.download_to_file = false;
  request.should_reset_appcache = false;
  request.is_main_frame = true;
  request.transition_type = ui::PAGE_TRANSITION_LINK;
  request.allow_download = true;
  request.keepalive = (type == RESOURCE_TYPE_PING);
  return request;
}

// Spin up the message loop to kick off the request.
static void KickOffRequest() {
  content::RunAllTasksUntilIdle();
}

// We may want to move this to a shared space if it is useful for something else
class ResourceIPCAccumulator {
 public:
  ~ResourceIPCAccumulator() {
    for (size_t i = 0; i < messages_.size(); i++) {
      ReleaseHandlesInMessage(messages_[i]);
    }
  }

  // On Windows, takes ownership of SharedMemoryHandles in |msg|.
  void AddMessage(const IPC::Message& msg) {
    messages_.push_back(msg);
  }

  // This groups the messages by their request ID. The groups will be in order
  // that the first message for each request ID was received, and the messages
  // within the groups will be in the order that they appeared.
  // Note that this clears messages_. The caller takes ownership of any
  // SharedMemoryHandles in messages placed into |msgs|.
  // TODO(mmenke):  This seems really fragile.  Consider reworking ownership.
  typedef std::vector< std::vector<IPC::Message> > ClassifiedMessages;
  void GetClassifiedMessages(ClassifiedMessages* msgs);

  // Returns the reply of |msg|, which is a synchronous IPC message.
  const IPC::Message* GetReply(const IPC::Message& msg);

 private:
  std::vector<IPC::Message> messages_;
};

// This is very inefficient as a result of repeatedly extracting the ID, use
// only for tests!
void ResourceIPCAccumulator::GetClassifiedMessages(ClassifiedMessages* msgs) {
  while (!messages_.empty()) {
    // Ignore unknown message types as it is valid for code to generated other
    // IPCs as side-effects that we are not testing here.
    int cur_id = RequestIDForMessage(messages_[0]);
    if (cur_id != -1) {
      std::vector<IPC::Message> cur_requests;
      cur_requests.push_back(messages_[0]);
      // find all other messages with this ID
      for (int i = 1; i < static_cast<int>(messages_.size()); i++) {
        int id = RequestIDForMessage(messages_[i]);
        if (id == cur_id) {
          cur_requests.push_back(messages_[i]);
          messages_.erase(messages_.begin() + i);
          i--;
        }
      }
      msgs->push_back(cur_requests);
    }
    messages_.erase(messages_.begin());
  }
}

const IPC::Message* ResourceIPCAccumulator::GetReply(const IPC::Message& msg) {
  for (auto& reply : messages_) {
    if (!reply.is_reply())
      continue;
    if (IPC::SyncMessage::GetMessageId(reply) ==
        IPC::SyncMessage::GetMessageId(msg))
      return &reply;
  }
  return nullptr;
}

// This is used to create a filter matching a specified child id.
class TestFilterSpecifyingChild : public ResourceMessageFilter {
 public:
  explicit TestFilterSpecifyingChild(ResourceContext* resource_context,
                                     int process_id)
      : ResourceMessageFilter(
            process_id,
            NULL,
            NULL,
            NULL,
            NULL,
            base::Bind(&TestFilterSpecifyingChild::GetContexts,
                       base::Unretained(this)),
            BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)),
        resource_context_(resource_context),
        canceled_(false),
        received_after_canceled_(0) {
    InitializeForTest();
    set_peer_process_for_testing(base::Process::Current());
  }

  void set_canceled(bool canceled) { canceled_ = canceled; }
  int received_after_canceled() const { return received_after_canceled_; }

  // ResourceMessageFilter override
  bool Send(IPC::Message* msg) override {
    // No messages should be received when the process has been canceled.
    if (canceled_)
      received_after_canceled_++;
    ReleaseHandlesInMessage(*msg);
    delete msg;
    return true;
  }

  ResourceContext* resource_context() { return resource_context_; }

 protected:
  ~TestFilterSpecifyingChild() override {}

 private:
  void GetContexts(ResourceType resource_type,
                   ResourceContext** resource_context,
                   net::URLRequestContext** request_context) {
    *resource_context = resource_context_;
    *request_context = resource_context_->GetRequestContext();
  }

  ResourceContext* resource_context_;
  bool canceled_;
  int received_after_canceled_;

  DISALLOW_COPY_AND_ASSIGN(TestFilterSpecifyingChild);
};

class TestFilter : public TestFilterSpecifyingChild {
 public:
  explicit TestFilter(ResourceContext* resource_context)
      : TestFilterSpecifyingChild(
            resource_context,
            ChildProcessHostImpl::GenerateChildProcessUniqueId()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->Add(child_id());
  }

 protected:
  ~TestFilter() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(child_id());
  }
};

// This class forwards the incoming messages to the ResourceDispatcherHostTest.
// For the test, we want all the incoming messages to go to the same place,
// which is why this forwards.
class ForwardingFilter : public TestFilter {
 public:
  explicit ForwardingFilter(IPC::Sender* dest,
                            ResourceContext* resource_context)
      : TestFilter(resource_context),
        dest_(dest) {
  }

  // TestFilter override
  bool Send(IPC::Message* msg) override { return dest_->Send(msg); }

 private:
  ~ForwardingFilter() override {}

  IPC::Sender* dest_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingFilter);
};

// This class is a variation on URLRequestTestJob in that it does
// not complete start upon entry, only when specifically told to.
class URLRequestTestDelayedStartJob : public net::URLRequestTestJob {
 public:
  URLRequestTestDelayedStartJob(net::URLRequest* request,
                                net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request, network_delegate) {
    Init();
  }
  URLRequestTestDelayedStartJob(net::URLRequest* request,
                                net::NetworkDelegate* network_delegate,
                                const std::string& response_headers,
                                const std::string& response_data,
                                bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {
    Init();
  }

  // Do nothing until you're told to.
  void Start() override {}

  // Finish starting a URL request whose job is an instance of
  // URLRequestTestDelayedStartJob.  It is illegal to call this routine
  // with a URLRequest that does not use URLRequestTestDelayedStartJob.
  static void CompleteStart(net::URLRequest* request) {
    for (URLRequestTestDelayedStartJob* job = list_head_;
         job;
         job = job->next_) {
      if (job->request() == request) {
        job->net::URLRequestTestJob::Start();
        return;
      }
    }
    NOTREACHED();
  }

  static bool DelayedStartQueueEmpty() {
    return !list_head_;
  }

  static void ClearQueue() {
    if (list_head_) {
      LOG(ERROR)
          << "Unreleased entries on URLRequestTestDelayedStartJob delay queue"
          << "; may result in leaks.";
      list_head_ = NULL;
    }
  }

 protected:
  ~URLRequestTestDelayedStartJob() override {
    for (URLRequestTestDelayedStartJob** job = &list_head_; *job;
         job = &(*job)->next_) {
      if (*job == this) {
        *job = (*job)->next_;
        return;
      }
    }
    NOTREACHED();
  }

 private:
  void Init() {
    next_ = list_head_;
    list_head_ = this;
  }

  static URLRequestTestDelayedStartJob* list_head_;
  URLRequestTestDelayedStartJob* next_;
};

URLRequestTestDelayedStartJob*
URLRequestTestDelayedStartJob::list_head_ = NULL;

// This class is a variation on URLRequestTestJob in that it
// returns IO_pending errors before every read, not just the first one.
class URLRequestTestDelayedCompletionJob : public net::URLRequestTestJob {
 public:
  URLRequestTestDelayedCompletionJob(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request, network_delegate) {}
  URLRequestTestDelayedCompletionJob(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate,
                                     bool auto_advance)
      : net::URLRequestTestJob(request, network_delegate, auto_advance) {}
  URLRequestTestDelayedCompletionJob(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate,
                                     const std::string& response_headers,
                                     const std::string& response_data,
                                     bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {}

 protected:
  ~URLRequestTestDelayedCompletionJob() override {}

 private:
  bool NextReadAsync() override { return true; }
};

// This class is a variation on URLRequestTestJob that ensures that no read is
// asked of this job.
class URLRequestMustNotReadTestJob : public net::URLRequestTestJob {
 public:
  URLRequestMustNotReadTestJob(net::URLRequest* request,
                               net::NetworkDelegate* network_delegate,
                               const std::string& response_headers,
                               const std::string& response_data)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               false) {}

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    EXPECT_TRUE(false) << "The job should have been cancelled before trying to "
                          "read the response body.";
    return 0;
  }
};

class URLRequestBigJob : public net::URLRequestSimpleJob {
 public:
  URLRequestBigJob(net::URLRequest* request,
                   net::NetworkDelegate* network_delegate)
      : net::URLRequestSimpleJob(request, network_delegate) {
  }

  // URLRequestSimpleJob implementation:
  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              const net::CompletionCallback& callback) const override {
    *mime_type = "text/plain";
    *charset = "UTF-8";

    std::string text;
    int count;
    if (!ParseURL(request_->url(), &text, &count))
      return net::ERR_INVALID_URL;

    data->reserve(text.size() * count);
    for (int i = 0; i < count; ++i)
      data->append(text);

    return net::OK;
  }

 private:
  ~URLRequestBigJob() override {}

  // big-job:substring,N
  static bool ParseURL(const GURL& url, std::string* text, int* count) {
    std::vector<std::string> parts = base::SplitString(
        url.path(), ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    if (parts.size() != 2)
      return false;

    *text = parts[0];
    return base::StringToInt(parts[1], count);
  }
};

// URLRequestJob used to test GetLoadInfoForAllRoutes.  The LoadState and
// UploadProgress values are set for the jobs at the time of creation, and
// the jobs will never actually do anything.
class URLRequestLoadInfoJob : public net::URLRequestJob {
 public:
  URLRequestLoadInfoJob(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate,
                        const net::LoadState& load_state)
      : net::URLRequestJob(request, network_delegate),
        load_state_(load_state) {}

  // net::URLRequestJob implementation:
  void Start() override {}
  net::LoadState GetLoadState() const override { return load_state_; }

 private:
  ~URLRequestLoadInfoJob() override {}

  // big-job:substring,N
  static bool ParseURL(const GURL& url, std::string* text, int* count) {
    std::vector<std::string> parts = base::SplitString(
        url.path(), ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    if (parts.size() != 2)
      return false;

    *text = parts[0];
    return base::StringToInt(parts[1], count);
  }

  const net::LoadState load_state_;
};

class ResourceDispatcherHostTest;

class TestURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  explicit TestURLRequestJobFactory(ResourceDispatcherHostTest* test_fixture)
      : test_fixture_(test_fixture),
        hang_after_start_(false),
        delay_start_(false),
        delay_complete_(false),
        must_not_read_(false),
        url_request_jobs_created_count_(0) {}

  void HandleScheme(const std::string& scheme) {
    supported_schemes_.insert(scheme);
  }

  int url_request_jobs_created_count() const {
    return url_request_jobs_created_count_;
  }

  // When set, jobs will hang eternally once started.
  void SetHangAfterStartJobGeneration(bool hang_after_start) {
    hang_after_start_ = hang_after_start;
  }

  void SetDelayedStartJobGeneration(bool delay_job_start) {
    delay_start_ = delay_job_start;
  }

  void SetDelayedCompleteJobGeneration(bool delay_job_complete) {
    delay_complete_ = delay_job_complete;
  }

  void SetMustNotReadJobGeneration(bool must_not_read) {
    must_not_read_ = must_not_read;
  }

  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override;

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  bool IsHandledProtocol(const std::string& scheme) const override {
    return supported_schemes_.count(scheme) > 0;
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return false;
  }

 private:
  ResourceDispatcherHostTest* test_fixture_;
  bool hang_after_start_;
  bool delay_start_;
  bool delay_complete_;
  bool must_not_read_;
  mutable int url_request_jobs_created_count_;
  std::set<std::string> supported_schemes_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestJobFactory);
};

// Associated with an URLRequest to determine if the URLRequest gets deleted.
class TestUserData : public base::SupportsUserData::Data {
 public:
  explicit TestUserData(bool* was_deleted)
      : was_deleted_(was_deleted) {
  }

  ~TestUserData() override { *was_deleted_ = true; }

 private:
  bool* was_deleted_;
};

enum GenericResourceThrottleFlags {
  NONE                       = 0,
  DEFER_STARTING_REQUEST     = 1 << 0,
  DEFER_PROCESSING_RESPONSE  = 1 << 1,
  CANCEL_BEFORE_START        = 1 << 2,
  CANCEL_PROCESSING_RESPONSE = 1 << 3,
  MUST_NOT_CACHE_BODY        = 1 << 4,
};

// Throttle that tracks the current throttle blocking a request.  Only one
// can throttle any request at a time.
class GenericResourceThrottle : public ResourceThrottle {
 public:
  // The value is used to indicate that the throttle should not provide
  // a error code when cancelling a request. net::OK is used, because this
  // is not an error code.
  static const int USE_DEFAULT_CANCEL_ERROR_CODE = net::OK;

  GenericResourceThrottle(int flags, int code)
      : flags_(flags),
        error_code_for_cancellation_(code) {
  }

  ~GenericResourceThrottle() override {
    if (active_throttle_ == this)
      active_throttle_ = NULL;
  }

  // ResourceThrottle implementation:
  void WillStartRequest(bool* defer) override {
    ASSERT_EQ(NULL, active_throttle_);
    if (flags_ & DEFER_STARTING_REQUEST) {
      active_throttle_ = this;
      *defer = true;
    }

    if (flags_ & CANCEL_BEFORE_START) {
      if (error_code_for_cancellation_ == USE_DEFAULT_CANCEL_ERROR_CODE) {
        Cancel();
      } else {
        CancelWithError(error_code_for_cancellation_);
      }
    }
  }

  void WillProcessResponse(bool* defer) override {
    ASSERT_EQ(NULL, active_throttle_);
    if (flags_ & DEFER_PROCESSING_RESPONSE) {
      active_throttle_ = this;
      *defer = true;
    }

    if (flags_ & CANCEL_PROCESSING_RESPONSE) {
      if (error_code_for_cancellation_ == USE_DEFAULT_CANCEL_ERROR_CODE) {
        Cancel();
      } else {
        CancelWithError(error_code_for_cancellation_);
      }
    }
  }

  const char* GetNameForLogging() const override {
    return "GenericResourceThrottle";
  }

  void AssertAndResume() {
    ASSERT_TRUE(this == active_throttle_);
    active_throttle_ = NULL;
    ResourceThrottle::Resume();
  }

  static GenericResourceThrottle* active_throttle() {
    return active_throttle_;
  }

  bool MustProcessResponseBeforeReadingBody() override {
    return flags_ & MUST_NOT_CACHE_BODY;
  }

 private:
  int flags_;  // bit-wise union of GenericResourceThrottleFlags.
  int error_code_for_cancellation_;

  // The currently active throttle, if any.
  static GenericResourceThrottle* active_throttle_;
};
// static
GenericResourceThrottle* GenericResourceThrottle::active_throttle_ = NULL;

class TestResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  TestResourceDispatcherHostDelegate()
      : create_two_throttles_(false),
        flags_(NONE),
        error_code_for_cancellation_(
            GenericResourceThrottle::USE_DEFAULT_CANCEL_ERROR_CODE) {
  }

  void set_url_request_user_data(base::SupportsUserData::Data* user_data) {
    user_data_.reset(user_data);
  }

  void set_flags(int value) {
    flags_ = value;
  }

  void set_error_code_for_cancellation(int code) {
    error_code_for_cancellation_ = code;
  }

  void set_create_two_throttles(bool create_two_throttles) {
    create_two_throttles_ = create_two_throttles;
  }

  // ResourceDispatcherHostDelegate implementation:

  void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      AppCacheService* appcache_service,
      ResourceType resource_type,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles) override {
    if (user_data_) {
      const void* key = user_data_.get();
      request->SetUserData(key, std::move(user_data_));
    }

    if (flags_ != NONE) {
      throttles->push_back(base::MakeUnique<GenericResourceThrottle>(
          flags_, error_code_for_cancellation_));
      if (create_two_throttles_)
        throttles->push_back(base::MakeUnique<GenericResourceThrottle>(
            flags_, error_code_for_cancellation_));
    }
  }

 private:
  bool create_two_throttles_;
  int flags_;
  int error_code_for_cancellation_;
  std::unique_ptr<base::SupportsUserData::Data> user_data_;
};

// Waits for a ShareableFileReference to be released.
class ShareableFileReleaseWaiter {
 public:
  explicit ShareableFileReleaseWaiter(const base::FilePath& path) {
    scoped_refptr<ShareableFileReference> file =
        ShareableFileReference::Get(path);
    file->AddFinalReleaseCallback(
        base::Bind(&ShareableFileReleaseWaiter::Released,
                   base::Unretained(this)));
  }

  void Wait() {
    loop_.Run();
  }

 private:
  void Released(const base::FilePath& path) {
    loop_.Quit();
  }

  base::RunLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(ShareableFileReleaseWaiter);
};

// For verifying notifications sent to the WebContents by the
// ResourceDispatcherHostImpl.
class TestWebContentsObserver : public WebContentsObserver {
 public:
  explicit TestWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        resource_request_redirect_count_(0),
        resource_response_start_count_(0) {}

  void DidGetRedirectForResourceRequest(
      const ResourceRedirectDetails& details) override {
    ++resource_request_redirect_count_;
  }

  void DidGetResourceResponseStart(
      const ResourceRequestDetails& details) override {
    ++resource_response_start_count_;
  }

  int resource_response_start_count() { return resource_response_start_count_; }

  int resource_request_redirect_count() {
    return resource_request_redirect_count_;
  }

 private:
  int resource_request_redirect_count_;
  int resource_response_start_count_;
};

void CheckRequestCompleteErrorCode(const IPC::Message& message,
                                   int expected_error_code) {
  // Verify the expected error code was received.
  int request_id;
  int error_code;

  ASSERT_EQ(ResourceMsg_RequestComplete::ID, message.type());

  base::PickleIterator iter(message);
  ASSERT_TRUE(IPC::ReadParam(&message, &iter, &request_id));
  ASSERT_TRUE(IPC::ReadParam(&message, &iter, &error_code));
  ASSERT_EQ(expected_error_code, error_code);
}

class ResourceDispatcherHostTest : public testing::Test, public IPC::Sender {
 public:
  typedef ResourceDispatcherHostImpl::LoadInfo LoadInfo;
  typedef ResourceDispatcherHostImpl::LoadInfoList LoadInfoList;
  typedef ResourceDispatcherHostImpl::LoadInfoMap LoadInfoMap;

  ResourceDispatcherHostTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        host_(base::Bind(&DownloadResourceHandler::Create),
              base::ThreadTaskRunnerHandle::Get()),
        use_test_ssl_certificate_(false),
        send_data_received_acks_(false),
        auto_advance_(false) {
    host_.SetLoaderDelegate(&loader_delegate_);
    browser_context_.reset(new TestBrowserContext());
    BrowserContext::EnsureResourceContextInitialized(browser_context_.get());
    content::RunAllTasksUntilIdle();

    filter_ = MakeForwardingFilter();
    // TODO(cbentzel): Better way to get URLRequestContext?
    net::URLRequestContext* request_context =
        browser_context_->GetResourceContext()->GetRequestContext();
    job_factory_.reset(new TestURLRequestJobFactory(this));
    request_context->set_job_factory(job_factory_.get());
    request_context->set_network_delegate(&network_delegate_);
  }

  ~ResourceDispatcherHostTest() override {
    filter_->OnChannelClosing();
    filter_ = nullptr;
    web_contents_filter_ = nullptr;
  }

  // IPC::Sender implementation
  bool Send(IPC::Message* msg) override {
    accum_.AddMessage(*msg);

    if (send_data_received_acks_ &&
        msg->type() == ResourceMsg_DataReceived::ID) {
      GenerateDataReceivedACK(*msg);
    }

    if (wait_for_request_complete_loop_ &&
        msg->type() == ResourceMsg_RequestComplete::ID) {
      wait_for_request_complete_loop_->Quit();
    }

    // Do not release handles in it yet; the accumulator owns them now.
    delete msg;
    return true;
  }

 protected:
  friend class TestURLRequestJobFactory;

  // testing::Test
  void SetUp() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Add(0);
    HandleScheme("test");
    scoped_refptr<SiteInstance> site_instance =
        SiteInstance::Create(browser_context_.get());
    web_contents_.reset(
        WebContents::Create(WebContents::CreateParams(browser_context_.get())));
    web_contents_observer_.reset(
        new TestWebContentsObserver(web_contents_.get()));
    web_contents_filter_ = new TestFilterSpecifyingChild(
        browser_context_->GetResourceContext(),
        web_contents_->GetMainFrame()->GetProcess()->GetID());
    child_ids_.insert(web_contents_->GetMainFrame()->GetProcess()->GetID());
    request_context_getter_ = new net::TestURLRequestContextGetter(
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::UI));
  }

  void TearDown() override {
    web_contents_filter_->OnChannelClosing();
    web_contents_observer_.reset();
    web_contents_.reset();

    EXPECT_TRUE(URLRequestTestDelayedStartJob::DelayedStartQueueEmpty());
    URLRequestTestDelayedStartJob::ClearQueue();

    for (std::set<int>::iterator it = child_ids_.begin();
         it != child_ids_.end(); ++it) {
      host_.CancelRequestsForProcess(*it);
    }

    host_.Shutdown();

    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(0);

    // Flush the message loop to make application verifiers happy.
    if (ResourceDispatcherHostImpl::Get())
      ResourceDispatcherHostImpl::Get()->CancelRequestsForContext(
          browser_context_->GetResourceContext());

    browser_context_.reset();
    content::RunAllTasksUntilIdle();
  }

  // Creates a new ForwardingFilter and registers it with |child_ids_| so as not
  // to leak per-child state on test shutdown.
  ForwardingFilter* MakeForwardingFilter() {
    ForwardingFilter* filter =
        new ForwardingFilter(this, browser_context_->GetResourceContext());
    child_ids_.insert(filter->child_id());
    return filter;
  }

  // Creates a request using the current test object as the filter and
  // SubResource as the resource type.
  void MakeTestRequest(int render_view_id,
                       int request_id,
                       const GURL& url);

  void MakeTestRequestWithRenderFrame(int render_view_id,
                                      int render_frame_id,
                                      int request_id,
                                      const GURL& url,
                                      ResourceType type);

  // Generates a request using the given filter and resource type.
  void MakeTestRequestWithResourceType(ResourceMessageFilter* filter,
                                       int render_view_id,
                                       int request_id,
                                       const GURL& url,
                                       ResourceType type);

  void MakeWebContentsAssociatedTestRequest(int request_id, const GURL& url);
  void MakeWebContentsAssociatedTestRequestWithResourceType(int request_id,
                                                            const GURL& url,
                                                            ResourceType type);

  // Generates a request with the given priority.
  void MakeTestRequestWithPriority(int render_view_id,
                                   int request_id,
                                   net::RequestPriority priority);

  void MakeTestRequestWithPriorityAndRenderFrame(int render_view_id,
                                                 int render_frame_id,
                                                 int request_id,
                                                 net::RequestPriority priority);

  void MakeWebContentsAssociatedDownloadRequest(int request_id,
                                                const GURL& url);

  void CancelRequest(int request_id);
  void RendererCancelRequest(int request_id) {
    filter_->OnMessageReceived(ResourceHostMsg_CancelRequest(request_id));
  }

  void CompleteStartRequest(int request_id);
  void CompleteStartRequest(ResourceMessageFilter* filter, int request_id);

  net::TestNetworkDelegate* network_delegate() { return &network_delegate_; }

  void EnsureSchemeIsAllowed(const std::string& scheme) {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    if (!policy->IsWebSafeScheme(scheme))
      policy->RegisterWebSafeScheme(scheme);
  }

  // Sets a particular response for any request from now on. To switch back to
  // the default bahavior, pass an empty |headers|. |headers| should be CR[LF]
  // terminated.
  void SetResponse(const std::string& headers, const std::string& data) {
    response_headers_ = headers;
    response_data_ = data;
  }
  void SetResponse(const std::string& headers) {
    SetResponse(headers, std::string());
  }

  // If called, requests called from now on will be created as
  // TestHTTPSURLRequestJobs: that is, a test certificate will be set on
  // the |ssl_info| field of the response.
  void SetTestSSLCertificate() { use_test_ssl_certificate_ = true; }

  void SendDataReceivedACKs(bool send_acks) {
    send_data_received_acks_ = send_acks;
  }

  // Intercepts requests for the given protocol.
  void HandleScheme(const std::string& scheme) {
    job_factory_->HandleScheme(scheme);
    EnsureSchemeIsAllowed(scheme);
  }

  void GenerateDataReceivedACK(const IPC::Message& msg) {
    EXPECT_EQ(ResourceMsg_DataReceived::ID, msg.type());

    int request_id = -1;
    bool result = base::PickleIterator(msg).ReadInt(&request_id);
    DCHECK(result);
    std::unique_ptr<IPC::Message> ack(
        new ResourceHostMsg_DataReceived_ACK(request_id));

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&GenerateIPCMessage,
                                  scoped_refptr<ResourceRequesterInfo>(
                                      filter_->requester_info_for_test()),
                                  base::Passed(&ack)));
  }

  void WaitForRequestComplete() {
    DCHECK(!wait_for_request_complete_loop_);
    wait_for_request_complete_loop_.reset(new base::RunLoop);
    wait_for_request_complete_loop_->Run();
    wait_for_request_complete_loop_.reset();
  }

  void DeleteRenderFrame(const GlobalFrameRoutingId& global_routing_id) {
    host_.OnRenderFrameDeleted(global_routing_id);
  }

  // Creates and drives a main resource request until completion. Then asserts
  // that the expected_error_code has been emitted for the request.
  void CompleteFailingMainResourceRequest(const GURL& url,
                                          int expected_error_code) {
    if (IsBrowserSideNavigationEnabled()) {
      auto_advance_ = true;

      // Make a navigation request.
      TestNavigationURLLoaderDelegate delegate;
      BeginNavigationParams begin_params(
          std::string(), net::LOAD_NORMAL, false, false,
          REQUEST_CONTEXT_TYPE_LOCATION,
          blink::WebMixedContentContextType::kBlockable,
          false,  // is_form_submission
          url::Origin(url));
      CommonNavigationParams common_params;
      common_params.url = url;
      std::unique_ptr<NavigationRequestInfo> request_info(
          new NavigationRequestInfo(common_params, begin_params, url, true,
                                    false, false, -1, false, false,
                                    blink::kWebPageVisibilityStateVisible));
      std::unique_ptr<NavigationURLLoader> test_loader =
          NavigationURLLoader::Create(
              browser_context_->GetResourceContext(),
              BrowserContext::GetDefaultStoragePartition(
                  browser_context_.get()),
              std::move(request_info), nullptr, nullptr, nullptr, &delegate);

      // The navigation should fail with the expected error code.
      delegate.WaitForRequestFailed();
      ASSERT_EQ(expected_error_code, delegate.net_error());
      return;
    }

    MakeTestRequestWithResourceType(filter_.get(), 0, 1, url,
                                    RESOURCE_TYPE_MAIN_FRAME);

    // Flush all pending requests.
    while (net::URLRequestTestJob::ProcessOnePendingMessage()) {
    }
    content::RunAllTasksUntilIdle();

    // Sorts out all the messages we saw by request.
    ResourceIPCAccumulator::ClassifiedMessages msgs;
    accum_.GetClassifiedMessages(&msgs);

    // We should have gotten one RequestComplete message.
    ASSERT_EQ(1U, msgs.size());
    ASSERT_EQ(1U, msgs[0].size());
    EXPECT_EQ(ResourceMsg_RequestComplete::ID, msgs[0][0].type());

    // The RequestComplete message should have had the expected error code.
    CheckRequestCompleteErrorCode(msgs[0][0], expected_error_code);
  }

  void OnMessageReceived(const IPC::Message& message,
                         ResourceMessageFilter* filter) {
    host_.OnMessageReceived(message, filter->requester_info_for_test());
  }

  ResourceRequesterInfo* GetResourceRequesterInfo(
      ResourceMessageFilter* filter) {
    return filter->requester_info_for_test();
  }

  bool IsDetached(net::URLRequest* request) {
    auto* request_info = ResourceRequestInfoImpl::ForRequest(request);
    if (!request_info)
      return false;
    return request_info->detachable_handler() &&
           request_info->detachable_handler()->is_detached();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestURLRequestJobFactory> job_factory_;
  std::unique_ptr<WebContents> web_contents_;
  std::unique_ptr<TestWebContentsObserver> web_contents_observer_;
  scoped_refptr<ForwardingFilter> filter_;
  scoped_refptr<TestFilterSpecifyingChild> web_contents_filter_;
  net::TestNetworkDelegate network_delegate_;
  LoaderDelegateImpl loader_delegate_;
  ResourceDispatcherHostImpl host_;
  ResourceIPCAccumulator accum_;
  std::string response_headers_;
  std::string response_data_;
  bool use_test_ssl_certificate_;
  std::string scheme_;
  bool send_data_received_acks_;
  std::set<int> child_ids_;
  std::unique_ptr<base::RunLoop> wait_for_request_complete_loop_;
  RenderViewHostTestEnabler render_view_host_test_enabler_;
  bool auto_advance_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
};

void ResourceDispatcherHostTest::MakeTestRequest(int render_view_id,
                                                 int request_id,
                                                 const GURL& url) {
  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  url, RESOURCE_TYPE_SUB_RESOURCE);
}

void ResourceDispatcherHostTest::MakeTestRequestWithRenderFrame(
    int render_view_id,
    int render_frame_id,
    int request_id,
    const GURL& url,
    ResourceType type) {
  ResourceRequest request = CreateResourceRequest("GET", type, url);
  request.render_frame_id = render_frame_id;
  ResourceHostMsg_RequestResource msg(
      render_view_id, request_id, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(msg, filter_.get());
  KickOffRequest();
}

void ResourceDispatcherHostTest::MakeTestRequestWithResourceType(
    ResourceMessageFilter* filter,
    int render_view_id,
    int request_id,
    const GURL& url,
    ResourceType type) {
  ResourceRequest request = CreateResourceRequest("GET", type, url);
  ResourceHostMsg_RequestResource msg(
      render_view_id, request_id, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(msg, filter);
  KickOffRequest();
}

void ResourceDispatcherHostTest::MakeWebContentsAssociatedTestRequest(
    int request_id,
    const GURL& url) {
  MakeWebContentsAssociatedTestRequestWithResourceType(
      request_id, url, RESOURCE_TYPE_SUB_RESOURCE);
}

void ResourceDispatcherHostTest::
    MakeWebContentsAssociatedTestRequestWithResourceType(int request_id,
                                                         const GURL& url,
                                                         ResourceType type) {
  ResourceRequest request = CreateResourceRequest("GET", type, url);
  request.origin_pid = web_contents_->GetMainFrame()->GetProcess()->GetID();
  request.render_frame_id = web_contents_->GetMainFrame()->GetRoutingID();
  ResourceHostMsg_RequestResource msg(
      web_contents_->GetRenderViewHost()->GetRoutingID(), request_id, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(msg, web_contents_filter_.get());
  KickOffRequest();
}

void ResourceDispatcherHostTest::MakeTestRequestWithPriority(
    int render_view_id,
    int request_id,
    net::RequestPriority priority) {
  MakeTestRequestWithPriorityAndRenderFrame(render_view_id, -1, request_id,
                                            priority);
}

void ResourceDispatcherHostTest::MakeTestRequestWithPriorityAndRenderFrame(
    int render_view_id,
    int render_frame_id,
    int request_id,
    net::RequestPriority priority) {
  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_SUB_RESOURCE, GURL("http://example.com/priority"));
  request.render_frame_id = render_frame_id;
  request.priority = priority;
  ResourceHostMsg_RequestResource msg(
      render_view_id, request_id, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(msg, filter_.get());
}

void ResourceDispatcherHostTest::MakeWebContentsAssociatedDownloadRequest(
    int request_id,
    const GURL& url) {
  net::URLRequestContext* request_context =
      browser_context_->GetResourceContext()->GetRequestContext();
  std::unique_ptr<net::URLRequest> request(request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, NULL, TRAFFIC_ANNOTATION_FOR_TESTS));
  DownloadManagerImpl::BeginDownloadRequest(
      std::move(request), Referrer(), browser_context_->GetResourceContext(),
      false,  // is_content_initiated
      web_contents_->GetRenderViewHost()->GetProcess()->GetID(),
      web_contents_->GetRenderViewHost()->GetRoutingID(),
      web_contents_->GetMainFrame()->GetRoutingID(), false);
}

void ResourceDispatcherHostTest::CancelRequest(int request_id) {
  host_.CancelRequest(filter_->child_id(), request_id);
}

void ResourceDispatcherHostTest::CompleteStartRequest(int request_id) {
  CompleteStartRequest(filter_.get(), request_id);
}

void ResourceDispatcherHostTest::CompleteStartRequest(
    ResourceMessageFilter* filter,
    int request_id) {
  GlobalRequestID gid(filter->child_id(), request_id);
  net::URLRequest* req = host_.GetURLRequest(gid);
  EXPECT_TRUE(req);
  if (req)
    URLRequestTestDelayedStartJob::CompleteStart(req);
}

testing::AssertionResult ExtractDataOffsetAndLength(const IPC::Message& message,
                                                    int* data_offset,
                                                    int* data_length) {
  base::PickleIterator iter(message);
  int request_id;
  if (!IPC::ReadParam(&message, &iter, &request_id))
    return testing::AssertionFailure() << "Could not read request_id";
  if (!IPC::ReadParam(&message, &iter, data_offset))
    return testing::AssertionFailure() << "Could not read data_offset";
  if (!IPC::ReadParam(&message, &iter, data_length))
    return testing::AssertionFailure() << "Could not read data_length";
  return testing::AssertionSuccess();
}

void CheckSuccessfulRequestWithErrorCode(
    const std::vector<IPC::Message>& messages,
    const std::string& reference_data,
    int expected_error) {
  ASSERT_LT(2U, messages.size());

  // A successful request will have received 4 messages:
  //     ReceivedResponse    (indicates headers received)
  //     SetDataBuffer       (contains shared memory handle)
  //     DataReceived        (data offset and length into shared memory)
  //     RequestComplete     (request is done)
  //
  // This function verifies that we received 4 messages and that they are
  // appropriate. It allows for an error code other than net::OK if the request
  // should successfully receive data and then abort, e.g., on cancel.
  ASSERT_EQ(4U, messages.size());

  // The first messages should be received response
  ASSERT_EQ(ResourceMsg_ReceivedResponse::ID, messages[0].type());

  ASSERT_EQ(ResourceMsg_SetDataBuffer::ID, messages[1].type());

  base::PickleIterator iter(messages[1]);
  int request_id;
  ASSERT_TRUE(IPC::ReadParam(&messages[1], &iter, &request_id));
  base::SharedMemoryHandle shm_handle;
  ASSERT_TRUE(IPC::ReadParam(&messages[1], &iter, &shm_handle));
  int shm_size;
  ASSERT_TRUE(IPC::ReadParam(&messages[1], &iter, &shm_size));

  // Followed by the data, currently we only do the data in one chunk, but
  // should probably test multiple chunks later
  ASSERT_EQ(ResourceMsg_DataReceived::ID, messages[2].type());

  int data_offset;
  int data_length;
  ASSERT_TRUE(
      ExtractDataOffsetAndLength(messages[2], &data_offset, &data_length));

  ASSERT_EQ(reference_data.size(), static_cast<size_t>(data_length));
  ASSERT_GE(shm_size, data_length);

  base::SharedMemory shared_mem(shm_handle, true);  // read only
  shared_mem.Map(data_length);
  const char* data = static_cast<char*>(shared_mem.memory()) + data_offset;
  ASSERT_EQ(0, memcmp(reference_data.c_str(), data, data_length));

  // The last message should be all data received.
  CheckRequestCompleteErrorCode(messages[3], expected_error);
}

void CheckSuccessfulRequest(const std::vector<IPC::Message>& messages,
                            const std::string& reference_data) {
  CheckSuccessfulRequestWithErrorCode(messages, reference_data, net::OK);
}

void CheckSuccessfulRedirect(const std::vector<IPC::Message>& messages,
                             const std::string& reference_data) {
  ASSERT_EQ(5U, messages.size());
  ASSERT_EQ(ResourceMsg_ReceivedRedirect::ID, messages[0].type());

  const std::vector<IPC::Message> second_req_msgs =
      std::vector<IPC::Message>(messages.begin() + 1, messages.end());
  CheckSuccessfulRequest(second_req_msgs, reference_data);
}

void CheckFailedRequest(const std::vector<IPC::Message>& messages,
                        const std::string& reference_data,
                        int expected_error) {
  ASSERT_LT(0U, messages.size());
  ASSERT_GE(2U, messages.size());
  size_t failure_index = messages.size() - 1;

  if (messages.size() == 2) {
    EXPECT_EQ(ResourceMsg_ReceivedResponse::ID, messages[0].type());
  }

  CheckRequestCompleteErrorCode(messages[failure_index], expected_error);
}

// Tests whether many messages get dispatched properly.
TEST_F(ResourceDispatcherHostTest, TestMany) {
  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 2, net::URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 3, net::URLRequestTestJob::test_url_3());
  MakeTestRequestWithResourceType(filter_.get(), 0, 4,
                                  net::URLRequestTestJob::test_url_4(),
                                  RESOURCE_TYPE_PREFETCH);  // detachable type
  MakeTestRequest(0, 5, net::URLRequestTestJob::test_url_redirect_to_url_2());

  // Finish the redirection
  ResourceHostMsg_FollowRedirect redirect_msg(5);
  OnMessageReceived(redirect_msg, filter_.get());
  content::RunAllTasksUntilIdle();

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // sorts out all the messages we saw by request
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // there are five requests, so we should have gotten them classified as such
  ASSERT_EQ(5U, msgs.size());

  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[2], net::URLRequestTestJob::test_data_3());
  CheckSuccessfulRequest(msgs[3], net::URLRequestTestJob::test_data_4());
  CheckSuccessfulRedirect(msgs[4], net::URLRequestTestJob::test_data_2());
}

// Tests whether messages get canceled properly. We issue four requests,
// cancel two of them, and make sure that each sent the proper notifications.
TEST_F(ResourceDispatcherHostTest, Cancel) {
  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 2, net::URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 3, net::URLRequestTestJob::test_url_3());

  MakeTestRequestWithResourceType(filter_.get(), 0, 4,
                                  net::URLRequestTestJob::test_url_4(),
                                  RESOURCE_TYPE_PREFETCH);  // detachable type

  CancelRequest(2);

  // Cancel request must come from the renderer for a detachable resource to
  // delay.
  RendererCancelRequest(4);

  // The handler should have been detached now.
  GlobalRequestID global_request_id(filter_->child_id(), 4);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  ASSERT_TRUE(info->detachable_handler()->is_detached());

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  // Everything should be out now.
  EXPECT_EQ(0, host_.pending_requests());

  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // there are four requests, so we should have gotten them classified as such
  ASSERT_EQ(4U, msgs.size());

  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[2], net::URLRequestTestJob::test_data_3());

  // Check that request 2 and 4 got canceled, as far as the renderer is
  // concerned.  Request 2 will have been deleted.
  ASSERT_EQ(1U, msgs[1].size());
  ASSERT_EQ(ResourceMsg_ReceivedResponse::ID, msgs[1][0].type());

  ASSERT_EQ(2U, msgs[3].size());
  ASSERT_EQ(ResourceMsg_ReceivedResponse::ID, msgs[3][0].type());
  CheckRequestCompleteErrorCode(msgs[3][1], net::ERR_ABORTED);

  // However, request 4 should have actually gone to completion. (Only request 2
  // was canceled.)
  EXPECT_EQ(4, network_delegate()->completed_requests());
  EXPECT_EQ(1, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

TEST_F(ResourceDispatcherHostTest, DownloadToNetworkCache) {
  // Normal request.
  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_2());

  // Cache-only request.
  ResourceRequest request_to_cache = CreateResourceRequest(
      "GET", RESOURCE_TYPE_IMAGE, net::URLRequestTestJob::test_url_3());
  request_to_cache.download_to_network_cache_only = true;
  ResourceHostMsg_RequestResource msg_to_cache(
      0, 2, request_to_cache,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(msg_to_cache, filter_.get());

  KickOffRequest();

  // The handler for the cache-only request should have been detached now.
  GlobalRequestID global_request_id(filter_->child_id(), 2);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  ASSERT_TRUE(info->detachable_handler()->is_detached());

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {
  }
  content::RunAllTasksUntilIdle();

  // Everything should be out now.
  EXPECT_EQ(0, host_.pending_requests());

  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // There are two requests, so we should have gotten them classified as such.
  ASSERT_EQ(2U, msgs.size());

  // The normal request succeeded.
  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_2());

  // The cache-only request got canceled, as far as the renderer is concerned.
  ASSERT_EQ(1U, msgs[1].size());
  CheckRequestCompleteErrorCode(msgs[1][0], net::ERR_ABORTED);

  // However, all requests should have actually gone to completion.
  EXPECT_EQ(2, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

// Shows that detachable requests will timeout if the request takes too long to
// complete.
TEST_F(ResourceDispatcherHostTest, DetachedResourceTimesOut) {
  MakeTestRequestWithResourceType(filter_.get(), 0, 1,
                                  net::URLRequestTestJob::test_url_2(),
                                  RESOURCE_TYPE_PREFETCH);  // detachable type
  GlobalRequestID global_request_id(filter_->child_id(), 1);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  ASSERT_TRUE(info->detachable_handler());
  info->detachable_handler()->set_cancel_delay(
      base::TimeDelta::FromMilliseconds(200));
  content::RunAllTasksUntilIdle();

  RendererCancelRequest(1);

  // From the renderer's perspective, the request was cancelled.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());
  ASSERT_EQ(2U, msgs[0].size());
  ASSERT_EQ(ResourceMsg_ReceivedResponse::ID, msgs[0][0].type());
  CheckRequestCompleteErrorCode(msgs[0][1], net::ERR_ABORTED);

  // But it continues detached.
  EXPECT_EQ(1, host_.pending_requests());
  EXPECT_TRUE(info->detachable_handler()->is_detached());

  // Wait until after the delay timer times out before we start processing any
  // messages.
  base::OneShotTimer timer;
  base::RunLoop run_loop;
  timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(210),
              run_loop.QuitWhenIdleClosure());
  run_loop.Run();

  // The prefetch should be cancelled by now.
  EXPECT_EQ(0, host_.pending_requests());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(1, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

TEST_F(ResourceDispatcherHostTest, SyncLoadSuccess) {
  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_XHR, net::URLRequestTestJob::test_url_1());
  request.priority = net::MAXIMUM_PRIORITY;

  // Successful sync load.
  std::tuple<SyncLoadResult> result;
  ResourceHostMsg_SyncLoad sync_load_msg(0, 1, request, &std::get<0>(result));
  OnMessageReceived(sync_load_msg, filter_.get());
  content::RunAllTasksUntilIdle();

  const IPC::Message* reply = accum_.GetReply(sync_load_msg);
  ASSERT_TRUE(reply);

  ASSERT_TRUE(ResourceHostMsg_SyncLoad::ReadReplyParam(reply, &result));
  EXPECT_EQ(net::OK, std::get<0>(result).error_code);
}

TEST_F(ResourceDispatcherHostTest, SyncLoadError) {
  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_XHR, net::URLRequestTestJob::test_url_error());
  request.priority = net::MAXIMUM_PRIORITY;

  // Failued sync load.
  std::tuple<SyncLoadResult> result;
  ResourceHostMsg_SyncLoad sync_load_msg(0, 1, request, &std::get<0>(result));
  OnMessageReceived(sync_load_msg, filter_.get());
  content::RunAllTasksUntilIdle();

  const IPC::Message* reply = accum_.GetReply(sync_load_msg);
  ASSERT_TRUE(reply);

  ASSERT_TRUE(ResourceHostMsg_SyncLoad::ReadReplyParam(reply, &result));
  EXPECT_EQ(net::ERR_INVALID_URL, std::get<0>(result).error_code);
}

TEST_F(ResourceDispatcherHostTest, SyncLoadCancel) {
  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_XHR, net::URLRequestTestJob::test_url_1());
  request.priority = net::MAXIMUM_PRIORITY;

  // Cancelled sync load.
  SyncLoadResult result;
  ResourceHostMsg_SyncLoad sync_load_msg(0, 1, request, &result);

  OnMessageReceived(sync_load_msg, filter_.get());
  host_.CancelRequestsForProcess(filter_->child_id());
  content::RunAllTasksUntilIdle();

  const IPC::Message* reply = accum_.GetReply(sync_load_msg);
  ASSERT_TRUE(reply);
  ASSERT_TRUE(reply->is_reply_error());
}

// If the filter has disappeared then detachable resources should continue to
// load.
// RESOURCE_TYPE_PING requests Handling is affected by
// "KeepAliveRendererForKeepaliveRequests" feature. We keep this test so that
// we can unship the feature easily if needed.
// TODO(yhirano): Add a corresponding test case with the feature.
TEST_F(ResourceDispatcherHostTest, DeletedFilterDetached) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kKeepAliveRendererForKeepaliveRequests);
  // test_url_1's data is available synchronously, so use 2 and 3.
  ResourceRequest request_prefetch = CreateResourceRequest(
      "GET", RESOURCE_TYPE_PREFETCH, net::URLRequestTestJob::test_url_2());
  ResourceRequest request_ping = CreateResourceRequest(
      "GET", RESOURCE_TYPE_PING, net::URLRequestTestJob::test_url_3());

  ResourceHostMsg_RequestResource msg_prefetch(
      0, 1, request_prefetch,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(msg_prefetch, filter_.get());
  ResourceHostMsg_RequestResource msg_ping(
      0, 2, request_ping,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(msg_ping, filter_.get());

  // Remove the filter before processing the requests by simulating channel
  // closure.
  ResourceRequestInfoImpl* info_prefetch = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 1)));
  ResourceRequestInfoImpl* info_ping = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 2)));
  DCHECK_EQ(filter_.get(), info_prefetch->requester_info()->filter());
  DCHECK_EQ(filter_.get(), info_ping->requester_info()->filter());
  filter_->OnChannelClosing();

  // From the renderer's perspective, the requests were cancelled.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(2U, msgs.size());
  CheckRequestCompleteErrorCode(msgs[0][0], net::ERR_ABORTED);
  CheckRequestCompleteErrorCode(msgs[1][0], net::ERR_ABORTED);

  // But it continues detached.
  EXPECT_EQ(2, host_.pending_requests());
  EXPECT_TRUE(info_prefetch->detachable_handler()->is_detached());
  EXPECT_TRUE(info_ping->detachable_handler()->is_detached());

  KickOffRequest();

  // Make sure the requests weren't canceled early.
  EXPECT_EQ(2, host_.pending_requests());

  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(0, host_.pending_requests());
  EXPECT_EQ(2, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

// If the filter has disappeared (original process dies) then detachable
// resources should continue to load, even when redirected.
TEST_F(ResourceDispatcherHostTest, DeletedFilterDetachedRedirect) {
  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_PREFETCH,
      net::URLRequestTestJob::test_url_redirect_to_url_2());

  ResourceHostMsg_RequestResource msg(
      0, 1, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(msg, filter_.get());

  // Remove the filter before processing the request by simulating channel
  // closure.
  GlobalRequestID global_request_id(filter_->child_id(), 1);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  info->requester_info()->filter()->OnChannelClosing();

  // From the renderer's perspective, the request was cancelled.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());
  CheckRequestCompleteErrorCode(msgs[0][0], net::ERR_ABORTED);

  // But it continues detached.
  EXPECT_EQ(1, host_.pending_requests());
  EXPECT_TRUE(info->detachable_handler()->is_detached());

  // Verify no redirects before resetting the filter.
  net::URLRequest* url_request = host_.GetURLRequest(global_request_id);
  EXPECT_EQ(1u, url_request->url_chain().size());
  KickOffRequest();

  // Verify that a redirect was followed.
  EXPECT_EQ(2u, url_request->url_chain().size());

  // Make sure the request wasn't canceled early.
  EXPECT_EQ(1, host_.pending_requests());

  // Finish up the request.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(0, host_.pending_requests());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

TEST_F(ResourceDispatcherHostTest, CancelWhileStartIsDeferred) {
  bool was_deleted = false;

  // Arrange to have requests deferred before starting.
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(DEFER_STARTING_REQUEST);
  delegate.set_url_request_user_data(new TestUserData(&was_deleted));
  host_.SetDelegate(&delegate);

  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1());
  // We cancel from the renderer because all non-renderer cancels delete
  // the request synchronously.
  RendererCancelRequest(1);

  // Our TestResourceThrottle should not have been deleted yet.  This is to
  // ensure that destruction of the URLRequest happens asynchronously to
  // calling CancelRequest.
  EXPECT_FALSE(was_deleted);

  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(was_deleted);
}

TEST_F(ResourceDispatcherHostTest, DetachWhileStartIsDeferred) {
  bool was_deleted = false;

  // Arrange to have requests deferred before starting.
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(DEFER_STARTING_REQUEST);
  delegate.set_url_request_user_data(new TestUserData(&was_deleted));
  host_.SetDelegate(&delegate);

  MakeTestRequestWithResourceType(filter_.get(), 0, 1,
                                  net::URLRequestTestJob::test_url_1(),
                                  RESOURCE_TYPE_PREFETCH);  // detachable type
  // Cancel request must come from the renderer for a detachable resource to
  // detach.
  RendererCancelRequest(1);

  // Even after driving the event loop, the request has not been deleted.
  EXPECT_FALSE(was_deleted);

  // However, it is still throttled because the defer happened above the
  // DetachableResourceHandler.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(was_deleted);

  // Resume the request.
  GenericResourceThrottle* throttle =
      GenericResourceThrottle::active_throttle();
  ASSERT_TRUE(throttle);
  throttle->AssertAndResume();

  // Now, the request completes.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(was_deleted);
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

// Tests if cancel is called in ResourceThrottle::WillStartRequest, then the
// URLRequest will not be started.
TEST_F(ResourceDispatcherHostTest, CancelInResourceThrottleWillStartRequest) {
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(CANCEL_BEFORE_START);
  host_.SetDelegate(&delegate);

  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1());

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // Check that request got canceled.
  ASSERT_EQ(1U, msgs[0].size());
  CheckRequestCompleteErrorCode(msgs[0][0], net::ERR_ABORTED);

  // Make sure URLRequest is never started.
  EXPECT_EQ(0, job_factory_->url_request_jobs_created_count());
}

TEST_F(ResourceDispatcherHostTest, PausedStartError) {
  // Arrange to have requests deferred before processing response headers.
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(DEFER_PROCESSING_RESPONSE);
  host_.SetDelegate(&delegate);

  job_factory_->SetDelayedStartJobGeneration(true);
  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_error());
  CompleteStartRequest(1);

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(0, host_.pending_requests());
}

TEST_F(ResourceDispatcherHostTest, ThrottleAndResumeTwice) {
  // Arrange to have requests deferred before starting.
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(DEFER_STARTING_REQUEST);
  delegate.set_create_two_throttles(true);
  host_.SetDelegate(&delegate);

  // Make sure the first throttle blocked the request, and then resume.
  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1());
  GenericResourceThrottle* first_throttle =
      GenericResourceThrottle::active_throttle();
  ASSERT_TRUE(first_throttle);
  first_throttle->AssertAndResume();

  // Make sure the second throttle blocked the request, and then resume.
  ASSERT_TRUE(GenericResourceThrottle::active_throttle());
  ASSERT_NE(first_throttle, GenericResourceThrottle::active_throttle());
  GenericResourceThrottle::active_throttle()->AssertAndResume();

  ASSERT_FALSE(GenericResourceThrottle::active_throttle());

  // The request is started asynchronously.
  content::RunAllTasksUntilIdle();

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.pending_requests());

  // Make sure the request completed successfully.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());
  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
}


// Tests that the delegate can cancel a request and provide a error code.
TEST_F(ResourceDispatcherHostTest, CancelInDelegate) {
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(CANCEL_BEFORE_START);
  delegate.set_error_code_for_cancellation(net::ERR_ACCESS_DENIED);
  host_.SetDelegate(&delegate);

  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1());
  // The request will get cancelled by the throttle.

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // Check the cancellation
  ASSERT_EQ(1U, msgs.size());
  ASSERT_EQ(1U, msgs[0].size());

  CheckRequestCompleteErrorCode(msgs[0][0], net::ERR_ACCESS_DENIED);
}

// RESOURCE_TYPE_PING requests Handling is affected by
// "KeepAliveRendererForKeepaliveRequests" feature. We keep this test so that
// we can unship the feature easily if needed.
// TODO(yhirano): Add a corresponding test case with the feature.
TEST_F(ResourceDispatcherHostTest, CancelRequestsForRoute) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kKeepAliveRendererForKeepaliveRequests);
  job_factory_->SetDelayedStartJobGeneration(true);
  MakeTestRequestWithRenderFrame(0, 11, 1, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_XHR);
  EXPECT_EQ(1, host_.pending_requests());

  MakeTestRequestWithRenderFrame(0, 12, 2, net::URLRequestTestJob::test_url_2(),
                                 RESOURCE_TYPE_XHR);
  EXPECT_EQ(2, host_.pending_requests());

  MakeTestRequestWithRenderFrame(0, 11, 3, net::URLRequestTestJob::test_url_3(),
                                 RESOURCE_TYPE_PING);
  EXPECT_EQ(3, host_.pending_requests());

  MakeTestRequestWithRenderFrame(0, 12, 4, net::URLRequestTestJob::test_url_4(),
                                 RESOURCE_TYPE_PING);
  EXPECT_EQ(4, host_.pending_requests());

  EXPECT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 1)));
  EXPECT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 2)));
  EXPECT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 3)));
  EXPECT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 4)));

  host_.CancelRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 11));

  EXPECT_FALSE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 1)));
  EXPECT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 2)));
  ASSERT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 3)));
  ASSERT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 4)));

  EXPECT_TRUE(
      IsDetached(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 3))));
  EXPECT_FALSE(
      IsDetached(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 4))));

  CompleteStartRequest(2);
  CompleteStartRequest(3);
  CompleteStartRequest(4);

  while (host_.pending_requests() > 0) {
    while (net::URLRequestTestJob::ProcessOnePendingMessage()) {
    }
    content::RunAllTasksUntilIdle();
  }
}

// Tests CancelRequestsForProcess
TEST_F(ResourceDispatcherHostTest, TestProcessCancel) {
  scoped_refptr<TestFilter> test_filter = new TestFilter(
      browser_context_->GetResourceContext());
  child_ids_.insert(test_filter->child_id());

  // request 1 goes to the test delegate
  MakeTestRequestWithResourceType(test_filter.get(), 0, 1,
                                  net::URLRequestTestJob::test_url_1(),
                                  RESOURCE_TYPE_SUB_RESOURCE);

  // request 2 goes to us
  MakeTestRequest(0, 2, net::URLRequestTestJob::test_url_2());

  // request 3 goes to the test delegate
  MakeTestRequestWithResourceType(test_filter.get(), 0, 3,
                                  net::URLRequestTestJob::test_url_3(),
                                  RESOURCE_TYPE_SUB_RESOURCE);

  // request 4 goes to us
  MakeTestRequestWithResourceType(filter_.get(), 0, 4,
                                  net::URLRequestTestJob::test_url_4(),
                                  RESOURCE_TYPE_PREFETCH);  // detachable type


  // Make sure all requests have finished stage one. test_url_1 will have
  // finished.
  content::RunAllTasksUntilIdle();

  // TODO(mbelshe):
  // Now that the async IO path is in place, the IO always completes on the
  // initial call; so the requests have already completed.  This basically
  // breaks the whole test.
  // EXPECT_EQ(3, host_.pending_requests());

  // Process test_url_2 and test_url_3 for one level so one callback is called.
  // We'll cancel test_url_4 (detachable) before processing it to verify that it
  // delays the cancel.
  for (int i = 0; i < 2; i++)
    EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());

  // Cancel the requests to the test process.
  host_.CancelRequestsForProcess(filter_->child_id());
  test_filter->set_canceled(true);

  // The requests should all be cancelled, except request 4, which is detached.
  EXPECT_EQ(1, host_.pending_requests());
  GlobalRequestID global_request_id(filter_->child_id(), 4);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  ASSERT_TRUE(info->detachable_handler()->is_detached());

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.pending_requests());

  // The test delegate should not have gotten any messages after being canceled.
  ASSERT_EQ(0, test_filter->received_after_canceled());

  // There should be two results.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(2U, msgs.size());
  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_2());
  // The detachable request was cancelled by the renderer before it
  // finished. From the perspective of the renderer, it should have cancelled.
  ASSERT_EQ(2U, msgs[1].size());
  ASSERT_EQ(ResourceMsg_ReceivedResponse::ID, msgs[1][0].type());
  CheckRequestCompleteErrorCode(msgs[1][1], net::ERR_ABORTED);
  // But it completed anyway. For the network stack, no requests were canceled.
  EXPECT_EQ(4, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());

  test_filter->OnChannelClosing();
}

// Tests whether the correct requests get canceled when a RenderViewHost is
// deleted.
TEST_F(ResourceDispatcherHostTest, CancelRequestsOnRenderFrameDeleted) {
  // Requests all hang once started.  This prevents requests from being
  // destroyed due to completion.
  job_factory_->SetHangAfterStartJobGeneration(true);
  HandleScheme("http");

  TestResourceDispatcherHostDelegate delegate;
  host_.SetDelegate(&delegate);
  host_.OnRenderViewHostCreated(filter_->child_id(), 0,
                                request_context_getter_.get());

  // One RenderView issues a high priority request and a low priority one. Both
  // should be started.
  MakeTestRequestWithPriorityAndRenderFrame(0, 10, 1, net::HIGHEST);
  MakeTestRequestWithPriorityAndRenderFrame(0, 11, 2, net::LOWEST);
  KickOffRequest();
  EXPECT_EQ(2, network_delegate_.created_requests());
  EXPECT_EQ(0, network_delegate_.canceled_requests());

  // The same RenderView issues two more low priority requests. The
  // ResourceScheduler shouldn't let them start immediately.
  MakeTestRequestWithPriorityAndRenderFrame(0, 10, 3, net::LOWEST);
  MakeTestRequestWithPriorityAndRenderFrame(0, 11, 4, net::LOWEST);
  KickOffRequest();
  EXPECT_EQ(2, network_delegate_.created_requests());
  EXPECT_EQ(0, network_delegate_.canceled_requests());

  // Another RenderView in the same process as the old one issues a request,
  // which is then started.
  MakeTestRequestWithPriorityAndRenderFrame(1, 12, 5, net::LOWEST);
  KickOffRequest();
  EXPECT_EQ(3, network_delegate_.created_requests());
  EXPECT_EQ(0, network_delegate_.canceled_requests());

  // The first two RenderFrameHosts are destroyed.  All 4 of their requests
  // should be cancelled, and none of the two deferred requests should be
  // started.
  DeleteRenderFrame(GlobalFrameRoutingId(filter_->child_id(), 10));
  DeleteRenderFrame(GlobalFrameRoutingId(filter_->child_id(), 11));
  host_.OnRenderViewHostDeleted(filter_->child_id(), 0);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(3, network_delegate_.created_requests());
  EXPECT_EQ(4, network_delegate_.canceled_requests());

  // No messages should have been sent, since none of the jobs made any
  // progress.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  EXPECT_EQ(0U, msgs.size());
}

TEST_F(ResourceDispatcherHostTest, TestProcessCancelDetachedTimesOut) {
  MakeTestRequestWithResourceType(filter_.get(), 0, 1,
                                  net::URLRequestTestJob::test_url_4(),
                                  RESOURCE_TYPE_PREFETCH);  // detachable type
  GlobalRequestID global_request_id(filter_->child_id(), 1);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  ASSERT_TRUE(info->detachable_handler());
  info->detachable_handler()->set_cancel_delay(
      base::TimeDelta::FromMilliseconds(200));
  content::RunAllTasksUntilIdle();

  // Cancel the requests to the test process.
  host_.CancelRequestsForProcess(filter_->child_id());
  EXPECT_EQ(1, host_.pending_requests());

  // Wait until after the delay timer times out before we start processing any
  // messages.
  base::RunLoop run_loop;
  base::OneShotTimer timer;
  timer.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(210),
      base::Bind(&base::RunLoop::QuitWhenIdle, base::Unretained(&run_loop)));
  run_loop.Run();

  // The prefetch should be cancelled by now.
  EXPECT_EQ(0, host_.pending_requests());

  // In case any messages are still to be processed.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  ASSERT_EQ(1U, msgs.size());

  // The request should have cancelled.
  ASSERT_EQ(2U, msgs[0].size());
  ASSERT_EQ(ResourceMsg_ReceivedResponse::ID, msgs[0][0].type());
  CheckRequestCompleteErrorCode(msgs[0][1], net::ERR_ABORTED);
  // And not run to completion.
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(1, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

// Tests blocking and resuming requests.
TEST_F(ResourceDispatcherHostTest, TestBlockingResumingRequests) {
  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 11));
  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 12));
  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 13));

  MakeTestRequestWithRenderFrame(0, 10, 1, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithRenderFrame(1, 11, 2, net::URLRequestTestJob::test_url_2(),
                                 RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithRenderFrame(0, 10, 3, net::URLRequestTestJob::test_url_3(),
                                 RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithRenderFrame(1, 11, 4, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithRenderFrame(2, 12, 5, net::URLRequestTestJob::test_url_2(),
                                 RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithRenderFrame(3, 13, 6, net::URLRequestTestJob::test_url_3(),
                                 RESOURCE_TYPE_SUB_RESOURCE);

  // Flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // Sort out all the messages we saw by request
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // All requests but the 2 for the RFH 0 should have been blocked.
  ASSERT_EQ(2U, msgs.size());

  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_3());

  // Resume requests for RFH 11 and flush pending requests.
  host_.ResumeBlockedRequestsForRoute(
      GlobalFrameRoutingId(filter_->child_id(), 11));
  KickOffRequest();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(2U, msgs.size());
  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_1());

  // Test that new requests are not blocked for RFH 11.
  MakeTestRequestWithRenderFrame(1, 11, 7, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_SUB_RESOURCE);
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());
  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());

  // Now resumes requests for all RFH (12 and 13).
  host_.ResumeBlockedRequestsForRoute(
      GlobalFrameRoutingId(filter_->child_id(), 12));
  host_.ResumeBlockedRequestsForRoute(
      GlobalFrameRoutingId(filter_->child_id(), 13));
  KickOffRequest();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(2U, msgs.size());
  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_3());
}

// Tests blocking and canceling requests.
TEST_F(ResourceDispatcherHostTest, TestBlockingCancelingRequests) {
  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 11));

  MakeTestRequestWithRenderFrame(0, 10, 1, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithRenderFrame(1, 11, 2, net::URLRequestTestJob::test_url_2(),
                                 RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithRenderFrame(0, 10, 3, net::URLRequestTestJob::test_url_3(),
                                 RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithRenderFrame(1, 11, 4, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_SUB_RESOURCE);
  // Blocked detachable resources should not delay cancellation.
  //
  MakeTestRequestWithRenderFrame(1, 11, 5, net::URLRequestTestJob::test_url_4(),
                                 RESOURCE_TYPE_PREFETCH);  // detachable type

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // Sort out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // The 2 requests for the RFH 10 should have been processed.
  ASSERT_EQ(2U, msgs.size());

  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_3());

  // Cancel requests for RFH 11.
  host_.CancelBlockedRequestsForRoute(
      GlobalFrameRoutingId(filter_->child_id(), 11));
  KickOffRequest();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(0U, msgs.size());
}

// Tests that blocked requests are canceled if their associated process dies.
TEST_F(ResourceDispatcherHostTest, TestBlockedRequestsProcessDies) {
  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = MakeForwardingFilter();

  host_.BlockRequestsForRoute(
      GlobalFrameRoutingId(second_filter->child_id(), 0));

  MakeTestRequestWithResourceType(filter_.get(), 0, 1,
                                  net::URLRequestTestJob::test_url_1(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(second_filter.get(), 0, 2,
                                  net::URLRequestTestJob::test_url_2(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(filter_.get(), 0, 3,
                                  net::URLRequestTestJob::test_url_3(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(second_filter.get(), 0, 4,
                                  net::URLRequestTestJob::test_url_1(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(second_filter.get(), 0, 5,
                                  net::URLRequestTestJob::test_url_4(),
                                  RESOURCE_TYPE_PREFETCH);  // detachable type

  // Simulate process death.
  host_.CancelRequestsForProcess(second_filter->child_id());

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // Sort out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // The 2 requests for the RVH 0 should have been processed.  Note that
  // blocked detachable requests are canceled without delay.
  ASSERT_EQ(2U, msgs.size());

  CheckSuccessfulRequest(msgs[0], net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], net::URLRequestTestJob::test_data_3());

  EXPECT_TRUE(host_.blocked_loaders_map_.empty());
  second_filter->OnChannelClosing();
}

// Tests that blocked requests don't leak when the ResourceDispatcherHost goes
// away.  Note that we rely on Purify for finding the leaks if any.
// If this test turns the Purify bot red, check the ResourceDispatcherHost
// destructor to make sure the blocked requests are deleted.
TEST_F(ResourceDispatcherHostTest, TestBlockedRequestsDontLeak) {
  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = MakeForwardingFilter();

  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 1));
  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 2));
  host_.BlockRequestsForRoute(
      GlobalFrameRoutingId(second_filter->child_id(), 1));

  MakeTestRequestWithResourceType(filter_.get(), 0, 1,
                                  net::URLRequestTestJob::test_url_1(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(filter_.get(), 1, 2,
                                  net::URLRequestTestJob::test_url_2(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(filter_.get(), 0, 3,
                                  net::URLRequestTestJob::test_url_3(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(second_filter.get(), 1, 4,
                                  net::URLRequestTestJob::test_url_1(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(filter_.get(), 2, 5,
                                  net::URLRequestTestJob::test_url_2(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(filter_.get(), 2, 6,
                                  net::URLRequestTestJob::test_url_3(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(filter_.get(), 0, 7,
                                  net::URLRequestTestJob::test_url_4(),
                                  RESOURCE_TYPE_PREFETCH);  // detachable type
  MakeTestRequestWithResourceType(second_filter.get(), 1, 8,
                                  net::URLRequestTestJob::test_url_4(),
                                  RESOURCE_TYPE_PREFETCH);  // detachable type

  host_.CancelRequestsForProcess(filter_->child_id());
  host_.CancelRequestsForProcess(second_filter->child_id());

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  second_filter->OnChannelClosing();
}

// Test the private helper method "CalculateApproximateMemoryCost()".
TEST_F(ResourceDispatcherHostTest, CalculateApproximateMemoryCost) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> req(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, NULL,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(4425, ResourceDispatcherHostImpl::CalculateApproximateMemoryCost(
                      req.get()));

  // Add 9 bytes of referrer.
  req->SetReferrer("123456789");
  EXPECT_EQ(4434, ResourceDispatcherHostImpl::CalculateApproximateMemoryCost(
                      req.get()));

  // Add 33 bytes of upload content.
  std::string upload_content;
  upload_content.resize(33);
  std::fill(upload_content.begin(), upload_content.end(), 'x');
  std::unique_ptr<net::UploadElementReader> reader(
      new net::UploadBytesElementReader(upload_content.data(),
                                        upload_content.size()));
  req->set_upload(
      net::ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));

  // Since the upload throttling is disabled, this has no effect on the cost.
  EXPECT_EQ(4434, ResourceDispatcherHostImpl::CalculateApproximateMemoryCost(
                      req.get()));
}

// Test that too much memory for outstanding requests for a particular
// render_process_host_id causes requests to fail.
TEST_F(ResourceDispatcherHostTest, TooMuchOutstandingRequestsMemory) {
  // Expected cost of each request as measured by
  // ResourceDispatcherHost::CalculateApproximateMemoryCost().
  int kMemoryCostOfTest2Req =
      ResourceDispatcherHostImpl::kAvgBytesPerOutstandingRequest +
      net::URLRequestTestJob::test_url_2().spec().size() + sizeof("GET") - 1;

  // Tighten the bound on the ResourceDispatcherHost, to speed things up.
  int kMaxCostPerProcess = 440000;
  host_.set_max_outstanding_requests_cost_per_process(kMaxCostPerProcess);

  // Determine how many instance of test_url_2() we can request before
  // throttling kicks in.
  size_t kMaxRequests = kMaxCostPerProcess / kMemoryCostOfTest2Req;

  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = MakeForwardingFilter();

  // Saturate the number of outstanding requests for our process.
  for (size_t i = 0; i < kMaxRequests; ++i) {
    MakeTestRequestWithResourceType(filter_.get(), 0, i + 1,
                                    net::URLRequestTestJob::test_url_2(),
                                    RESOURCE_TYPE_SUB_RESOURCE);
  }

  // Issue two more requests for our process -- these should fail immediately.
  MakeTestRequestWithResourceType(filter_.get(), 0, kMaxRequests + 1,
                                  net::URLRequestTestJob::test_url_2(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(filter_.get(), 0, kMaxRequests + 2,
                                  net::URLRequestTestJob::test_url_2(),
                                  RESOURCE_TYPE_SUB_RESOURCE);

  // Issue two requests for the second process -- these should succeed since
  // it is just process 0 that is saturated.
  MakeTestRequestWithResourceType(second_filter.get(), 0, kMaxRequests + 3,
                                  net::URLRequestTestJob::test_url_2(),
                                  RESOURCE_TYPE_SUB_RESOURCE);
  MakeTestRequestWithResourceType(second_filter.get(), 0, kMaxRequests + 4,
                                  net::URLRequestTestJob::test_url_2(),
                                  RESOURCE_TYPE_SUB_RESOURCE);

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

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
    CheckFailedRequest(msgs[index], net::URLRequestTestJob::test_data_2(),
                       net::ERR_INSUFFICIENT_RESOURCES);
  }

  // The final 2 requests should have succeeded.
  CheckSuccessfulRequest(msgs[kMaxRequests + 2],
                         net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[kMaxRequests + 3],
                         net::URLRequestTestJob::test_data_2());

  second_filter->OnChannelClosing();
}

// Test that when too many requests are outstanding for a particular
// render_process_host_id, any subsequent request from it fails. Also verify
// that the global limit is honored.
TEST_F(ResourceDispatcherHostTest, TooManyOutstandingRequests) {
  // Tighten the bound on the ResourceDispatcherHost, to speed things up.
  const size_t kMaxRequestsPerProcess = 2;
  host_.set_max_num_in_flight_requests_per_process(kMaxRequestsPerProcess);
  const size_t kMaxRequests = 3;
  host_.set_max_num_in_flight_requests(kMaxRequests);

  // Needed to emulate additional processes.
  scoped_refptr<ForwardingFilter> second_filter = MakeForwardingFilter();
  scoped_refptr<ForwardingFilter> third_filter = MakeForwardingFilter();

  // Saturate the number of outstanding requests for our process.
  for (size_t i = 0; i < kMaxRequestsPerProcess; ++i) {
    MakeTestRequestWithResourceType(filter_.get(), 0, i + 1,
                                    net::URLRequestTestJob::test_url_2(),
                                    RESOURCE_TYPE_SUB_RESOURCE);
  }

  // Issue another request for our process -- this should fail immediately.
  MakeTestRequestWithResourceType(filter_.get(), 0, kMaxRequestsPerProcess + 1,
                                  net::URLRequestTestJob::test_url_2(),
                                  RESOURCE_TYPE_SUB_RESOURCE);

  // Issue a request for the second process -- this should succeed, because it
  // is just process 0 that is saturated.
  MakeTestRequestWithResourceType(
      second_filter.get(), 0, kMaxRequestsPerProcess + 2,
      net::URLRequestTestJob::test_url_2(), RESOURCE_TYPE_SUB_RESOURCE);

  // Issue a request for the third process -- this should fail, because the
  // global limit has been reached.
  MakeTestRequestWithResourceType(
      third_filter.get(), 0, kMaxRequestsPerProcess + 3,
      net::URLRequestTestJob::test_url_2(), RESOURCE_TYPE_SUB_RESOURCE);

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  // Sorts out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // The processes issued the following requests:
  // #1 issued kMaxRequestsPerProcess that passed + 1 that failed
  // #2 issued 1 request that passed
  // #3 issued 1 request that failed
  ASSERT_EQ((kMaxRequestsPerProcess + 1) + 1 + 1, msgs.size());

  for (size_t i = 0; i < kMaxRequestsPerProcess; ++i)
    CheckSuccessfulRequest(msgs[i], net::URLRequestTestJob::test_data_2());

  CheckFailedRequest(msgs[kMaxRequestsPerProcess + 0],
                     net::URLRequestTestJob::test_data_2(),
                     net::ERR_INSUFFICIENT_RESOURCES);
  CheckSuccessfulRequest(msgs[kMaxRequestsPerProcess + 1],
                         net::URLRequestTestJob::test_data_2());
  CheckFailedRequest(msgs[kMaxRequestsPerProcess + 2],
                     net::URLRequestTestJob::test_data_2(),
                     net::ERR_INSUFFICIENT_RESOURCES);

  second_filter->OnChannelClosing();
  third_filter->OnChannelClosing();
}

// Tests that we sniff the mime type for a simple request.
TEST_F(ResourceDispatcherHostTest, MimeSniffed) {
  std::string raw_headers("HTTP/1.1 200 OK\n\n");
  std::string response_data("<html><title>Test One</title></html>");
  SetResponse(raw_headers, response_data);

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"));

  // Flush all pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

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
  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Content-type: image/jpeg\n\n");
  std::string response_data("<html><title>Test One</title></html>");
  SetResponse(raw_headers, response_data);

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"));

  // Flush all pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

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
  SetResponse("HTTP/1.1 304 Not Modified\n\n");

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"));

  // Flush all pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // Sorts out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());

  ResourceResponseHead response_head;
  GetResponseHead(msgs[0], &response_head);
  ASSERT_EQ("", response_head.mime_type);
}

TEST_F(ResourceDispatcherHostTest, MimeSniff204) {
  SetResponse("HTTP/1.1 204 No Content\n\n");

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"));

  // Flush all pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // Sorts out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1U, msgs.size());

  ResourceResponseHead response_head;
  GetResponseHead(msgs[0], &response_head);
  ASSERT_EQ("text/plain", response_head.mime_type);
}

TEST_F(ResourceDispatcherHostTest, MimeSniffEmpty) {
  SetResponse("HTTP/1.1 200 OK\n\n");

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"));

  // Flush all pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

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
  NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(true);
  std::string raw_headers("HTTP/1.1 403 Forbidden\n"
                          "Content-disposition: attachment; filename=blah\n"
                          "Content-type: application/octet-stream\n\n");
  std::string response_data("<html><title>Test One</title></html>");
  SetResponse(raw_headers, response_data);

  HandleScheme("http");

  int expected_error_code = net::ERR_INVALID_RESPONSE;
  GURL forbidden_download_url = GURL("http:bla");

  CompleteFailingMainResourceRequest(forbidden_download_url,
                                     expected_error_code);
}

// Test for http://crbug.com/76202 .  We don't want to destroy a
// download request prematurely when processing a cancellation from
// the renderer.
TEST_F(ResourceDispatcherHostTest, IgnoreCancelForDownloads) {
  // PlzNavigate: A request that ends up being a download is a main resource
  // request. Hence, it has been initiated by the browser and is not associated
  // with a renderer. Therefore, it cannot be canceled by a renderer IPC.
  if (IsBrowserSideNavigationEnabled()) {
    SUCCEED() << "Not applicable with --enable-browser-side-navigation.";
    return;
  }

  NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(true);

  EXPECT_EQ(0, host_.pending_requests());

  int render_view_id = 0;
  int request_id = 1;

  std::string raw_headers("HTTP\n"
                          "Content-disposition: attachment; filename=foo\n\n");
  std::string response_data("01234567890123456789\x01foobar");

  // Get past sniffing metrics in the MimeTypeResourceHandler.  Note that
  // if we don't get past the sniffing metrics, the result will be that
  // the MimeTypeResourceHandler won't have figured out that it's a download,
  // won't have constructed a DownloadResourceHandler, and and the request will
  // be successfully canceled below, failing the test.
  response_data.resize(1025, ' ');

  SetResponse(raw_headers, response_data);
  job_factory_->SetDelayedCompleteJobGeneration(true);
  HandleScheme("http");

  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  GURL("http://example.com/blah"),
                                  RESOURCE_TYPE_MAIN_FRAME);
  // Return some data so that the request is identified as a download
  // and the proper resource handlers are created.
  EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());
  content::RunAllTasksUntilIdle();

  // And now simulate a cancellation coming from the renderer.
  ResourceHostMsg_CancelRequest msg(request_id);
  OnMessageReceived(msg, filter_.get());

  // Since the request had already started processing as a download,
  // the cancellation above should have been ignored and the request
  // should still be alive.
  EXPECT_EQ(1, host_.pending_requests());

  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();
}

TEST_F(ResourceDispatcherHostTest, CancelRequestsForContext) {
  if (IsBrowserSideNavigationEnabled())
    return;

  EXPECT_EQ(0, host_.pending_requests());
  NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(true);

  int render_view_id = 0;
  int request_id = 1;

  std::string raw_headers("HTTP\n"
                          "Content-disposition: attachment; filename=foo\n\n");
  std::string response_data("01234567890123456789\x01foobar");
  // Get past sniffing metrics.
  response_data.resize(1025, ' ');

  SetResponse(raw_headers, response_data);
  job_factory_->SetDelayedCompleteJobGeneration(true);
  HandleScheme("http");

  const GURL download_url = GURL("http://example.com/blah");

  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  download_url, RESOURCE_TYPE_MAIN_FRAME);

  // Return some data so that the request is identified as a download
  // and the proper resource handlers are created.
  EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());
  content::RunAllTasksUntilIdle();

  // And now simulate a cancellation coming from the renderer.
  ResourceHostMsg_CancelRequest msg(request_id);
  OnMessageReceived(msg, filter_.get());

  // Since the request had already started processing as a download,
  // the cancellation above should have been ignored and the request
  // should still be alive.
  EXPECT_EQ(1, host_.pending_requests());

  // Cancelling by other methods shouldn't work either.
  host_.CancelRequestsForProcess(render_view_id);
  EXPECT_EQ(1, host_.pending_requests());

  // Cancelling by context should work.
  host_.CancelRequestsForContext(filter_->resource_context());
  EXPECT_EQ(0, host_.pending_requests());

  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {
  }
  content::RunAllTasksUntilIdle();
}

TEST_F(ResourceDispatcherHostTest, CancelRequestsForContextDetached) {
  EXPECT_EQ(0, host_.pending_requests());

  int render_view_id = 0;
  int request_id = 1;

  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  net::URLRequestTestJob::test_url_4(),
                                  RESOURCE_TYPE_PREFETCH);  // detachable type

  // Simulate a cancel coming from the renderer.
  RendererCancelRequest(request_id);

  // Since the request had already started processing as detachable,
  // the cancellation above should have been ignored and the request
  // should have been detached.
  EXPECT_EQ(1, host_.pending_requests());

  // Cancelling by other methods should also leave it detached.
  host_.CancelRequestsForProcess(render_view_id);
  EXPECT_EQ(1, host_.pending_requests());

  // Cancelling by context should work.
  host_.CancelRequestsForContext(filter_->resource_context());
  EXPECT_EQ(0, host_.pending_requests());
}

// Test the cancelling of requests that are being transferred to a new renderer
// due to a redirection.
TEST_F(ResourceDispatcherHostTest, CancelRequestsForContextTransferred) {
  // PlzNavigate: there are no transferred requests in PlzNavigate.
  if (IsBrowserSideNavigationEnabled())
    return;

  NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(true);

  EXPECT_EQ(0, host_.pending_requests());

  int request_id = 1;

  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Content-Type: text/html; charset=utf-8\n\n");
  std::string response_data("<html>foobar</html>");

  SetResponse(raw_headers, response_data);
  HandleScheme("http");

  MakeWebContentsAssociatedTestRequestWithResourceType(
      request_id, GURL("http://example.com/blah"), RESOURCE_TYPE_MAIN_FRAME);

  GlobalRequestID global_request_id(web_contents_filter_->child_id(),
                                    request_id);
  host_.MarkAsTransferredNavigation(global_request_id, base::Closure());

  // And now simulate a cancellation coming from the renderer.
  ResourceHostMsg_CancelRequest msg(request_id);
  OnMessageReceived(msg, web_contents_filter_.get());

  // Since the request is marked as being transferred,
  // the cancellation above should have been ignored and the request
  // should still be alive.
  EXPECT_EQ(1, host_.pending_requests());

  // Cancelling by other methods shouldn't work either.
  host_.CancelRequestsForProcess(
      web_contents_->GetRenderViewHost()->GetRoutingID());
  EXPECT_EQ(1, host_.pending_requests());

  // Cancelling by context should work.
  host_.CancelRequestsForContext(web_contents_filter_->resource_context());
  EXPECT_EQ(0, host_.pending_requests());
}

// Test transferred navigations with text/html, which doesn't trigger any
// content sniffing.
TEST_F(ResourceDispatcherHostTest, TransferNavigationHtml) {
  if (IsBrowserSideNavigationEnabled()) {
    SUCCEED() << "Test is not applicable with browser side navigation enabled";
    return;
  }

  EXPECT_EQ(0, host_.pending_requests());

  NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(true);
  NavigationResourceThrottle::set_force_transfer_for_testing(true);

  int render_view_id = 0;
  int request_id = 1;

  // Configure initial request.
  SetResponse("HTTP/1.1 302 Found\n"
              "Location: http://other.com/blech\n\n");

  HandleScheme("http");

  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  GURL("http://example.com/blah"),
                                  RESOURCE_TYPE_MAIN_FRAME);

  // Now that we're blocked on the redirect, update the response and unblock by
  // telling the AsyncResourceHandler to follow the redirect.
  const std::string kResponseBody = "hello world";
  SetResponse("HTTP/1.1 200 OK\n"
              "Content-Type: text/html\n\n",
              kResponseBody);
  ResourceHostMsg_FollowRedirect redirect_msg(request_id);
  OnMessageReceived(redirect_msg, filter_.get());
  content::RunAllTasksUntilIdle();

  // Flush all the pending requests to get the response through the
  // MimeTypeResourceHandler.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = MakeForwardingFilter();

  int new_render_view_id = 1;
  int new_request_id = 2;

  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, GURL("http://other.com/blech"));
  request.transferred_request_child_id = filter_->child_id();
  request.transferred_request_request_id = request_id;

  ResourceHostMsg_RequestResource transfer_request_msg(
      new_render_view_id, new_request_id, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(transfer_request_msg, second_filter.get());
  content::RunAllTasksUntilIdle();

  // Check generated messages.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  ASSERT_EQ(2U, msgs.size());
  EXPECT_EQ(ResourceMsg_ReceivedRedirect::ID, msgs[0][0].type());
  CheckSuccessfulRequest(msgs[1], kResponseBody);

  second_filter->OnChannelClosing();
}

// Test transferring two navigations with text/html, to ensure the resource
// accounting works.
TEST_F(ResourceDispatcherHostTest, TransferTwoNavigationsHtml) {
  if (IsBrowserSideNavigationEnabled()) {
    SUCCEED() << "Test is not applicable with browser side navigation enabled";
    return;
  }

  NavigationResourceThrottle::set_force_transfer_for_testing(true);
  NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(true);

  EXPECT_EQ(0, host_.pending_requests());

  int render_view_id = 0;
  int request_id = 1;

  // Configure initial request.
  const std::string kResponseBody = "hello world";
  SetResponse("HTTP/1.1 200 OK\n"
              "Content-Type: text/html\n\n",
              kResponseBody);

  HandleScheme("http");

  // Make the first request.
  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  GURL("http://example.com/blah"),
                                  RESOURCE_TYPE_MAIN_FRAME);

  // Make a second request from the same process.
  int second_request_id = 2;
  MakeTestRequestWithResourceType(filter_.get(), render_view_id,
                                  second_request_id,
                                  GURL("http://example.com/foo"),
                                  RESOURCE_TYPE_MAIN_FRAME);

  // Flush all the pending requests to get the response through the
  // MimeTypeResourceHandler.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  NavigationResourceThrottle::set_force_transfer_for_testing(false);

  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = MakeForwardingFilter();

  // Transfer the first request.
  int new_render_view_id = 1;
  int new_request_id = 5;
  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, GURL("http://example.com/blah"));
  request.transferred_request_child_id = filter_->child_id();
  request.transferred_request_request_id = request_id;

  ResourceHostMsg_RequestResource transfer_request_msg(
      new_render_view_id, new_request_id, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(transfer_request_msg, second_filter.get());
  content::RunAllTasksUntilIdle();

  // Transfer the second request.
  int new_second_request_id = 6;
  ResourceRequest second_request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, GURL("http://example.com/foo"));
  request.transferred_request_child_id = filter_->child_id();
  request.transferred_request_request_id = second_request_id;

  ResourceHostMsg_RequestResource second_transfer_request_msg(
      new_render_view_id, new_second_request_id, second_request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(second_transfer_request_msg, second_filter.get());
  content::RunAllTasksUntilIdle();

  // Check generated messages.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  ASSERT_EQ(2U, msgs.size());
  CheckSuccessfulRequest(msgs[0], kResponseBody);

  second_filter->OnChannelClosing();
}

// Test transferred navigations with text/plain, which causes
// MimeTypeResourceHandler to buffer the response to sniff the content before
// the transfer occurs.
TEST_F(ResourceDispatcherHostTest, TransferNavigationText) {
  if (IsBrowserSideNavigationEnabled()) {
    SUCCEED() << "Test is not applicable with browser side navigation enabled";
    return;
  }

  NavigationResourceThrottle::set_force_transfer_for_testing(true);
  NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(true);

  EXPECT_EQ(0, host_.pending_requests());

  int render_view_id = 0;
  int request_id = 1;

  // Configure initial request.
  SetResponse("HTTP/1.1 302 Found\n"
              "Location: http://other.com/blech\n\n");

  HandleScheme("http");

  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  GURL("http://example.com/blah"),
                                  RESOURCE_TYPE_MAIN_FRAME);

  // Now that we're blocked on the redirect, update the response and unblock by
  // telling the AsyncResourceHandler to follow the redirect.  Use a text/plain
  // MIME type, which causes MimeTypeResourceHandler to buffer it before the
  // transfer occurs.
  const std::string kResponseBody = "hello world";
  SetResponse("HTTP/1.1 200 OK\n"
              "Content-Type: text/plain\n\n",
              kResponseBody);
  ResourceHostMsg_FollowRedirect redirect_msg(request_id);
  OnMessageReceived(redirect_msg, filter_.get());
  content::RunAllTasksUntilIdle();

  // Flush all the pending requests to get the response through the
  // MimeTypeResourceHandler.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = MakeForwardingFilter();

  int new_render_view_id = 1;
  int new_request_id = 2;

  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, GURL("http://other.com/blech"));
  request.transferred_request_child_id = filter_->child_id();
  request.transferred_request_request_id = request_id;

  ResourceHostMsg_RequestResource transfer_request_msg(
      new_render_view_id, new_request_id, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(transfer_request_msg, second_filter.get());
  content::RunAllTasksUntilIdle();

  // Check generated messages.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  ASSERT_EQ(2U, msgs.size());
  EXPECT_EQ(ResourceMsg_ReceivedRedirect::ID, msgs[0][0].type());
  CheckSuccessfulRequest(msgs[1], kResponseBody);

  second_filter->OnChannelClosing();
}

TEST_F(ResourceDispatcherHostTest, TransferNavigationWithProcessCrash) {
  if (IsBrowserSideNavigationEnabled()) {
    SUCCEED() << "Test is not applicable with browser side navigation enabled";
    return;
  }

  NavigationResourceThrottle::set_force_transfer_for_testing(true);
  NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(true);

  EXPECT_EQ(0, host_.pending_requests());

  int render_view_id = 0;
  int request_id = 1;
  int first_child_id = -1;

  // Configure initial request.
  SetResponse("HTTP/1.1 302 Found\n"
              "Location: http://other.com/blech\n\n");
  const std::string kResponseBody = "hello world";

  HandleScheme("http");

  // Create a first filter that can be deleted before the second one starts.
  {
    scoped_refptr<ForwardingFilter> first_filter = MakeForwardingFilter();
    first_child_id = first_filter->child_id();

    ResourceRequest first_request = CreateResourceRequest(
        "GET", RESOURCE_TYPE_MAIN_FRAME, GURL("http://example.com/blah"));

    ResourceHostMsg_RequestResource first_request_msg(
        render_view_id, request_id, first_request,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    OnMessageReceived(first_request_msg, first_filter.get());
    content::RunAllTasksUntilIdle();

    // Now that we're blocked on the redirect, update the response and unblock
    // by telling the AsyncResourceHandler to follow the redirect.
    SetResponse("HTTP/1.1 200 OK\n"
                "Content-Type: text/html\n\n",
                kResponseBody);
    ResourceHostMsg_FollowRedirect redirect_msg(request_id);
    OnMessageReceived(redirect_msg, first_filter.get());
    content::RunAllTasksUntilIdle();

    // Flush all the pending requests to get the response through the
    // MimeTypeResourceHandler.
    while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

    first_filter->OnChannelClosing();
  }
  // The first filter is now deleted, as if the child process died.

  // Make sure we don't hold onto the ResourceMessageFilter after it is deleted.
  GlobalRequestID first_global_request_id(first_child_id, request_id);

  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = MakeForwardingFilter();

  int new_render_view_id = 1;
  int new_request_id = 2;

  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, GURL("http://other.com/blech"));
  request.transferred_request_child_id = first_child_id;
  request.transferred_request_request_id = request_id;

  // For cleanup.
  child_ids_.insert(second_filter->child_id());
  ResourceHostMsg_RequestResource transfer_request_msg(
      new_render_view_id, new_request_id, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(transfer_request_msg, second_filter.get());
  content::RunAllTasksUntilIdle();

  // Check generated messages.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  ASSERT_EQ(2U, msgs.size());
  EXPECT_EQ(ResourceMsg_ReceivedRedirect::ID, msgs[0][0].type());
  CheckSuccessfulRequest(msgs[1], kResponseBody);

  second_filter->OnChannelClosing();
}

TEST_F(ResourceDispatcherHostTest, TransferNavigationWithTwoRedirects) {
  if (IsBrowserSideNavigationEnabled()) {
    SUCCEED() << "Test is not applicable with browser side navigation enabled";
    return;
  }

  NavigationResourceThrottle::set_force_transfer_for_testing(true);
  NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(true);

  EXPECT_EQ(0, host_.pending_requests());

  int render_view_id = 0;
  int request_id = 1;

  // Configure initial request.
  SetResponse("HTTP/1.1 302 Found\n"
              "Location: http://other.com/blech\n\n");

  HandleScheme("http");

  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  GURL("http://example.com/blah"),
                                  RESOURCE_TYPE_MAIN_FRAME);

  // Now that we're blocked on the redirect, simulate hitting another redirect.
  SetResponse("HTTP/1.1 302 Found\n"
              "Location: http://other.com/blerg\n\n");
  ResourceHostMsg_FollowRedirect redirect_msg(request_id);
  OnMessageReceived(redirect_msg, filter_.get());
  content::RunAllTasksUntilIdle();

  // Now that we're blocked on the second redirect, update the response and
  // unblock by telling the AsyncResourceHandler to follow the redirect.
  // Again, use text/plain to force MimeTypeResourceHandler to buffer before
  // the transfer.
  const std::string kResponseBody = "hello world";
  SetResponse("HTTP/1.1 200 OK\n"
              "Content-Type: text/plain\n\n",
              kResponseBody);
  ResourceHostMsg_FollowRedirect redirect_msg2(request_id);
  OnMessageReceived(redirect_msg2, filter_.get());
  content::RunAllTasksUntilIdle();

  // Flush all the pending requests to get the response through the
  // MimeTypeResourceHandler.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  // This second filter is used to emulate a second process.
  scoped_refptr<ForwardingFilter> second_filter = MakeForwardingFilter();

  int new_render_view_id = 1;
  int new_request_id = 2;

  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_MAIN_FRAME, GURL("http://other.com/blech"));
  request.transferred_request_child_id = filter_->child_id();
  request.transferred_request_request_id = request_id;

  // For cleanup.
  child_ids_.insert(second_filter->child_id());
  ResourceHostMsg_RequestResource transfer_request_msg(
      new_render_view_id, new_request_id, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(transfer_request_msg, second_filter.get());

  // Verify that we update the ResourceRequestInfo.
  GlobalRequestID global_request_id(second_filter->child_id(), new_request_id);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  EXPECT_EQ(second_filter->child_id(), info->GetChildID());
  EXPECT_EQ(new_render_view_id, info->GetRouteID());
  EXPECT_EQ(new_request_id, info->GetRequestID());
  EXPECT_EQ(second_filter.get(), info->requester_info()->filter());

  // Let request complete.
  content::RunAllTasksUntilIdle();

  // Check generated messages.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  ASSERT_EQ(2U, msgs.size());
  EXPECT_EQ(ResourceMsg_ReceivedRedirect::ID, msgs[0][0].type());
  CheckSuccessfulRequest(msgs[1], kResponseBody);

  second_filter->OnChannelClosing();
}

TEST_F(ResourceDispatcherHostTest, UnknownURLScheme) {
  NavigationResourceThrottle::set_ui_checks_always_succeed_for_testing(true);
  EXPECT_EQ(0, host_.pending_requests());

  HandleScheme("http");

  const GURL invalid_sheme_url = GURL("foo://bar");
  const int expected_error_code = net::ERR_UNKNOWN_URL_SCHEME;

  CompleteFailingMainResourceRequest(invalid_sheme_url, expected_error_code);
}

TEST_F(ResourceDispatcherHostTest, DataReceivedACKs) {
  EXPECT_EQ(0, host_.pending_requests());

  SendDataReceivedACKs(true);

  HandleScheme("big-job");
  MakeTestRequest(0, 1, GURL("big-job:0123456789,1000000"));

  content::RunAllTasksUntilIdle();

  // Sort all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  size_t size = msgs[0].size();

  EXPECT_EQ(ResourceMsg_ReceivedResponse::ID, msgs[0][0].type());
  EXPECT_EQ(ResourceMsg_SetDataBuffer::ID, msgs[0][1].type());
  for (size_t i = 2; i < size - 1; ++i)
    EXPECT_EQ(ResourceMsg_DataReceived::ID, msgs[0][i].type());
  EXPECT_EQ(ResourceMsg_RequestComplete::ID, msgs[0][size - 1].type());
}

// Request a very large detachable resource and cancel part way. Some of the
// data should have been sent to the renderer, but not all.
TEST_F(ResourceDispatcherHostTest, DataSentBeforeDetach) {
  EXPECT_EQ(0, host_.pending_requests());

  int render_view_id = 0;
  int request_id = 1;

  std::string raw_headers("HTTP\n"
                          "Content-type: image/jpeg\n\n");
  std::string response_data("01234567890123456789\x01foobar");

  // Create a response larger than kMaxAllocationSize (currently 32K). Note
  // that if this increase beyond 512K we'll need to make the response longer.
  const int kAllocSize = 1024*512;
  response_data.resize(kAllocSize, ' ');

  SetResponse(raw_headers, response_data);
  job_factory_->SetDelayedCompleteJobGeneration(true);
  HandleScheme("http");

  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  GURL("http://example.com/blah"),
                                  RESOURCE_TYPE_PREFETCH);

  // Get a bit of data before cancelling.
  EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());

  // Simulate a cancellation coming from the renderer.
  ResourceHostMsg_CancelRequest msg(request_id);
  OnMessageReceived(msg, filter_.get());

  EXPECT_EQ(1, host_.pending_requests());

  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // Sort all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  EXPECT_EQ(4U, msgs[0].size());

  // Figure out how many bytes were received by the renderer.
  int data_offset;
  int data_length;
  ASSERT_TRUE(
      ExtractDataOffsetAndLength(msgs[0][2], &data_offset, &data_length));
  EXPECT_LT(0, data_length);
  EXPECT_GT(kAllocSize, data_length);

  // Verify the data that was received before cancellation. The request should
  // have appeared to cancel, however.
  CheckSuccessfulRequestWithErrorCode(
      msgs[0],
      std::string(response_data.begin(), response_data.begin() + data_length),
      net::ERR_ABORTED);
}

TEST_F(ResourceDispatcherHostTest, DelayedDataReceivedACKs) {
  EXPECT_EQ(0, host_.pending_requests());

  HandleScheme("big-job");
  MakeTestRequest(0, 1, GURL("big-job:0123456789,1000000"));

  content::RunAllTasksUntilIdle();

  // Sort all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // We expect 1x ReceivedResponse, 1x SetDataBuffer, Nx ReceivedData messages.
  EXPECT_EQ(ResourceMsg_ReceivedResponse::ID, msgs[0][0].type());
  EXPECT_EQ(ResourceMsg_SetDataBuffer::ID, msgs[0][1].type());
  for (size_t i = 2; i < msgs[0].size(); ++i)
    EXPECT_EQ(ResourceMsg_DataReceived::ID, msgs[0][i].type());

  // NOTE: If we fail the above checks then it means that we probably didn't
  // load a big enough response to trigger the delay mechanism we are trying to
  // test!

  msgs[0].erase(msgs[0].begin());
  msgs[0].erase(msgs[0].begin());

  // ACK all DataReceived messages until we find a RequestComplete message.
  bool complete = false;
  while (!complete) {
    for (size_t i = 0; i < msgs[0].size(); ++i) {
      if (msgs[0][i].type() == ResourceMsg_RequestComplete::ID) {
        complete = true;
        break;
      }

      EXPECT_EQ(ResourceMsg_DataReceived::ID, msgs[0][i].type());

      ResourceHostMsg_DataReceived_ACK msg(1);
      OnMessageReceived(msg, filter_.get());
    }

    content::RunAllTasksUntilIdle();

    msgs.clear();
    accum_.GetClassifiedMessages(&msgs);
  }
}

// Flakyness of this test might indicate memory corruption issues with
// for example the ResourceBuffer of AsyncResourceHandler.
TEST_F(ResourceDispatcherHostTest, DataReceivedUnexpectedACKs) {
  EXPECT_EQ(0, host_.pending_requests());

  HandleScheme("big-job");
  MakeTestRequest(0, 1, GURL("big-job:0123456789,1000000"));

  content::RunAllTasksUntilIdle();

  // Sort all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // We expect 1x ReceivedResponse, 1x SetDataBuffer, Nx ReceivedData messages.
  EXPECT_EQ(ResourceMsg_ReceivedResponse::ID, msgs[0][0].type());
  EXPECT_EQ(ResourceMsg_SetDataBuffer::ID, msgs[0][1].type());
  for (size_t i = 2; i < msgs[0].size(); ++i)
    EXPECT_EQ(ResourceMsg_DataReceived::ID, msgs[0][i].type());

  // NOTE: If we fail the above checks then it means that we probably didn't
  // load a big enough response to trigger the delay mechanism.

  // Send some unexpected ACKs.
  for (size_t i = 0; i < 128; ++i) {
    ResourceHostMsg_DataReceived_ACK msg(1);
    OnMessageReceived(msg, filter_.get());
  }

  msgs[0].erase(msgs[0].begin());
  msgs[0].erase(msgs[0].begin());

  // ACK all DataReceived messages until we find a RequestComplete message.
  bool complete = false;
  while (!complete) {
    for (size_t i = 0; i < msgs[0].size(); ++i) {
      if (msgs[0][i].type() == ResourceMsg_RequestComplete::ID) {
        complete = true;
        break;
      }

      EXPECT_EQ(ResourceMsg_DataReceived::ID, msgs[0][i].type());

      ResourceHostMsg_DataReceived_ACK msg(1);
      OnMessageReceived(msg, filter_.get());
    }

    content::RunAllTasksUntilIdle();

    msgs.clear();
    accum_.GetClassifiedMessages(&msgs);
  }
}

// Tests the dispatcher host's temporary file management.
TEST_F(ResourceDispatcherHostTest, RegisterDownloadedTempFile) {
  const int kRequestID = 1;

  // Create a temporary file.
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
  EXPECT_TRUE(base::PathExists(file_path));
  scoped_refptr<ShareableFileReference> deletable_file =
      ShareableFileReference::GetOrCreate(
          file_path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          base::CreateSingleThreadTaskRunnerWithTraits({base::MayBlock()})
              .get());

  // Not readable.
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), file_path));

  // Register it for a resource request.
  host_.RegisterDownloadedTempFile(filter_->child_id(), kRequestID, file_path);

  // Should be readable now.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), file_path));

  // The child releases from the request.
  ResourceHostMsg_ReleaseDownloadedFile release_msg(kRequestID);
  OnMessageReceived(release_msg, filter_.get());

  // Still readable because there is another reference to the file. (The child
  // may take additional blob references.)
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), file_path));

  // Release extra references and wait for the file to be deleted. (This relies
  // on the delete happening on the FILE thread which is mapped to main thread
  // in this test.)
  deletable_file = nullptr;
  content::RunAllTasksUntilIdle();

  // The file is no longer readable to the child and has been deleted.
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), file_path));
  EXPECT_FALSE(base::PathExists(file_path));
}

// Tests the dispatcher host's temporary file management in the mojo-enabled
// loading.
TEST_F(ResourceDispatcherHostTest, RegisterDownloadedTempFileWithMojo) {
  const int kRequestID = 1;

  // Create a temporary file.
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
  EXPECT_TRUE(base::PathExists(file_path));
  scoped_refptr<ShareableFileReference> deletable_file =
      ShareableFileReference::GetOrCreate(
          file_path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          base::CreateSingleThreadTaskRunnerWithTraits({base::MayBlock()})
              .get());

  // Not readable.
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), file_path));

  // Register it for a resource request.
  auto downloaded_file =
      DownloadedTempFileImpl::Create(filter_->child_id(), kRequestID);
  mojom::DownloadedTempFilePtr downloaded_file_ptr =
      DownloadedTempFileImpl::Create(filter_->child_id(), kRequestID);
  host_.RegisterDownloadedTempFile(filter_->child_id(), kRequestID, file_path);

  // Should be readable now.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), file_path));

  // The child releases from the request.
  downloaded_file_ptr = nullptr;
  content::RunAllTasksUntilIdle();

  // Still readable because there is another reference to the file. (The child
  // may take additional blob references.)
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), file_path));

  // Release extra references and wait for the file to be deleted. (This relies
  // on the delete happening on the FILE thread which is mapped to main thread
  // in this test.)
  deletable_file = nullptr;
  content::RunAllTasksUntilIdle();

  // The file is no longer readable to the child and has been deleted.
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), file_path));
  EXPECT_FALSE(base::PathExists(file_path));
}

// Tests that temporary files held on behalf of child processes are released
// when the child process dies.
TEST_F(ResourceDispatcherHostTest, ReleaseTemporiesOnProcessExit) {
  const int kRequestID = 1;

  // Create a temporary file.
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
  scoped_refptr<ShareableFileReference> deletable_file =
      ShareableFileReference::GetOrCreate(
          file_path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          base::CreateSingleThreadTaskRunnerWithTraits({base::MayBlock()})
              .get());

  // Register it for a resource request.
  host_.RegisterDownloadedTempFile(filter_->child_id(), kRequestID, file_path);
  deletable_file = NULL;

  // Should be readable now.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), file_path));

  // Let the process die.
  filter_->OnChannelClosing();
  content::RunAllTasksUntilIdle();

  // The file is no longer readable to the child and has been deleted.
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), file_path));
  EXPECT_FALSE(base::PathExists(file_path));
}

TEST_F(ResourceDispatcherHostTest, DownloadToFile) {
  // Make a request which downloads to file.
  ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_SUB_RESOURCE, net::URLRequestTestJob::test_url_1());
  request.download_to_file = true;
  ResourceHostMsg_RequestResource request_msg(
      0, 1, request,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  OnMessageReceived(request_msg, filter_.get());

  // Running the message loop until idle does not work because
  // ResourceDispatcherHostImpl posts tasks to TaskScheduler. Instead, wait for
  // the ResourceMsg_RequestComplete to go out. Then, wait until no
  // TaskScheduler or main thread tasks remain.
  WaitForRequestComplete();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, host_.pending_requests());

  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  ASSERT_EQ(1U, msgs.size());
  const std::vector<IPC::Message>& messages = msgs[0];

  // The request should contain the following messages:
  //     ReceivedResponse    (indicates headers received and filename)
  //     DataDownloaded*     (bytes downloaded and total length)
  //     RequestComplete     (request is done)

  // ReceivedResponse
  ResourceResponseHead response_head;
  GetResponseHead(messages, &response_head);
  ASSERT_FALSE(response_head.download_file_path.empty());

  // DataDownloaded
  size_t total_len = 0;
  for (size_t i = 1; i < messages.size() - 1; i++) {
    ASSERT_EQ(ResourceMsg_DataDownloaded::ID, messages[i].type());
    base::PickleIterator iter(messages[i]);
    int request_id, data_len;
    ASSERT_TRUE(IPC::ReadParam(&messages[i], &iter, &request_id));
    ASSERT_TRUE(IPC::ReadParam(&messages[i], &iter, &data_len));
    total_len += data_len;
  }
  EXPECT_EQ(net::URLRequestTestJob::test_data_1().size(), total_len);

  // RequestComplete
  CheckRequestCompleteErrorCode(messages.back(), net::OK);

  // Verify that the data ended up in the temporary file.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(response_head.download_file_path,
                                     &contents));
  EXPECT_EQ(net::URLRequestTestJob::test_data_1(), contents);

  // The file should be readable by the child.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), response_head.download_file_path));

  // When the renderer releases the file, it should be deleted. Again,
  // RunUntilIdle doesn't work because base::WorkerPool is involved.
  ShareableFileReleaseWaiter waiter(response_head.download_file_path);
  ResourceHostMsg_ReleaseDownloadedFile release_msg(1);
  OnMessageReceived(release_msg, filter_.get());
  waiter.Wait();
  // The release callback runs before the delete is scheduled, so pump the
  // message loop for the delete itself. (This relies on the delete happening on
  // the FILE thread which is mapped to main thread in this test.)
  content::RunAllTasksUntilIdle();

  EXPECT_FALSE(base::PathExists(response_head.download_file_path));
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      filter_->child_id(), response_head.download_file_path));
}

WebContents* WebContentsBinder(WebContents* rv) { return rv; }

// Tests GetLoadInfoForAllRoutes when there are 3 requests from the same
// RenderView.  The second one is farthest along.
TEST_F(ResourceDispatcherHostTest, LoadInfo) {
  std::unique_ptr<LoadInfoList> infos(new LoadInfoList);
  LoadInfo info;
  WebContents* wc1 = reinterpret_cast<WebContents*>(0x1);
  info.web_contents_getter = base::Bind(WebContentsBinder, wc1);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_SENDING_REQUEST,
                                            base::string16());
  info.url = GURL("test://1/");
  info.upload_position = 0;
  info.upload_size = 0;
  infos->push_back(info);

  info.url = GURL("test://2/");
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_READING_RESPONSE,
                                            base::string16());
  infos->push_back(info);

  info.url = GURL("test://3/");

  std::unique_ptr<LoadInfoMap> load_info_map =
      ResourceDispatcherHostImpl::PickMoreInterestingLoadInfos(
          std::move(infos));
  ASSERT_EQ(1u, load_info_map->size());
  ASSERT_TRUE(load_info_map->find(wc1) != load_info_map->end());
  EXPECT_EQ(GURL("test://2/"), (*load_info_map)[wc1].url);
  EXPECT_EQ(net::LOAD_STATE_READING_RESPONSE,
            (*load_info_map)[wc1].load_state.state);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_position);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_size);
}

// Tests GetLoadInfoForAllRoutes when there are 2 requests with the same
// priority.  The first one (Which will have the lowest ID) should be returned.
TEST_F(ResourceDispatcherHostTest, LoadInfoSamePriority) {
  std::unique_ptr<LoadInfoList> infos(new LoadInfoList);
  LoadInfo info;
  WebContents* wc1 = reinterpret_cast<WebContents*>(0x1);
  info.web_contents_getter = base::Bind(WebContentsBinder, wc1);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_IDLE,
                                            base::string16());
  info.url = GURL("test://1/");
  info.upload_position = 0;
  info.upload_size = 0;
  infos->push_back(info);

  info.url = GURL("test://2/");
  infos->push_back(info);

  std::unique_ptr<LoadInfoMap> load_info_map =
      ResourceDispatcherHostImpl::PickMoreInterestingLoadInfos(
          std::move(infos));
  ASSERT_EQ(1u, load_info_map->size());
  ASSERT_TRUE(load_info_map->find(wc1) != load_info_map->end());
  EXPECT_EQ(GURL("test://1/"), (*load_info_map)[wc1].url);
  EXPECT_EQ(net::LOAD_STATE_IDLE, (*load_info_map)[wc1].load_state.state);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_position);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_size);
}

// Tests GetLoadInfoForAllRoutes when a request is uploading a body.
TEST_F(ResourceDispatcherHostTest, LoadInfoUploadProgress) {
  std::unique_ptr<LoadInfoList> infos(new LoadInfoList);
  LoadInfo info;
  WebContents* wc1 = reinterpret_cast<WebContents*>(0x1);
  info.web_contents_getter = base::Bind(WebContentsBinder, wc1);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_READING_RESPONSE,
                                            base::string16());
  info.url = GURL("test://1/");
  info.upload_position = 0;
  info.upload_size = 0;
  infos->push_back(info);

  info.upload_position = 1000;
  info.upload_size = 1000;
  infos->push_back(info);

  info.url = GURL("test://2/");
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_SENDING_REQUEST,
                                            base::string16());
  info.upload_position = 50;
  info.upload_size = 100;
  infos->push_back(info);

  info.url = GURL("test://1/");
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_READING_RESPONSE,
                                            base::string16());
  info.upload_position = 1000;
  info.upload_size = 1000;
  infos->push_back(info);

  info.url = GURL("test://3/");
  info.upload_position = 0;
  info.upload_size = 0;
  infos->push_back(info);

  std::unique_ptr<LoadInfoMap> load_info_map =
      ResourceDispatcherHostImpl::PickMoreInterestingLoadInfos(
          std::move(infos));
  ASSERT_EQ(1u, load_info_map->size());
  ASSERT_TRUE(load_info_map->find(wc1) != load_info_map->end());
  EXPECT_EQ(GURL("test://2/"), (*load_info_map)[wc1].url);
  EXPECT_EQ(net::LOAD_STATE_SENDING_REQUEST,
            (*load_info_map)[wc1].load_state.state);
  EXPECT_EQ(50u, (*load_info_map)[wc1].upload_position);
  EXPECT_EQ(100u, (*load_info_map)[wc1].upload_size);
}

// Tests GetLoadInfoForAllRoutes when there are 4 requests from 2 different
// RenderViews.  Also tests the case where the first / last requests are the
// most interesting ones.
TEST_F(ResourceDispatcherHostTest, LoadInfoTwoRenderViews) {
  std::unique_ptr<LoadInfoList> infos(new LoadInfoList);
  LoadInfo info;
  WebContents* wc1 = reinterpret_cast<WebContents*>(0x1);
  info.web_contents_getter = base::Bind(WebContentsBinder, wc1);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_CONNECTING,
                                            base::string16());
  info.url = GURL("test://1/");
  info.upload_position = 0;
  info.upload_size = 0;
  infos->push_back(info);

  WebContents* wc2 = reinterpret_cast<WebContents*>(0x2);
  info.web_contents_getter = base::Bind(WebContentsBinder, wc2);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_IDLE,
                                            base::string16());
  info.url = GURL("test://2/");
  infos->push_back(info);

  info.web_contents_getter = base::Bind(WebContentsBinder, wc1);
  info.url = GURL("test://3/");
  infos->push_back(info);

  info.web_contents_getter = base::Bind(WebContentsBinder, wc2);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_CONNECTING,
                                            base::string16());
  info.url = GURL("test://4/");
  infos->push_back(info);

  std::unique_ptr<LoadInfoMap> load_info_map =
      ResourceDispatcherHostImpl::PickMoreInterestingLoadInfos(
          std::move(infos));
  ASSERT_EQ(2u, load_info_map->size());

  ASSERT_TRUE(load_info_map->find(wc1) != load_info_map->end());
  EXPECT_EQ(GURL("test://1/"), (*load_info_map)[wc1].url);
  EXPECT_EQ(net::LOAD_STATE_CONNECTING,
            (*load_info_map)[wc1].load_state.state);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_position);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_size);

  ASSERT_TRUE(load_info_map->find(wc2) != load_info_map->end());
  EXPECT_EQ(GURL("test://4/"), (*load_info_map)[wc2].url);
  EXPECT_EQ(net::LOAD_STATE_CONNECTING,
            (*load_info_map)[wc2].load_state.state);
  EXPECT_EQ(0u, (*load_info_map)[wc2].upload_position);
  EXPECT_EQ(0u, (*load_info_map)[wc2].upload_size);
}

// Confirm that resource response started notifications are correctly
// transmitted to the WebContents.
TEST_F(ResourceDispatcherHostTest, TransferResponseStarted) {
  int initial_count = web_contents_observer_->resource_response_start_count();

  MakeWebContentsAssociatedTestRequest(1, net::URLRequestTestJob::test_url_1());
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(initial_count + 1,
            web_contents_observer_->resource_response_start_count());
}

// Confirm that request redirected notifications are correctly
// transmitted to the WebContents.
TEST_F(ResourceDispatcherHostTest, TransferRequestRedirected) {
  int initial_count = web_contents_observer_->resource_request_redirect_count();

  MakeWebContentsAssociatedTestRequest(
      1, net::URLRequestTestJob::test_url_redirect_to_url_2());
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(initial_count + 1,
            web_contents_observer_->resource_request_redirect_count());
}

// Confirm that DidChangePriority messages are respected.
TEST_F(ResourceDispatcherHostTest, DidChangePriority) {
  // ResourceScheduler only throttles http and https requests.
  HandleScheme("http");

  // Needed to enable scheduling for this child.
  host_.OnRenderViewHostCreated(filter_->child_id(),  // child_id
                                0,                    // route_id
                                request_context_getter_.get());

  // Prevent any of these requests from completing.
  job_factory_->SetDelayedCompleteJobGeneration(true);
  SetResponse("HTTP/1.1 200 OK\n\n", "<title>Dummy body</title>");

  // Only one idle priority request will run while a high-priority request
  // exists.
  MakeTestRequestWithPriority(0, 1, net::HIGHEST);
  MakeTestRequestWithPriority(0, 2, net::IDLE);
  MakeTestRequestWithPriority(0, 3, net::IDLE);

  KickOffRequest();

  EXPECT_EQ(2, job_factory_->url_request_jobs_created_count());

  // Increase the priority of the second idle priority request. It was
  // scheduled later, so it is not currently running.
  ResourceHostMsg_DidChangePriority priority_msg(3, net::MAXIMUM_PRIORITY, 0);
  OnMessageReceived(priority_msg, filter_.get());
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(3, job_factory_->url_request_jobs_created_count());

  // Cleanup.
  host_.OnRenderViewHostDeleted(filter_->child_id(),  // child_id
                                0);                   // route_id
}

// Confirm that resource response started notifications for downloads are not
// transmitted to the WebContents.
TEST_F(ResourceDispatcherHostTest, TransferResponseStartedDownload) {
  int initial_count(web_contents_observer_->resource_response_start_count());

  MakeWebContentsAssociatedDownloadRequest(
      1, net::URLRequestTestJob::test_url_1());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(initial_count,
            web_contents_observer_->resource_response_start_count());
}

// Confirm that request redirected notifications for downloads are not
// transmitted to the WebContents.
TEST_F(ResourceDispatcherHostTest, TransferRequestRedirectedDownload) {
  int initial_count(web_contents_observer_->resource_request_redirect_count());

  MakeWebContentsAssociatedDownloadRequest(
      1, net::URLRequestTestJob::test_url_redirect_to_url_2());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(initial_count,
            web_contents_observer_->resource_request_redirect_count());
}

// Tests that a ResourceThrottle that needs to process the response before any
// part of the body is read can do so.
TEST_F(ResourceDispatcherHostTest, ThrottleMustProcessResponseBeforeRead) {
  // Ensure all jobs will check that no read operation is called.
  job_factory_->SetMustNotReadJobGeneration(true);
  HandleScheme("http");

  // Create a ResourceThrottle that must process the response before any part of
  // the body is read. This throttle will also cancel the request in
  // WillProcessResponse.
  TestResourceDispatcherHostDelegate delegate;
  int throttle_flags = CANCEL_PROCESSING_RESPONSE | MUST_NOT_CACHE_BODY;
  delegate.set_flags(throttle_flags);
  host_.SetDelegate(&delegate);

  // This response should normally result in the MIME type being sniffed, which
  // requires reading the body.
  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/plain; charset=utf-8\n\n");
  std::string response_data("p { text-align: center; }");
  SetResponse(raw_headers, response_data);

  MakeTestRequestWithResourceType(filter_.get(), filter_->child_id(), 1,
                                  GURL("http://example.com/blah"),
                                  RESOURCE_TYPE_STYLESHEET);

  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {
  }
  content::RunAllTasksUntilIdle();
}

// A URLRequestTestJob that sets a test certificate on the |ssl_info|
// field of the response.
class TestHTTPSURLRequestJob : public net::URLRequestTestJob {
 public:
  TestHTTPSURLRequestJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& response_headers,
                         const std::string& response_data,
                         bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {}

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    net::URLRequestTestJob::GetResponseInfo(info);
    info->ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  }
};

net::URLRequestJob* TestURLRequestJobFactory::MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const {
  url_request_jobs_created_count_++;
  if (hang_after_start_) {
    return new net::URLRequestFailedJob(request, network_delegate,
                                        net::ERR_IO_PENDING);
  }
  if (test_fixture_->response_headers_.empty()) {
    if (delay_start_) {
      return new URLRequestTestDelayedStartJob(request, network_delegate);
    } else if (delay_complete_) {
      return new URLRequestTestDelayedCompletionJob(request,
                                                    network_delegate);
    } else if (scheme == "big-job") {
      return new URLRequestBigJob(request, network_delegate);
    } else {
      return new net::URLRequestTestJob(request, network_delegate,
                                        test_fixture_->auto_advance_);
    }
  } else {
    if (delay_start_) {
      return new URLRequestTestDelayedStartJob(
          request, network_delegate,
          test_fixture_->response_headers_, test_fixture_->response_data_,
          false);
    } else if (delay_complete_) {
      return new URLRequestTestDelayedCompletionJob(
          request, network_delegate,
          test_fixture_->response_headers_, test_fixture_->response_data_,
          false);
    } else if (must_not_read_) {
      return new URLRequestMustNotReadTestJob(request, network_delegate,
                                              test_fixture_->response_headers_,
                                              test_fixture_->response_data_);
    } else if (test_fixture_->use_test_ssl_certificate_) {
      return new TestHTTPSURLRequestJob(request, network_delegate,
                                        test_fixture_->response_headers_,
                                        test_fixture_->response_data_, false);
    } else {
      return new net::URLRequestTestJob(
          request, network_delegate, test_fixture_->response_headers_,
          test_fixture_->response_data_, test_fixture_->auto_advance_);
    }
  }
}

net::URLRequestJob* TestURLRequestJobFactory::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  return nullptr;
}

net::URLRequestJob* TestURLRequestJobFactory::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return nullptr;
}

}  // namespace content
