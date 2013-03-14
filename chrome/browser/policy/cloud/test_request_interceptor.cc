// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/test_request_interceptor.h"

#include <limits>
#include <queue>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_element_reader.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_job.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Helper to execute a |task| on IO, and return only after it has completed.
void PostToIOAndWait(const base::Closure& task) {
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE, task);
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
}

// Helper callback for jobs that should fail with a network |error|.
net::URLRequestJob* ErrorJobCallback(int error,
                                     net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate) {
  return new net::URLRequestErrorJob(request, network_delegate, error);
}

// Helper callback for jobs that should fail with a 400 HTTP error.
net::URLRequestJob* BadRequestJobCallback(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  static const char kBadHeaders[] =
      "HTTP/1.1 400 Bad request\0"
      "Content-type: application/protobuf\0"
      "\0";
  std::string headers(kBadHeaders, arraysize(kBadHeaders));
  return new net::URLRequestTestJob(
      request, network_delegate, headers, std::string(), true);
}

// Parses the upload data in |request| into |request_msg|, and validates the
// request. The query string in the URL must contain the |expected_type| for
// the "request" parameter. Returns true if all checks succeeded, and the
// request data has been parsed into |request_msg|.
bool ValidRequest(net::URLRequest* request,
                  const std::string& expected_type,
                  em::DeviceManagementRequest* request_msg) {
  if (request->method() != "POST")
    return false;
  std::string spec = request->url().spec();
  if (spec.find("request=" + expected_type) == std::string::npos)
    return false;

  // This assumes that the payload data was set from a single string. In that
  // case the UploadDataStream has a single UploadBytesElementReader with the
  // data in memory.
  const net::UploadDataStream* stream = request->get_upload();
  if (!stream)
    return false;
  const ScopedVector<net::UploadElementReader>& readers =
      stream->element_readers();
  if (readers.size() != 1u)
    return false;
  const net::UploadBytesElementReader* reader = readers[0]->AsBytesReader();
  if (!reader)
    return false;
  std::string data(reader->bytes(), reader->length());
  if (!request_msg->ParseFromString(data))
    return false;

  return true;
}

// Helper callback for register jobs that should suceed. Validates the request
// parameters and returns an appropriate response job. If |expect_reregister|
// is true then the reregister flag must be set in the DeviceRegisterRequest
// protobuf.
net::URLRequestJob* RegisterJobCallback(
    em::DeviceRegisterRequest::Type expected_type,
    bool expect_reregister,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  em::DeviceManagementRequest request_msg;
  if (!ValidRequest(request, "register", &request_msg))
    return BadRequestJobCallback(request, network_delegate);

  if (!request_msg.has_register_request() ||
      request_msg.has_unregister_request() ||
      request_msg.has_policy_request() ||
      request_msg.has_device_status_report_request() ||
      request_msg.has_session_status_report_request() ||
      request_msg.has_auto_enrollment_request()) {
    return BadRequestJobCallback(request, network_delegate);
  }

  const em::DeviceRegisterRequest& register_request =
      request_msg.register_request();
  if (expect_reregister &&
      (!register_request.has_reregister() || !register_request.reregister())) {
    return BadRequestJobCallback(request, network_delegate);
  } else if (!expect_reregister &&
             register_request.has_reregister() &&
             register_request.reregister()) {
    return BadRequestJobCallback(request, network_delegate);
  }

  if (!register_request.has_type() || register_request.type() != expected_type)
    return BadRequestJobCallback(request, network_delegate);

  em::DeviceManagementResponse response;
  em::DeviceRegisterResponse* register_response =
      response.mutable_register_response();
  register_response->set_device_management_token("s3cr3t70k3n");
  std::string data;
  response.SerializeToString(&data);

  static const char kGoodHeaders[] =
      "HTTP/1.1 200 OK\0"
      "Content-type: application/protobuf\0"
      "\0";
  std::string headers(kGoodHeaders, arraysize(kGoodHeaders));
  return new net::URLRequestTestJob(
      request, network_delegate, headers, data, true);
}

}  // namespace

class TestRequestInterceptor::Delegate
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  explicit Delegate(const std::string& hostname);
  virtual ~Delegate();

  // ProtocolHandler implementation:
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;

  void GetPendingSize(size_t* pending_size) const;
  void PushJobCallback(const JobCallback& callback);

 private:
  const std::string hostname_;

  // The queue of pending callbacks. 'mutable' because MaybeCreateJob() is a
  // const method; it can't reenter though, because it runs exclusively on
  // the IO thread.
  mutable std::queue<JobCallback> pending_job_callbacks_;
};

TestRequestInterceptor::Delegate::Delegate(const std::string& hostname)
    : hostname_(hostname) {}

TestRequestInterceptor::Delegate::~Delegate() {}

net::URLRequestJob* TestRequestInterceptor::Delegate::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (request->url().host() != hostname_) {
    // Reject requests to other servers.
    return ErrorJobCallback(
        net::ERR_CONNECTION_REFUSED, request, network_delegate);
  }

  if (pending_job_callbacks_.empty()) {
    // Reject dmserver requests by default.
    return BadRequestJobCallback(request, network_delegate);
  }

  JobCallback callback = pending_job_callbacks_.front();
  pending_job_callbacks_.pop();
  return callback.Run(request, network_delegate);
}

void TestRequestInterceptor::Delegate::GetPendingSize(
    size_t* pending_size) const {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  *pending_size = pending_job_callbacks_.size();
}

void TestRequestInterceptor::Delegate::PushJobCallback(
    const JobCallback& callback) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  pending_job_callbacks_.push(callback);
}

TestRequestInterceptor::TestRequestInterceptor(const std::string& hostname)
    : hostname_(hostname) {
  delegate_ = new Delegate(hostname_);
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler> handler(delegate_);
  PostToIOAndWait(
      base::Bind(&net::URLRequestFilter::AddHostnameProtocolHandler,
                 base::Unretained(net::URLRequestFilter::GetInstance()),
                 "http", hostname_, base::Passed(&handler)));
}

TestRequestInterceptor::~TestRequestInterceptor() {
  // RemoveHostnameHandler() destroys the |delegate_|, which is owned by
  // the URLRequestFilter.
  delegate_ = NULL;
  PostToIOAndWait(
      base::Bind(&net::URLRequestFilter::RemoveHostnameHandler,
                 base::Unretained(net::URLRequestFilter::GetInstance()),
                 "http", hostname_));
}

size_t TestRequestInterceptor::GetPendingSize() const {
  size_t pending_size = std::numeric_limits<size_t>::max();
  PostToIOAndWait(base::Bind(&Delegate::GetPendingSize,
                             base::Unretained(delegate_),
                             &pending_size));
  return pending_size;
}

void TestRequestInterceptor::PushJobCallback(const JobCallback& callback) {
  PostToIOAndWait(base::Bind(&Delegate::PushJobCallback,
                             base::Unretained(delegate_),
                             callback));
}

// static
TestRequestInterceptor::JobCallback TestRequestInterceptor::ErrorJob(
    int error) {
  return base::Bind(&ErrorJobCallback, error);
}

// static
TestRequestInterceptor::JobCallback TestRequestInterceptor::BadRequestJob() {
  return base::Bind(&BadRequestJobCallback);
}

// static
TestRequestInterceptor::JobCallback TestRequestInterceptor::RegisterJob(
    em::DeviceRegisterRequest::Type expected_type,
    bool expect_reregister) {
  return base::Bind(&RegisterJobCallback, expected_type, expect_reregister);
}

}  // namespace policy
