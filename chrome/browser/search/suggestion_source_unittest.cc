// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestion_source.h"

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/search/instant_io_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

const int kNonInstantRendererPID = 0;
const char kNonInstantOrigin[] = "http://evil";
const int kInstantRendererPID = 1;
const char kInstantOrigin[] = "chrome-search://instant";
const int kInvalidRendererPID = 42;

class TestableSuggestionSource : public SuggestionSource {
 public:
  using SuggestionSource::StartDataRequest;
  using SuggestionSource::GetMimeType;
  using SuggestionSource::ShouldServiceRequest;

 protected:
  // RenderViewHost is hard to mock in concert with everything else, so stub
  // this method out for testing.
  virtual bool GetOrigin(
      int process_id,
      int render_view_id,
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

class SuggestionSourceTest : public testing::Test {
 public:
  // net::URLRequest wants to be executed with a message loop that has TYPE_IO.
  // InstantIOContext needs to be created on the UI thread and have everything
  // else happen on the IO thread. This setup is a hacky way to satisfy all
  // those constraints.
  SuggestionSourceTest()
    : message_loop_(base::MessageLoop::TYPE_IO),
      ui_thread_(content::BrowserThread::UI, &message_loop_),
      io_thread_(content::BrowserThread::IO, &message_loop_),
      instant_io_context_(NULL),
      response_(NULL) {
  }

  TestableSuggestionSource* source() { return source_.get(); }

  std::string response_string() {
    if (response_) {
      return std::string(reinterpret_cast<const char*>(response_->front()),
                         response_->size());
    }
    return "";
  }

  net::URLRequest* MockRequest(
      const std::string& url,
      bool allocate_info,
      int render_process_id,
      int render_view_id) {
    net::URLRequest* request =
        new net::URLRequest(GURL(url), NULL,
                            resource_context_.GetRequestContext());
    if (allocate_info) {
      content::ResourceRequestInfo::AllocateForTesting(request,
                                                       ResourceType::SUB_FRAME,
                                                       &resource_context_,
                                                       render_process_id,
                                                       render_view_id);
    }
    return request;
  }

  void StartDataRequest(
      const std::string& path_and_query,
      int render_process_id,
      int render_view_id) {
    source()->StartDataRequest(path_and_query, render_process_id,
                               render_view_id, callback_);
  }

 private:
  virtual void SetUp() {
    source_.reset(new TestableSuggestionSource());
    callback_ = base::Bind(&SuggestionSourceTest::SaveResponse,
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

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  content::MockResourceContext resource_context_;
  scoped_ptr<TestableSuggestionSource> source_;
  content::URLDataSource::GotDataCallback callback_;
  scoped_refptr<InstantIOContext> instant_io_context_;
  scoped_refptr<base::RefCountedMemory> response_;
};

TEST_F(SuggestionSourceTest, ShouldServiceRequest) {
  scoped_ptr<net::URLRequest> request;
  request.reset(MockRequest("http://suggestion/loader.js",
                            true, kNonInstantRendererPID, 0));
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  request.reset(MockRequest("chrome-search://bogus/loader.js",
                            true, kInstantRendererPID, 0));
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  request.reset(MockRequest("chrome-search://suggestion/bogus.js",
                            true, kInstantRendererPID, 0));
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  request.reset(MockRequest("chrome-search://suggestion/loader.js",
                            true, kInstantRendererPID, 0));
  EXPECT_TRUE(source()->ShouldServiceRequest(request.get()));
  request.reset(MockRequest("chrome-search://suggestion/loader.js",
                            true, kNonInstantRendererPID, 0));
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  request.reset(MockRequest("chrome-search://suggestion/loader.js",
                            true, kInvalidRendererPID, 0));
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
}

TEST_F(SuggestionSourceTest, GetMimeType) {
  // URLDataManagerBackend does not include / in path_and_query.
  EXPECT_EQ("text/html", source()->GetMimeType("loader.html"));
  EXPECT_EQ("application/javascript", source()->GetMimeType("loader.js"));
  EXPECT_EQ("", source()->GetMimeType("bogus.html"));
}

TEST_F(SuggestionSourceTest, StartDataRequest) {
  StartDataRequest("bogus.html", kInstantRendererPID, 0);
  EXPECT_TRUE(response_string().empty());
  StartDataRequest("bogus.html", kNonInstantRendererPID, 0);
  EXPECT_TRUE(response_string().empty());
  StartDataRequest("loader.html", kInstantRendererPID, 0);
  EXPECT_FALSE(response_string().empty());
  StartDataRequest("loader.js", kInstantRendererPID, 0);
  EXPECT_FALSE(response_string().empty());
  EXPECT_NE(std::string::npos, response_string().find(kInstantOrigin));
  StartDataRequest("loader.js", kNonInstantRendererPID, 0);
  EXPECT_FALSE(response_string().empty());
  EXPECT_NE(std::string::npos, response_string().find(kNonInstantOrigin));
  StartDataRequest("loader.js", kInvalidRendererPID, 0);
  EXPECT_TRUE(response_string().empty());
}
