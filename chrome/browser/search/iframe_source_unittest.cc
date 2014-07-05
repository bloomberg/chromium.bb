// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/iframe_source.h"

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/search/instant_io_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "grit/browser_resources.h"
#include "ipc/ipc_message.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::ResourceType;

const int kNonInstantRendererPID = 0;
const char kNonInstantOrigin[] = "http://evil";
const int kInstantRendererPID = 1;
const char kInstantOrigin[] = "chrome-search://instant";
const int kInvalidRendererPID = 42;

class TestIframeSource : public IframeSource {
 public:
  using IframeSource::GetMimeType;
  using IframeSource::ShouldServiceRequest;
  using IframeSource::SendResource;
  using IframeSource::SendJSWithOrigin;

 protected:
  virtual std::string GetSource() const OVERRIDE {
    return "test";
  }

  virtual bool ServesPath(const std::string& path) const OVERRIDE {
    return path == "/valid.html" || path == "/valid.js";
  }

  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE {
  }

  // RenderFrameHost is hard to mock in concert with everything else, so stub
  // this method out for testing.
  virtual bool GetOrigin(
      int process_id,
      int render_frame_id,
      std::string* origin) const OVERRIDE {
    if (process_id == kInstantRendererPID) {
      *origin = kInstantOrigin;
      return true;
    }
    if (process_id == kNonInstantRendererPID) {
      *origin = kNonInstantOrigin;
      return true;
    }
    return false;
  }
};

class IframeSourceTest : public testing::Test {
 public:
  // net::URLRequest wants to be executed with a message loop that has TYPE_IO.
  // InstantIOContext needs to be created on the UI thread and have everything
  // else happen on the IO thread. This setup is a hacky way to satisfy all
  // those constraints.
  IframeSourceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        resource_context_(&test_url_request_context_),
        instant_io_context_(NULL),
        response_(NULL) {
  }

  TestIframeSource* source() { return source_.get(); }

  std::string response_string() {
    if (response_.get()) {
      return std::string(response_->front_as<char>(), response_->size());
    }
    return "";
  }

  net::URLRequest* MockRequest(
      const std::string& url,
      bool allocate_info,
      int render_process_id,
      int render_frame_id) {
    net::URLRequest* request =
        new net::URLRequest(GURL(url),
                            net::DEFAULT_PRIORITY,
                            NULL,
                            resource_context_.GetRequestContext());
    if (allocate_info) {
      content::ResourceRequestInfo::AllocateForTesting(request,
                                                       ResourceType::SUB_FRAME,
                                                       &resource_context_,
                                                       render_process_id,
                                                       render_frame_id,
                                                       MSG_ROUTING_NONE,
                                                       false);
    }
    return request;
  }

  void SendResource(int resource_id) {
    source()->SendResource(resource_id, callback_);
  }

  void SendJSWithOrigin(
      int resource_id,
      int render_process_id,
      int render_frame_id) {
    source()->SendJSWithOrigin(resource_id, render_process_id, render_frame_id,
                               callback_);
  }

 private:
  virtual void SetUp() OVERRIDE {
    source_.reset(new TestIframeSource());
    callback_ = base::Bind(&IframeSourceTest::SaveResponse,
                           base::Unretained(this));
    instant_io_context_ = new InstantIOContext;
    InstantIOContext::SetUserDataOnIO(&resource_context_, instant_io_context_);
    InstantIOContext::AddInstantProcessOnIO(instant_io_context_,
                                            kInstantRendererPID);
    response_ = NULL;
  }

  virtual void TearDown() {
    source_.reset();
  }

  void SaveResponse(base::RefCountedMemory* data) {
    response_ = data;
  }

  content::TestBrowserThreadBundle thread_bundle_;

  net::TestURLRequestContext test_url_request_context_;
  content::MockResourceContext resource_context_;
  scoped_ptr<TestIframeSource> source_;
  content::URLDataSource::GotDataCallback callback_;
  scoped_refptr<InstantIOContext> instant_io_context_;
  scoped_refptr<base::RefCountedMemory> response_;
};

TEST_F(IframeSourceTest, ShouldServiceRequest) {
  scoped_ptr<net::URLRequest> request;
  request.reset(MockRequest("http://test/loader.js", true,
                            kNonInstantRendererPID, 0));
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  request.reset(MockRequest("chrome-search://bogus/valid.js", true,
                            kInstantRendererPID, 0));
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  request.reset(MockRequest("chrome-search://test/bogus.js", true,
                            kInstantRendererPID, 0));
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  request.reset(MockRequest("chrome-search://test/valid.js", true,
                            kInstantRendererPID, 0));
  EXPECT_TRUE(source()->ShouldServiceRequest(request.get()));
  request.reset(MockRequest("chrome-search://test/valid.js", true,
                            kNonInstantRendererPID, 0));
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  request.reset(MockRequest("chrome-search://test/valid.js", true,
                            kInvalidRendererPID, 0));
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
}

TEST_F(IframeSourceTest, GetMimeType) {
  // URLDataManagerBackend does not include / in path_and_query.
  EXPECT_EQ("text/html", source()->GetMimeType("foo.html"));
  EXPECT_EQ("application/javascript", source()->GetMimeType("foo.js"));
  EXPECT_EQ("text/css", source()->GetMimeType("foo.css"));
  EXPECT_EQ("image/png", source()->GetMimeType("foo.png"));
  EXPECT_EQ("", source()->GetMimeType("bogus"));
}

TEST_F(IframeSourceTest, SendResource) {
  SendResource(IDR_MOST_VISITED_TITLE_HTML);
  EXPECT_FALSE(response_string().empty());
}

TEST_F(IframeSourceTest, SendJSWithOrigin) {
  SendJSWithOrigin(IDR_MOST_VISITED_TITLE_JS, kInstantRendererPID, 0);
  EXPECT_FALSE(response_string().empty());
  SendJSWithOrigin(IDR_MOST_VISITED_TITLE_JS, kNonInstantRendererPID, 0);
  EXPECT_FALSE(response_string().empty());
  SendJSWithOrigin(IDR_MOST_VISITED_TITLE_JS, kInvalidRendererPID, 0);
  EXPECT_TRUE(response_string().empty());
}
