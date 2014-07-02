// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/malware_details_history.h"
#include "chrome/browser/safe_browsing/report.pb.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/safe_browsing/safebrowsing_messages.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

static const char* kOriginalLandingURL = "http://www.originallandingpage.com/";
static const char* kHttpsURL = "https://www.url.com/";
static const char* kDOMChildURL = "http://www.domparent.com/";
static const char* kDOMParentURL = "http://www.domchild.com/";
static const char* kFirstRedirectURL = "http://redirectone.com/";
static const char* kSecondRedirectURL = "http://redirecttwo.com/";

static const char* kMalwareURL = "http://www.malware.com/";
static const char* kMalwareHeaders =
    "HTTP/1.1 200 OK\n"
    "Content-Type: image/jpeg\n";
static const char* kMalwareData = "exploit();";

static const char* kLandingURL = "http://www.landingpage.com/";
static const char* kLandingHeaders =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html\n"
    "Content-Length: 1024\n"
    "Set-Cookie: tastycookie\n";  // This header is stripped.
static const char* kLandingData = "<iframe src='http://www.malware.com'>";

using content::BrowserThread;
using content::WebContents;
using safe_browsing::ClientMalwareReportRequest;

namespace {

void WriteHeaders(disk_cache::Entry* entry, const std::string& headers) {
  net::HttpResponseInfo responseinfo;
  std::string raw_headers = net::HttpUtil::AssembleRawHeaders(
      headers.c_str(), headers.size());
  responseinfo.socket_address = net::HostPortPair("1.2.3.4", 80);
  responseinfo.headers = new net::HttpResponseHeaders(raw_headers);

  Pickle pickle;
  responseinfo.Persist(&pickle, false, false);

  scoped_refptr<net::WrappedIOBuffer> buf(new net::WrappedIOBuffer(
      reinterpret_cast<const char*>(pickle.data())));
  int len = static_cast<int>(pickle.size());

  net::TestCompletionCallback cb;
  int rv = entry->WriteData(0, 0, buf.get(), len, cb.callback(), true);
  ASSERT_EQ(len, cb.GetResult(rv));
}

void WriteData(disk_cache::Entry* entry, const std::string& data) {
  if (data.empty())
    return;

  int len = data.length();
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(len));
  memcpy(buf->data(), data.data(), data.length());

  net::TestCompletionCallback cb;
  int rv = entry->WriteData(1, 0, buf.get(), len, cb.callback(), true);
  ASSERT_EQ(len, cb.GetResult(rv));
}

void WriteToEntry(disk_cache::Backend* cache, const std::string& key,
                  const std::string& headers, const std::string& data) {
  net::TestCompletionCallback cb;
  disk_cache::Entry* entry;
  int rv = cache->CreateEntry(key, &entry, cb.callback());
  rv = cb.GetResult(rv);
  if (rv != net::OK) {
    rv = cache->OpenEntry(key, &entry, cb.callback());
    ASSERT_EQ(net::OK, cb.GetResult(rv));
  }

  WriteHeaders(entry, headers);
  WriteData(entry, data);
  entry->Close();
}

void FillCache(net::URLRequestContextGetter* context_getter) {
  net::TestCompletionCallback cb;
  disk_cache::Backend* cache;
  int rv =
      context_getter->GetURLRequestContext()->http_transaction_factory()->
      GetCache()->GetBackend(&cache, cb.callback());
  ASSERT_EQ(net::OK, cb.GetResult(rv));

  WriteToEntry(cache, kMalwareURL, kMalwareHeaders, kMalwareData);
  WriteToEntry(cache, kLandingURL, kLandingHeaders, kLandingData);
}

// Lets us provide a MockURLRequestContext with an HTTP Cache we pre-populate.
// Also exposes the constructor.
class MalwareDetailsWrap : public MalwareDetails {
 public:
  MalwareDetailsWrap(
      SafeBrowsingUIManager* ui_manager,
      WebContents* web_contents,
      const SafeBrowsingUIManager::UnsafeResource& unsafe_resource,
      net::URLRequestContextGetter* request_context_getter)
      : MalwareDetails(ui_manager, web_contents, unsafe_resource) {

    request_context_getter_ = request_context_getter;
  }

 private:
  virtual ~MalwareDetailsWrap() {}
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  base::RunLoop* run_loop_;
  // The safe browsing UI manager does not need a service for this test.
  MockSafeBrowsingUIManager()
      : SafeBrowsingUIManager(NULL), run_loop_(NULL) {}

  // When the MalwareDetails is done, this is called.
  virtual void SendSerializedMalwareDetails(
      const std::string& serialized) OVERRIDE {
    DVLOG(1) << "SendSerializedMalwareDetails";
    run_loop_->Quit();
    run_loop_ = NULL;
    serialized_ = serialized;
  }

  // Used to synchronize SendSerializedMalwareDetails() with
  // WaitForSerializedReport(). RunLoop::RunUntilIdle() is not sufficient
  // because the MessageLoop task queue completely drains at some point
  // between the send and the wait.
  void SetRunLoopToQuit(base::RunLoop* run_loop) {
    DCHECK(run_loop_ == NULL);
    run_loop_ = run_loop;
  }

  const std::string& GetSerialized() {
    return serialized_;
  }

 private:
  virtual ~MockSafeBrowsingUIManager() {}

  std::string serialized_;
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingUIManager);
};

}  // namespace.

class MalwareDetailsTest : public ChromeRenderViewHostTestHarness {
 public:
  typedef SafeBrowsingUIManager::UnsafeResource UnsafeResource;

  MalwareDetailsTest()
      : ui_manager_(new MockSafeBrowsingUIManager()) {
  }

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(profile()->CreateHistoryService(
        true /* delete_file */, false /* no_db */));
  }

  virtual void TearDown() OVERRIDE {
    profile()->DestroyHistoryService();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  static bool ResourceLessThan(
      const ClientMalwareReportRequest::Resource* lhs,
      const ClientMalwareReportRequest::Resource* rhs) {
    return lhs->id() < rhs->id();
  }

  std::string WaitForSerializedReport(MalwareDetails* report) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&MalwareDetails::FinishCollection, report));
    // Wait for the callback (SendSerializedMalwareDetails).
    DVLOG(1) << "Waiting for SendSerializedMalwareDetails";
    base::RunLoop run_loop;
    ui_manager_->SetRunLoopToQuit(&run_loop);
    run_loop.Run();
    return ui_manager_->GetSerialized();
  }

  HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(profile(),
                                                Profile::EXPLICIT_ACCESS);
  }

 protected:
  void InitResource(UnsafeResource* resource,
                    bool is_subresource,
                    const GURL& url) {
    resource->url = url;
    resource->is_subresource = is_subresource;
    resource->threat_type = SB_THREAT_TYPE_URL_MALWARE;
    resource->render_process_host_id =
        web_contents()->GetRenderProcessHost()->GetID();
    resource->render_view_id =
        web_contents()->GetRenderViewHost()->GetRoutingID();
  }

  void VerifyResults(const ClientMalwareReportRequest& report_pb,
                     const ClientMalwareReportRequest& expected_pb) {
    EXPECT_EQ(expected_pb.malware_url(), report_pb.malware_url());
    EXPECT_EQ(expected_pb.page_url(), report_pb.page_url());
    EXPECT_EQ(expected_pb.referrer_url(), report_pb.referrer_url());

    ASSERT_EQ(expected_pb.resources_size(), report_pb.resources_size());
    // Sort the resources, to make the test deterministic
    std::vector<const ClientMalwareReportRequest::Resource*> resources;
    for (int i = 0; i < report_pb.resources_size(); ++i) {
      const ClientMalwareReportRequest::Resource& resource =
          report_pb.resources(i);
      resources.push_back(&resource);
    }
    std::sort(resources.begin(), resources.end(),
              &MalwareDetailsTest::ResourceLessThan);

    std::vector<const ClientMalwareReportRequest::Resource*> expected;
    for (int i = 0; i < report_pb.resources_size(); ++i) {
      const ClientMalwareReportRequest::Resource& resource =
          expected_pb.resources(i);
      expected.push_back(&resource);
    }
    std::sort(expected.begin(), expected.end(),
              &MalwareDetailsTest::ResourceLessThan);

    for (uint32 i = 0; i < expected.size(); ++i) {
      VerifyResource(resources[i], expected[i]);
    }

    EXPECT_EQ(expected_pb.complete(), report_pb.complete());
  }

  void VerifyResource(const ClientMalwareReportRequest::Resource* resource,
                      const ClientMalwareReportRequest::Resource* expected) {
    EXPECT_EQ(expected->id(), resource->id());
    EXPECT_EQ(expected->url(), resource->url());
    EXPECT_EQ(expected->parent_id(), resource->parent_id());
    ASSERT_EQ(expected->child_ids_size(), resource->child_ids_size());
    for (int i = 0; i < expected->child_ids_size(); i++) {
      EXPECT_EQ(expected->child_ids(i), resource->child_ids(i));
    }

    // Verify HTTP Responses
    if (expected->has_response()) {
      ASSERT_TRUE(resource->has_response());
      EXPECT_EQ(expected->response().firstline().code(),
                resource->response().firstline().code());

      ASSERT_EQ(expected->response().headers_size(),
                resource->response().headers_size());
      for (int i = 0; i < expected->response().headers_size(); ++i) {
        EXPECT_EQ(expected->response().headers(i).name(),
                  resource->response().headers(i).name());
        EXPECT_EQ(expected->response().headers(i).value(),
                  resource->response().headers(i).value());
      }

      EXPECT_EQ(expected->response().body(), resource->response().body());
      EXPECT_EQ(expected->response().bodylength(),
                resource->response().bodylength());
      EXPECT_EQ(expected->response().bodydigest(),
                resource->response().bodydigest());
    }

    // Verify IP:port pair
    EXPECT_EQ(expected->response().remote_ip(),
              resource->response().remote_ip());
  }

  // Adds a page to history.
  // The redirects is the redirect url chain leading to the url.
  void AddPageToHistory(const GURL& url,
                        history::RedirectList* redirects) {
    // The last item of the redirect chain has to be the final url when adding
    // to history backend.
    redirects->push_back(url);
    history_service()->AddPage(
        url, base::Time::Now(), reinterpret_cast<history::ContextID>(1), 0,
        GURL(), *redirects, content::PAGE_TRANSITION_TYPED,
        history::SOURCE_BROWSED, false);
  }

  scoped_refptr<MockSafeBrowsingUIManager> ui_manager_;
};

// Tests creating a simple malware report.
TEST_F(MalwareDetailsTest, MalwareSubResource) {
  // Start a load.
  controller().LoadURL(GURL(kLandingURL), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());

  UnsafeResource resource;
  InitResource(&resource, true, GURL(kMalwareURL));

  scoped_refptr<MalwareDetailsWrap> report =
      new MalwareDetailsWrap(ui_manager_.get(), web_contents(), resource, NULL);

  std::string serialized = WaitForSerializedReport(report.get());

  ClientMalwareReportRequest actual;
  actual.ParseFromString(serialized);

  ClientMalwareReportRequest expected;
  expected.set_malware_url(kMalwareURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");

  ClientMalwareReportRequest::Resource* pb_resource = expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kMalwareURL);

  VerifyResults(actual, expected);
}

// Tests creating a simple malware report where the subresource has a
// different original_url.
TEST_F(MalwareDetailsTest, MalwareSubResourceWithOriginalUrl) {
  controller().LoadURL(GURL(kLandingURL), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());

  UnsafeResource resource;
  InitResource(&resource, true, GURL(kMalwareURL));
  resource.original_url = GURL(kOriginalLandingURL);

  scoped_refptr<MalwareDetailsWrap> report = new MalwareDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL);

  std::string serialized = WaitForSerializedReport(report.get());

  ClientMalwareReportRequest actual;
  actual.ParseFromString(serialized);

  ClientMalwareReportRequest expected;
  expected.set_malware_url(kMalwareURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");

  ClientMalwareReportRequest::Resource* pb_resource = expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kOriginalLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kMalwareURL);
  // The Resource for kMmalwareUrl should have the Resource for
  // kOriginalLandingURL (with id 1) as parent.
  pb_resource->set_parent_id(1);

  VerifyResults(actual, expected);
}

// Tests creating a malware report with data from the renderer.
TEST_F(MalwareDetailsTest, MalwareDOMDetails) {
  controller().LoadURL(GURL(kLandingURL), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());

  UnsafeResource resource;
  InitResource(&resource, true, GURL(kMalwareURL));

  scoped_refptr<MalwareDetailsWrap> report = new MalwareDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL);

  // Send a message from the DOM, with 2 nodes, a parent and a child.
  std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node> params;
  SafeBrowsingHostMsg_MalwareDOMDetails_Node child_node;
  child_node.url = GURL(kDOMChildURL);
  child_node.tag_name = "iframe";
  child_node.parent = GURL(kDOMParentURL);
  params.push_back(child_node);
  SafeBrowsingHostMsg_MalwareDOMDetails_Node parent_node;
  parent_node.url = GURL(kDOMParentURL);
  parent_node.children.push_back(GURL(kDOMChildURL));
  params.push_back(parent_node);
  report->OnReceivedMalwareDOMDetails(params);

  std::string serialized = WaitForSerializedReport(report.get());
  ClientMalwareReportRequest actual;
  actual.ParseFromString(serialized);

  ClientMalwareReportRequest expected;
  expected.set_malware_url(kMalwareURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");

  ClientMalwareReportRequest::Resource* pb_resource = expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kMalwareURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kDOMChildURL);
  pb_resource->set_parent_id(3);

  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kDOMParentURL);
  pb_resource->add_child_ids(2);
  expected.set_complete(false);  // Since the cache was missing.

  VerifyResults(actual, expected);
}

// Verify that https:// urls are dropped.
TEST_F(MalwareDetailsTest, NotPublicUrl) {
  controller().LoadURL(GURL(kHttpsURL), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());
  UnsafeResource resource;
  InitResource(&resource, true, GURL(kMalwareURL));
  scoped_refptr<MalwareDetailsWrap> report = new MalwareDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL);

  std::string serialized = WaitForSerializedReport(report.get());
  ClientMalwareReportRequest actual;
  actual.ParseFromString(serialized);

  ClientMalwareReportRequest expected;
  expected.set_malware_url(kMalwareURL);  // No page_url
  expected.set_referrer_url("");

  ClientMalwareReportRequest::Resource* pb_resource = expected.add_resources();
  pb_resource->set_url(kMalwareURL);  // Only one resource

  VerifyResults(actual, expected);
}

// Tests creating a malware report where there are redirect urls to an unsafe
// resource url
TEST_F(MalwareDetailsTest, MalwareWithRedirectUrl) {
  controller().LoadURL(GURL(kLandingURL), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());

  UnsafeResource resource;
  InitResource(&resource, true, GURL(kMalwareURL));
  resource.original_url = GURL(kOriginalLandingURL);

  // add some redirect urls
  resource.redirect_urls.push_back(GURL(kFirstRedirectURL));
  resource.redirect_urls.push_back(GURL(kSecondRedirectURL));
  resource.redirect_urls.push_back(GURL(kMalwareURL));

  scoped_refptr<MalwareDetailsWrap> report = new MalwareDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL);

  std::string serialized = WaitForSerializedReport(report.get());
  ClientMalwareReportRequest actual;
  actual.ParseFromString(serialized);

  ClientMalwareReportRequest expected;
  expected.set_malware_url(kMalwareURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");

  ClientMalwareReportRequest::Resource* pb_resource = expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kOriginalLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kMalwareURL);
  pb_resource->set_parent_id(4);

  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kFirstRedirectURL);
  pb_resource->set_parent_id(1);

  pb_resource = expected.add_resources();
  pb_resource->set_id(4);
  pb_resource->set_url(kSecondRedirectURL);
  pb_resource->set_parent_id(3);

  VerifyResults(actual, expected);
}

// Tests the interaction with the HTTP cache.
TEST_F(MalwareDetailsTest, HTTPCache) {
  controller().LoadURL(GURL(kLandingURL), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());

  UnsafeResource resource;
  InitResource(&resource, true, GURL(kMalwareURL));

  scoped_refptr<MalwareDetailsWrap> report = new MalwareDetailsWrap(
      ui_manager_.get(), web_contents(), resource,
      profile()->GetRequestContext());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&FillCache,
                 make_scoped_refptr(profile()->GetRequestContext())));

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node> params;
  report->OnReceivedMalwareDOMDetails(params);

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForSerializedReport(report.get());
  ClientMalwareReportRequest actual;
  actual.ParseFromString(serialized);

  ClientMalwareReportRequest expected;
  expected.set_malware_url(kMalwareURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");

  ClientMalwareReportRequest::Resource* pb_resource = expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  safe_browsing::ClientMalwareReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  safe_browsing::ClientMalwareReportRequest::HTTPHeader* pb_header =
      pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("text/html");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Length");
  pb_header->set_value("1024");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Set-Cookie");
  pb_header->set_value("");  // The cookie is dropped.
  pb_response->set_body(kLandingData);
  pb_response->set_bodylength(37);
  pb_response->set_bodydigest("9ca97475598a79bc1e8fc9bd6c72cd35");
  pb_response->set_remote_ip("1.2.3.4:80");

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kMalwareURL);
  pb_response = pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("image/jpeg");
  pb_response->set_body(kMalwareData);
  pb_response->set_bodylength(10);
  pb_response->set_bodydigest("581373551c43d4cf33bfb3b26838ff95");
  pb_response->set_remote_ip("1.2.3.4:80");
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Tests the interaction with the HTTP cache (where the cache is empty).
TEST_F(MalwareDetailsTest, HTTPCacheNoEntries) {
  controller().LoadURL(GURL(kLandingURL), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());

  UnsafeResource resource;
  InitResource(&resource, true, GURL(kMalwareURL));

  scoped_refptr<MalwareDetailsWrap> report = new MalwareDetailsWrap(
      ui_manager_.get(), web_contents(), resource,
      profile()->GetRequestContext());

  // No call to FillCache

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node> params;
  report->OnReceivedMalwareDOMDetails(params);

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForSerializedReport(report.get());
  ClientMalwareReportRequest actual;
  actual.ParseFromString(serialized);

  ClientMalwareReportRequest expected;
  expected.set_malware_url(kMalwareURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");

  ClientMalwareReportRequest::Resource* pb_resource = expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kMalwareURL);
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Test getting redirects from history service.
TEST_F(MalwareDetailsTest, HistoryServiceUrls) {
  // Add content to history service.
  // There are two redirect urls before reacing malware url:
  // kFirstRedirectURL -> kSecondRedirectURL -> kMalwareURL
  GURL baseurl(kMalwareURL);
  history::RedirectList redirects;
  redirects.push_back(GURL(kFirstRedirectURL));
  redirects.push_back(GURL(kSecondRedirectURL));
  AddPageToHistory(baseurl, &redirects);
  // Wait for history service operation finished.
  profile()->BlockUntilHistoryProcessesPendingRequests();

  controller().LoadURL(GURL(kLandingURL), content::Referrer(),
                       content::PAGE_TRANSITION_TYPED, std::string());

  UnsafeResource resource;
  InitResource(&resource, true, GURL(kMalwareURL));
  scoped_refptr<MalwareDetailsWrap> report = new MalwareDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL);

  // The redirects collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node> params;
  report->OnReceivedMalwareDOMDetails(params);

  // Let the redirects callbacks complete.
  base::RunLoop().RunUntilIdle();

  std::string serialized = WaitForSerializedReport(report.get());
  ClientMalwareReportRequest actual;
  actual.ParseFromString(serialized);

  ClientMalwareReportRequest expected;
  expected.set_malware_url(kMalwareURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");

  ClientMalwareReportRequest::Resource* pb_resource = expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_parent_id(2);
  pb_resource->set_url(kMalwareURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_parent_id(3);
  pb_resource->set_url(kSecondRedirectURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kFirstRedirectURL);

  VerifyResults(actual, expected);
}
