// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/browser/threat_details.h"
#include "components/safe_browsing/browser/threat_details_history.h"
#include "components/safe_browsing/common/safebrowsing_messages.h"
#include "components/safe_browsing/csd.pb.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
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
#include "testing/gmock/include/gmock/gmock-matchers.h"

using content::BrowserThread;
using content::WebContents;
using testing::Eq;
using testing::UnorderedPointwise;

namespace safe_browsing {

namespace {

// Mixture of HTTP and HTTPS.  No special treatment for HTTPS.
static const char* kOriginalLandingURL =
    "http://www.originallandingpage.com/with/path";
static const char* kDOMChildURL = "https://www.domchild.com/with/path";
static const char* kDOMChildUrl2 = "https://www.domchild2.com/path";
static const char* kDOMParentURL = "https://www.domparent.com/with/path";
static const char* kFirstRedirectURL = "http://redirectone.com/with/path";
static const char* kSecondRedirectURL = "https://redirecttwo.com/with/path";
static const char* kReferrerURL = "http://www.referrer.com/with/path";
static const char* kDataURL = "data:text/html;charset=utf-8;base64,PCFET0";
static const char* kBlankURL = "about:blank";

static const char* kThreatURL = "http://www.threat.com/with/path";
static const char* kThreatURLHttps = "https://www.threat.com/with/path";
static const char* kThreatHeaders =
    "HTTP/1.1 200 OK\n"
    "Content-Type: image/jpeg\n"
    "Some-Other-Header: foo\n";  // Persisted for http, stripped for https
static const char* kThreatData = "exploit();";

static const char* kLandingURL = "http://www.landingpage.com/with/path";
static const char* kLandingHeaders =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html\n"
    "Content-Length: 1024\n"
    "Set-Cookie: tastycookie\n";  // This header is stripped.
static const char* kLandingData =
    "<iframe src='http://www.threat.com/with/path'>";

using content::BrowserThread;
using content::WebContents;

void WriteHeaders(disk_cache::Entry* entry, const std::string& headers) {
  net::HttpResponseInfo responseinfo;
  std::string raw_headers =
      net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size());
  responseinfo.socket_address = net::HostPortPair("1.2.3.4", 80);
  responseinfo.headers = new net::HttpResponseHeaders(raw_headers);

  base::Pickle pickle;
  responseinfo.Persist(&pickle, false, false);

  scoped_refptr<net::WrappedIOBuffer> buf(
      new net::WrappedIOBuffer(reinterpret_cast<const char*>(pickle.data())));
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

void WriteToEntry(disk_cache::Backend* cache,
                  const std::string& key,
                  const std::string& headers,
                  const std::string& data) {
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

void FillCacheBase(net::URLRequestContextGetter* context_getter,
                   bool use_https_threat_url) {
  net::TestCompletionCallback cb;
  disk_cache::Backend* cache;
  int rv = context_getter->GetURLRequestContext()
               ->http_transaction_factory()
               ->GetCache()
               ->GetBackend(&cache, cb.callback());
  ASSERT_EQ(net::OK, cb.GetResult(rv));

  WriteToEntry(cache, use_https_threat_url ? kThreatURLHttps : kThreatURL,
               kThreatHeaders, kThreatData);
  WriteToEntry(cache, kLandingURL, kLandingHeaders, kLandingData);
}
void FillCache(net::URLRequestContextGetter* context_getter) {
  FillCacheBase(context_getter, /*use_https_threat_url=*/false);
}
void FillCacheHttps(net::URLRequestContextGetter* context_getter) {
  FillCacheBase(context_getter, /*use_https_threat_url=*/true);
}

// Lets us provide a MockURLRequestContext with an HTTP Cache we pre-populate.
// Also exposes the constructor.
class ThreatDetailsWrap : public ThreatDetails {
 public:
  ThreatDetailsWrap(
      SafeBrowsingUIManager* ui_manager,
      WebContents* web_contents,
      const security_interstitials::UnsafeResource& unsafe_resource,
      net::URLRequestContextGetter* request_context_getter,
      history::HistoryService* history_service)
      : ThreatDetails(ui_manager,
                      web_contents,
                      unsafe_resource,
                      request_context_getter,
                      history_service) {
    request_context_getter_ = request_context_getter;
  }

 private:
  ~ThreatDetailsWrap() override {}
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  base::RunLoop* run_loop_;
  // The safe browsing UI manager does not need a service for this test.
  MockSafeBrowsingUIManager() : SafeBrowsingUIManager(NULL), run_loop_(NULL) {}

  // When the ThreatDetails is done, this is called.
  void SendSerializedThreatDetails(const std::string& serialized) override {
    DVLOG(1) << "SendSerializedThreatDetails";
    run_loop_->Quit();
    run_loop_ = NULL;
    serialized_ = serialized;
  }

  // Used to synchronize SendSerializedThreatDetails() with
  // WaitForSerializedReport(). RunLoop::RunUntilIdle() is not sufficient
  // because the MessageLoop task queue completely drains at some point
  // between the send and the wait.
  void SetRunLoopToQuit(base::RunLoop* run_loop) {
    DCHECK(run_loop_ == NULL);
    run_loop_ = run_loop;
  }

  const std::string& GetSerialized() { return serialized_; }

 private:
  ~MockSafeBrowsingUIManager() override {}

  std::string serialized_;
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingUIManager);
};

}  // namespace

class ThreatDetailsTest : public ChromeRenderViewHostTestHarness {
 public:
  typedef SafeBrowsingUIManager::UnsafeResource UnsafeResource;

  ThreatDetailsTest() : ui_manager_(new MockSafeBrowsingUIManager()) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(profile()->CreateHistoryService(true /* delete_file */,
                                                false /* no_db */));
  }

  std::string WaitForSerializedReport(ThreatDetails* report,
                                      bool did_proceed,
                                      int num_visit) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::BindOnce(&ThreatDetails::FinishCollection,
                                           report, did_proceed, num_visit));
    // Wait for the callback (SendSerializedThreatDetails).
    DVLOG(1) << "Waiting for SendSerializedThreatDetails";
    base::RunLoop run_loop;
    ui_manager_->SetRunLoopToQuit(&run_loop);
    run_loop.Run();
    return ui_manager_->GetSerialized();
  }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

 protected:
  void InitResource(SBThreatType threat_type,
                    ThreatSource threat_source,
                    bool is_subresource,
                    const GURL& url,
                    UnsafeResource* resource) {
    resource->url = url;
    resource->is_subresource = is_subresource;
    resource->threat_type = threat_type;
    resource->threat_source = threat_source;
    resource->web_contents_getter =
        SafeBrowsingUIManager::UnsafeResource::GetWebContentsGetter(
            web_contents()->GetRenderProcessHost()->GetID(),
            web_contents()->GetMainFrame()->GetRoutingID());
  }

  void VerifyResults(const ClientSafeBrowsingReportRequest& report_pb,
                     const ClientSafeBrowsingReportRequest& expected_pb) {
    EXPECT_EQ(expected_pb.type(), report_pb.type());
    EXPECT_EQ(expected_pb.url(), report_pb.url());
    EXPECT_EQ(expected_pb.page_url(), report_pb.page_url());
    EXPECT_EQ(expected_pb.referrer_url(), report_pb.referrer_url());
    EXPECT_EQ(expected_pb.did_proceed(), report_pb.did_proceed());
    EXPECT_EQ(expected_pb.has_repeat_visit(), report_pb.has_repeat_visit());
    if (expected_pb.has_repeat_visit() && report_pb.has_repeat_visit()) {
      EXPECT_EQ(expected_pb.repeat_visit(), report_pb.repeat_visit());
    }

    ASSERT_EQ(expected_pb.resources_size(), report_pb.resources_size());
    // Put the actual resources in a map, then iterate over the expected
    // resources, making sure each exists and is equal.
    base::hash_map<int, const ClientSafeBrowsingReportRequest::Resource*>
        actual_resource_map;
    for (const ClientSafeBrowsingReportRequest::Resource& resource :
         report_pb.resources()) {
      actual_resource_map[resource.id()] = &resource;
    }
    // Make sure no resources disappeared when moving them to a map (IDs should
    // be unique).
    ASSERT_EQ(expected_pb.resources_size(),
              static_cast<int>(actual_resource_map.size()));
    for (const ClientSafeBrowsingReportRequest::Resource& expected_resource :
         expected_pb.resources()) {
      ASSERT_TRUE(actual_resource_map.count(expected_resource.id()) > 0);
      VerifyResource(*actual_resource_map[expected_resource.id()],
                     expected_resource);
    }

    ASSERT_EQ(expected_pb.dom_size(), report_pb.dom_size());
    // Put the actual elements in a map, then iterate over the expected
    // elements, making sure each exists and is equal.
    base::hash_map<int, const HTMLElement*> actual_dom_map;
    for (const HTMLElement& element : report_pb.dom()) {
      actual_dom_map[element.id()] = &element;
    }
    // Make sure no elements disappeared when moving them to a map (IDs should
    // be unique).
    ASSERT_EQ(expected_pb.dom_size(), static_cast<int>(actual_dom_map.size()));
    for (const HTMLElement& expected_element : expected_pb.dom()) {
      ASSERT_TRUE(actual_dom_map.count(expected_element.id()) > 0);
      VerifyElement(*actual_dom_map[expected_element.id()], expected_element);
    }

    EXPECT_TRUE(report_pb.client_properties().has_url_api_type());
    EXPECT_EQ(expected_pb.client_properties().url_api_type(),
              report_pb.client_properties().url_api_type());
    EXPECT_EQ(expected_pb.complete(), report_pb.complete());
  }

  void VerifyResource(
      const ClientSafeBrowsingReportRequest::Resource& resource,
      const ClientSafeBrowsingReportRequest::Resource& expected) {
    EXPECT_EQ(expected.id(), resource.id());
    EXPECT_EQ(expected.url(), resource.url());
    EXPECT_EQ(expected.parent_id(), resource.parent_id());
    ASSERT_EQ(expected.child_ids_size(), resource.child_ids_size());
    for (int i = 0; i < expected.child_ids_size(); i++) {
      EXPECT_EQ(expected.child_ids(i), resource.child_ids(i));
    }

    // Verify HTTP Responses
    if (expected.has_response()) {
      ASSERT_TRUE(resource.has_response());
      EXPECT_EQ(expected.response().firstline().code(),
                resource.response().firstline().code());

      ASSERT_EQ(expected.response().headers_size(),
                resource.response().headers_size());
      for (int i = 0; i < expected.response().headers_size(); ++i) {
        EXPECT_EQ(expected.response().headers(i).name(),
                  resource.response().headers(i).name());
        EXPECT_EQ(expected.response().headers(i).value(),
                  resource.response().headers(i).value());
      }

      EXPECT_EQ(expected.response().body(), resource.response().body());
      EXPECT_EQ(expected.response().bodylength(),
                resource.response().bodylength());
      EXPECT_EQ(expected.response().bodydigest(),
                resource.response().bodydigest());
    }

    // Verify IP:port pair
    EXPECT_EQ(expected.response().remote_ip(), resource.response().remote_ip());
  }

  void VerifyElement(const HTMLElement& element, const HTMLElement& expected) {
    EXPECT_EQ(expected.id(), element.id());
    EXPECT_EQ(expected.tag(), element.tag());
    EXPECT_EQ(expected.resource_id(), element.resource_id());
    EXPECT_THAT(element.child_ids(),
                UnorderedPointwise(Eq(), expected.child_ids()));
    ASSERT_EQ(expected.attribute_size(), element.attribute_size());
    base::hash_map<std::string, std::string> actual_attributes_map;
    for (const HTMLElement::Attribute& attribute : element.attribute()) {
      actual_attributes_map[attribute.name()] = attribute.value();
    }
    ASSERT_EQ(expected.attribute_size(),
              static_cast<int>(actual_attributes_map.size()));
    for (const HTMLElement::Attribute& expected_attribute :
         expected.attribute()) {
      ASSERT_TRUE(actual_attributes_map.count(expected_attribute.name()) > 0);
      EXPECT_EQ(expected_attribute.value(),
                actual_attributes_map[expected_attribute.name()]);
    }
  }
  // Adds a page to history.
  // The redirects is the redirect url chain leading to the url.
  void AddPageToHistory(const GURL& url, history::RedirectList* redirects) {
    // The last item of the redirect chain has to be the final url when adding
    // to history backend.
    redirects->push_back(url);
    history_service()->AddPage(url, base::Time::Now(),
                               reinterpret_cast<history::ContextID>(1), 0,
                               GURL(), *redirects, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, false);
  }

  scoped_refptr<MockSafeBrowsingUIManager> ui_manager_;
};

// Tests creating a simple threat report of a malware URL.
TEST_F(ThreatDetailsTest, ThreatSubResource) {
  // Commit a load.
  content::WebContentsTester::For(web_contents())
      ->TestDidNavigateWithReferrer(
          web_contents()->GetMainFrame(), 0 /* nav_entry_id */,
          true /* did_create_new_entry */, GURL(kLandingURL),
          content::Referrer(GURL(kReferrerURL),
                            blink::kWebReferrerPolicyDefault),
          ui::PAGE_TRANSITION_TYPED);

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::CLIENT_SIDE_DETECTION,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL, history_service());

  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  // Note that the referrer policy is not actually enacted here, since that's
  // done in Blink.
  expected.set_referrer_url(kReferrerURL);
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kReferrerURL);

  VerifyResults(actual, expected);
}

// Tests creating a simple threat report of a phishing page where the
// subresource has a different original_url.
TEST_F(ThreatDetailsTest, ThreatSubResourceWithOriginalUrl) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_PHISHING, ThreatSource::DATA_SAVER,
               true /* is_subresource */, GURL(kThreatURL), &resource);
  resource.original_url = GURL(kOriginalLandingURL);

  scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL, history_service());

  std::string serialized = WaitForSerializedReport(
      report.get(), false /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_PHISHING);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::FLYWHEEL);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kOriginalLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kThreatURL);
  // The Resource for kThreatURL should have the Resource for
  // kOriginalLandingURL (with id 1) as parent.
  pb_resource->set_parent_id(1);

  VerifyResults(actual, expected);
}

// Tests creating a threat report of a UwS page with data from the renderer.
TEST_F(ThreatDetailsTest, ThreatDOMDetails) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_UNWANTED, ThreatSource::LOCAL_PVER3,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL, history_service());

  // Send a message from the DOM, with 2 nodes, a parent and a child.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
  SafeBrowsingHostMsg_ThreatDOMDetails_Node child_node;
  child_node.url = GURL(kDOMChildURL);
  child_node.tag_name = "iframe";
  child_node.parent = GURL(kDOMParentURL);
  child_node.attributes.push_back(std::make_pair("src", kDOMChildURL));
  params.push_back(child_node);
  SafeBrowsingHostMsg_ThreatDOMDetails_Node parent_node;
  parent_node.url = GURL(kDOMParentURL);
  parent_node.children.push_back(GURL(kDOMChildURL));
  params.push_back(parent_node);
  report->OnReceivedThreatDOMDetails(main_rfh(), params);

  std::string serialized = WaitForSerializedReport(
      report.get(), false /* did_proceed*/, 0 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_UNWANTED);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER3_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kDOMChildURL);
  pb_resource->set_parent_id(3);

  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kDOMParentURL);
  pb_resource->add_child_ids(2);
  expected.set_complete(false);  // Since the cache was missing.

  HTMLElement* pb_element = expected.add_dom();
  pb_element->set_id(0);
  pb_element->set_tag("IFRAME");
  pb_element->set_resource_id(2);
  pb_element->add_attribute()->set_name("src");
  pb_element->mutable_attribute(0)->set_value(kDOMChildURL);

  VerifyResults(actual, expected);
}

// Tests creating a threat report when receiving data from multiple renderers.
// We use three layers in this test:
// kDOMParentURL
//  \- <div id=outer>
//    \- <iframe src=kDOMChildURL foo=bar>
//      \- <div id=inner bar=baz/> - div and script are at the same level.
//      \- <script src=kDOMChildURL2>
TEST_F(ThreatDetailsTest, ThreatDOMDetails_MultipleFrames) {
  // Create a child renderer inside the main frame to house the inner iframe.
  // Perform the navigation first in order to manipulate the frame tree.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");

  // Define two sets of DOM nodes - one for an outer page containing an iframe,
  // and then another for the inner page containing the contents of that iframe.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> outer_params;
  SafeBrowsingHostMsg_ThreatDOMDetails_Node outer_child_div;
  outer_child_div.node_id = 1;
  outer_child_div.child_node_ids.push_back(2);
  outer_child_div.tag_name = "div";
  outer_child_div.parent = GURL(kDOMParentURL);
  outer_child_div.attributes.push_back(std::make_pair("id", "outer"));
  outer_params.push_back(outer_child_div);

  SafeBrowsingHostMsg_ThreatDOMDetails_Node outer_child_iframe;
  outer_child_iframe.node_id = 2;
  outer_child_iframe.parent_node_id = 1;
  outer_child_iframe.url = GURL(kDOMChildURL);
  outer_child_iframe.tag_name = "iframe";
  outer_child_iframe.parent = GURL(kDOMParentURL);
  outer_child_iframe.attributes.push_back(std::make_pair("src", kDOMChildURL));
  outer_child_iframe.attributes.push_back(std::make_pair("foo", "bar"));
  outer_child_iframe.child_frame_routing_id = child_rfh->GetRoutingID();
  outer_params.push_back(outer_child_iframe);

  SafeBrowsingHostMsg_ThreatDOMDetails_Node outer_summary_node;
  outer_summary_node.url = GURL(kDOMParentURL);
  outer_summary_node.children.push_back(GURL(kDOMChildURL));
  outer_params.push_back(outer_summary_node);

  // Now define some more nodes for the body of the iframe.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> inner_params;
  SafeBrowsingHostMsg_ThreatDOMDetails_Node inner_child_div;
  inner_child_div.node_id = 1;
  inner_child_div.tag_name = "div";
  inner_child_div.parent = GURL(kDOMChildURL);
  inner_child_div.attributes.push_back(std::make_pair("id", "inner"));
  inner_child_div.attributes.push_back(std::make_pair("bar", "baz"));
  inner_params.push_back(inner_child_div);

  SafeBrowsingHostMsg_ThreatDOMDetails_Node inner_child_script;
  inner_child_script.node_id = 2;
  inner_child_script.url = GURL(kDOMChildUrl2);
  inner_child_script.tag_name = "script";
  inner_child_script.parent = GURL(kDOMChildURL);
  inner_child_script.attributes.push_back(std::make_pair("src", kDOMChildUrl2));
  inner_params.push_back(inner_child_script);

  SafeBrowsingHostMsg_ThreatDOMDetails_Node inner_summary_node;
  inner_summary_node.url = GURL(kDOMChildURL);
  inner_summary_node.children.push_back(GURL(kDOMChildUrl2));
  inner_params.push_back(inner_summary_node);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_UNWANTED);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER4_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);

  ClientSafeBrowsingReportRequest::Resource* res_dom_child =
      expected.add_resources();
  res_dom_child->set_id(2);
  res_dom_child->set_url(kDOMChildURL);
  res_dom_child->set_parent_id(3);
  res_dom_child->add_child_ids(4);

  ClientSafeBrowsingReportRequest::Resource* res_dom_parent =
      expected.add_resources();
  res_dom_parent->set_id(3);
  res_dom_parent->set_url(kDOMParentURL);
  res_dom_parent->add_child_ids(2);

  ClientSafeBrowsingReportRequest::Resource* res_dom_child2 =
      expected.add_resources();
  res_dom_child2->set_id(4);
  res_dom_child2->set_url(kDOMChildUrl2);
  res_dom_child2->set_parent_id(2);

  expected.set_complete(false);  // Since the cache was missing.

  HTMLElement* elem_dom_outer_div = expected.add_dom();
  elem_dom_outer_div->set_id(0);
  elem_dom_outer_div->set_tag("DIV");
  elem_dom_outer_div->add_attribute()->set_name("id");
  elem_dom_outer_div->mutable_attribute(0)->set_value("outer");
  elem_dom_outer_div->add_child_ids(1);

  HTMLElement* elem_dom_outer_iframe = expected.add_dom();
  elem_dom_outer_iframe->set_id(1);
  elem_dom_outer_iframe->set_tag("IFRAME");
  elem_dom_outer_iframe->set_resource_id(res_dom_child->id());
  elem_dom_outer_iframe->add_attribute()->set_name("src");
  elem_dom_outer_iframe->mutable_attribute(0)->set_value(kDOMChildURL);
  elem_dom_outer_iframe->add_attribute()->set_name("foo");
  elem_dom_outer_iframe->mutable_attribute(1)->set_value("bar");
  elem_dom_outer_iframe->add_child_ids(2);
  elem_dom_outer_iframe->add_child_ids(3);

  HTMLElement* elem_dom_inner_div = expected.add_dom();
  elem_dom_inner_div->set_id(2);
  elem_dom_inner_div->set_tag("DIV");
  elem_dom_inner_div->add_attribute()->set_name("id");
  elem_dom_inner_div->mutable_attribute(0)->set_value("inner");
  elem_dom_inner_div->add_attribute()->set_name("bar");
  elem_dom_inner_div->mutable_attribute(1)->set_value("baz");

  HTMLElement* elem_dom_inner_script = expected.add_dom();
  elem_dom_inner_script->set_id(3);
  elem_dom_inner_script->set_tag("SCRIPT");
  elem_dom_inner_script->set_resource_id(res_dom_child2->id());
  elem_dom_inner_script->add_attribute()->set_name("src");
  elem_dom_inner_script->mutable_attribute(0)->set_value(kDOMChildUrl2);

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_UNWANTED, ThreatSource::LOCAL_PVER4,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  // Send both sets of nodes, from different render frames.
  {
    scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
        ui_manager_.get(), web_contents(), resource, NULL, history_service());

    // Send both sets of nodes from different render frames.
    report->OnReceivedThreatDOMDetails(main_rfh(), outer_params);
    report->OnReceivedThreatDOMDetails(child_rfh, inner_params);

    std::string serialized = WaitForSerializedReport(
        report.get(), false /* did_proceed*/, 0 /* num_visit */);
    ClientSafeBrowsingReportRequest actual;
    actual.ParseFromString(serialized);
    VerifyResults(actual, expected);
  }

  // Try again but with the messages coming in a different order. The IDs change
  // slightly, but everything else remains the same.
  {
    // Adjust the expected IDs: the inner params come first, so InnerScript
    // and InnerDiv appear before DomParent
    res_dom_child2->set_id(2);
    res_dom_child2->set_parent_id(3);
    res_dom_child->set_id(3);
    res_dom_child->set_parent_id(4);
    res_dom_child->clear_child_ids();
    res_dom_child->add_child_ids(2);
    res_dom_parent->set_id(4);
    res_dom_parent->clear_child_ids();
    res_dom_parent->add_child_ids(3);

    // Also adjust the elements - they change order since InnerDiv and
    // InnerScript come in first.
    elem_dom_inner_div->set_id(0);
    elem_dom_inner_script->set_id(1);
    elem_dom_inner_script->set_resource_id(res_dom_child2->id());

    elem_dom_outer_div->set_id(2);
    elem_dom_outer_div->clear_child_ids();
    elem_dom_outer_div->add_child_ids(3);
    elem_dom_outer_iframe->set_id(3);
    elem_dom_outer_iframe->set_resource_id(res_dom_child->id());
    elem_dom_outer_iframe->clear_child_ids();
    elem_dom_outer_iframe->add_child_ids(0);
    elem_dom_outer_iframe->add_child_ids(1);

    scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
        ui_manager_.get(), web_contents(), resource, NULL, history_service());

    // Send both sets of nodes from different render frames.
    report->OnReceivedThreatDOMDetails(child_rfh, inner_params);
    report->OnReceivedThreatDOMDetails(main_rfh(), outer_params);

    std::string serialized = WaitForSerializedReport(
        report.get(), false /* did_proceed*/, 0 /* num_visit */);
    ClientSafeBrowsingReportRequest actual;
    actual.ParseFromString(serialized);
    VerifyResults(actual, expected);
  }
}

// Tests an ambiguous DOM, meaning that an inner render frame can not be mapped
// to an iframe element in the parent frame, which is a failure to lookup the
// frames in the frame tree and should not happen.
// We use three layers in this test:
// kDOMParentURL
//   \- <frame src=kDataURL>
//        \- <script src=kDOMChildURL2>
TEST_F(ThreatDetailsTest, ThreatDOMDetails_AmbiguousDOM) {
  const char kAmbiguousDomMetric[] = "SafeBrowsing.ThreatReport.DomIsAmbiguous";

  // Create a child renderer inside the main frame to house the inner iframe.
  // Perform the navigation first in order to manipulate the frame tree.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");

  // Define two sets of DOM nodes - one for an outer page containing a frame,
  // and then another for the inner page containing the contents of that frame.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> outer_params;
  SafeBrowsingHostMsg_ThreatDOMDetails_Node outer_child_node;
  outer_child_node.url = GURL(kDataURL);
  outer_child_node.tag_name = "frame";
  outer_child_node.parent = GURL(kDOMParentURL);
  outer_child_node.attributes.push_back(std::make_pair("src", kDataURL));
  outer_params.push_back(outer_child_node);
  SafeBrowsingHostMsg_ThreatDOMDetails_Node outer_summary_node;
  outer_summary_node.url = GURL(kDOMParentURL);
  outer_summary_node.children.push_back(GURL(kDataURL));
  // Set |child_frame_routing_id| for this node to something non-sensical so
  // that the child frame lookup fails.
  outer_summary_node.child_frame_routing_id = -100;
  outer_params.push_back(outer_summary_node);

  // Now define some more nodes for the body of the frame. The URL of this
  // inner frame is "about:blank".
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> inner_params;
  SafeBrowsingHostMsg_ThreatDOMDetails_Node inner_child_node;
  inner_child_node.url = GURL(kDOMChildUrl2);
  inner_child_node.tag_name = "script";
  inner_child_node.parent = GURL(kBlankURL);
  inner_child_node.attributes.push_back(std::make_pair("src", kDOMChildUrl2));
  inner_params.push_back(inner_child_node);
  SafeBrowsingHostMsg_ThreatDOMDetails_Node inner_summary_node;
  inner_summary_node.url = GURL(kBlankURL);
  inner_summary_node.children.push_back(GURL(kDOMChildUrl2));
  inner_params.push_back(inner_summary_node);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_UNWANTED);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kDOMParentURL);
  pb_resource->add_child_ids(3);

  // TODO(lpz): The data URL is added, despite being unreportable, because it
  // is a child of the top-level page. Consider if this should happen.
  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kDataURL);

  // This child can't be mapped to its containing iframe so its parent is unset.
  pb_resource = expected.add_resources();
  pb_resource->set_id(4);
  pb_resource->set_url(kDOMChildUrl2);

  expected.set_complete(false);  // Since the cache was missing.

  // This Element represents the Frame with the data URL. It has no resource or
  // children since it couldn't be mapped to anything. It does still contain the
  // src attribute with the data URL set.
  HTMLElement* pb_element = expected.add_dom();
  pb_element->set_id(0);
  pb_element->set_tag("FRAME");
  pb_element->add_attribute()->set_name("src");
  pb_element->mutable_attribute(0)->set_value(kDataURL);

  pb_element = expected.add_dom();
  pb_element->set_id(1);
  pb_element->set_tag("SCRIPT");
  pb_element->set_resource_id(4);
  pb_element->add_attribute()->set_name("src");
  pb_element->mutable_attribute(0)->set_value(kDOMChildUrl2);

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_UNWANTED,
               ThreatSource::PASSWORD_PROTECTION_SERVICE,
               true /* is_subresource */, GURL(kThreatURL), &resource);
  scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL, history_service());
  base::HistogramTester histograms;

  // Send both sets of nodes from different render frames.
  report->OnReceivedThreatDOMDetails(main_rfh(), outer_params);
  report->OnReceivedThreatDOMDetails(child_rfh, inner_params);

  std::string serialized = WaitForSerializedReport(
      report.get(), false /* did_proceed*/, 0 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);
  VerifyResults(actual, expected);

  // This DOM should be ambiguous, expect the UMA metric to be incremented.
  histograms.ExpectTotalCount(kAmbiguousDomMetric, 1);
}

// Tests creating a threat report of a malware page where there are redirect
// urls to an unsafe resource url.
TEST_F(ThreatDetailsTest, ThreatWithRedirectUrl) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::REMOTE,
               true /* is_subresource */, GURL(kThreatURL), &resource);
  resource.original_url = GURL(kOriginalLandingURL);

  // add some redirect urls
  resource.redirect_urls.push_back(GURL(kFirstRedirectURL));
  resource.redirect_urls.push_back(GURL(kSecondRedirectURL));
  resource.redirect_urls.push_back(GURL(kThreatURL));

  scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL, history_service());

  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, 0 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::ANDROID_SAFETYNET);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);
  expected.set_repeat_visit(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kOriginalLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kThreatURL);
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

// Test collecting threat details for a blocked main frame load.
TEST_F(ThreatDetailsTest, ThreatOnMainPageLoadBlocked) {
  const char* kUnrelatedReferrerURL =
      "http://www.unrelatedreferrer.com/some/path";
  const char* kUnrelatedURL = "http://www.unrelated.com/some/path";

  // Load and commit an unrelated URL. The ThreatDetails should not use this
  // navigation entry.
  content::WebContentsTester::For(web_contents())
      ->TestDidNavigateWithReferrer(
          web_contents()->GetMainFrame(), 0 /* nav_entry_id */,
          true /* did_create_new_entry */, GURL(kUnrelatedURL),
          content::Referrer(GURL(kUnrelatedReferrerURL),
                            blink::kWebReferrerPolicyDefault),
          ui::PAGE_TRANSITION_TYPED);

  // Start a pending load with a referrer.
  controller().LoadURL(
      GURL(kLandingURL),
      content::Referrer(GURL(kReferrerURL), blink::kWebReferrerPolicyDefault),
      ui::PAGE_TRANSITION_TYPED, std::string());

  // Create UnsafeResource for the pending main page load.
  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::UNKNOWN,
               false /* is_subresource */, GURL(kLandingURL), &resource);

  // Start ThreatDetails collection.
  scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL, history_service());

  // Simulate clicking don't proceed.
  controller().DiscardNonCommittedEntries();

  // Finish ThreatDetails collection.
  std::string serialized = WaitForSerializedReport(
      report.get(), false /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kLandingURL);
  expected.set_page_url(kLandingURL);
  // Note that the referrer policy is not actually enacted here, since that's
  // done in Blink.
  expected.set_referrer_url(kReferrerURL);
  expected.set_did_proceed(false);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kReferrerURL);

  VerifyResults(actual, expected);
}

// Tests that a pending load does not interfere with collecting threat details
// for the committed page.
TEST_F(ThreatDetailsTest, ThreatWithPendingLoad) {
  const char* kPendingReferrerURL = "http://www.pendingreferrer.com/some/path";
  const char* kPendingURL = "http://www.pending.com/some/path";

  // Load and commit the landing URL with a referrer.
  content::WebContentsTester::For(web_contents())
      ->TestDidNavigateWithReferrer(
          web_contents()->GetMainFrame(), 0 /* nav_entry_id */,
          true /* did_create_new_entry */, GURL(kLandingURL),
          content::Referrer(GURL(kReferrerURL),
                            blink::kWebReferrerPolicyDefault),
          ui::PAGE_TRANSITION_TYPED);

  // Create UnsafeResource for fake sub-resource of landing page.
  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::LOCAL_PVER4,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  // Start a pending load before creating ThreatDetails.
  controller().LoadURL(GURL(kPendingURL),
                       content::Referrer(GURL(kPendingReferrerURL),
                                         blink::kWebReferrerPolicyDefault),
                       ui::PAGE_TRANSITION_TYPED, std::string());

  // Do ThreatDetails collection.
  scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL, history_service());
  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER4_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  // Note that the referrer policy is not actually enacted here, since that's
  // done in Blink.
  expected.set_referrer_url(kReferrerURL);
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kReferrerURL);

  VerifyResults(actual, expected);
}

TEST_F(ThreatDetailsTest, ThreatOnFreshTab) {
  // A fresh WebContents should not have any NavigationEntries yet. (See
  // https://crbug.com/524208.)
  EXPECT_EQ(nullptr, controller().GetLastCommittedEntry());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());

  // Simulate a subresource malware hit (this could happen if the WebContents
  // was created with window.open, and had content injected into it).
  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::CLIENT_SIDE_DETECTION,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  // Do ThreatDetails collection.
  scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL, history_service());
  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kThreatURL);
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kThreatURL);

  VerifyResults(actual, expected);
}

// Tests the interaction with the HTTP cache.
TEST_F(ThreatDetailsTest, HTTPCache) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
               ThreatSource::CLIENT_SIDE_DETECTION, true /* is_subresource */,
               GURL(kThreatURL), &resource);

  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource,
                            profile()->GetRequestContext(), history_service());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&FillCache,
                     base::RetainedRef(profile()->GetRequestContext())));

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
  report->OnReceivedThreatDOMDetails(main_rfh(), params);

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, -1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  ClientSafeBrowsingReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  ClientSafeBrowsingReportRequest::HTTPHeader* pb_header =
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
  std::string landing_data(kLandingData);
  pb_response->set_bodylength(landing_data.size());
  pb_response->set_bodydigest(base::MD5String(landing_data));
  pb_response->set_remote_ip("1.2.3.4:80");

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  pb_response = pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("image/jpeg");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Some-Other-Header");
  pb_header->set_value("foo");
  pb_response->set_body(kThreatData);
  std::string threat_data(kThreatData);
  pb_response->set_bodylength(threat_data.size());
  pb_response->set_bodydigest(base::MD5String(threat_data));
  pb_response->set_remote_ip("1.2.3.4:80");
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Test that only some fields of the HTTPS resource (eg: whitelisted headers)
// are reported.
TEST_F(ThreatDetailsTest, HttpsResourceSanitization) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
               ThreatSource::CLIENT_SIDE_DETECTION, true /* is_subresource */,
               GURL(kThreatURLHttps), &resource);

  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource,
                            profile()->GetRequestContext(), history_service());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&FillCacheHttps,
                     base::RetainedRef(profile()->GetRequestContext())));

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
  report->OnReceivedThreatDOMDetails(main_rfh(), params);

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, -1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING);
  expected.set_url(kThreatURLHttps);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  ClientSafeBrowsingReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  ClientSafeBrowsingReportRequest::HTTPHeader* pb_header =
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
  std::string landing_data(kLandingData);
  pb_response->set_bodylength(landing_data.size());
  pb_response->set_bodydigest(base::MD5String(landing_data));
  pb_response->set_remote_ip("1.2.3.4:80");

  // The threat URL is HTTP so the request and response are cleared (except for
  // whitelisted headers and certain safe fields). Namely the firstline and body
  // are missing.
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURLHttps);
  pb_response = pb_resource->mutable_response();
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("image/jpeg");
  std::string threat_data(kThreatData);
  pb_response->set_bodylength(threat_data.size());
  pb_response->set_bodydigest(base::MD5String(threat_data));
  pb_response->set_remote_ip("1.2.3.4:80");
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Tests the interaction with the HTTP cache (where the cache is empty).
TEST_F(ThreatDetailsTest, HTTPCacheNoEntries) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE,
               ThreatSource::LOCAL_PVER3, true /* is_subresource */,
               GURL(kThreatURL), &resource);

  scoped_refptr<ThreatDetailsWrap> report =
      new ThreatDetailsWrap(ui_manager_.get(), web_contents(), resource,
                            profile()->GetRequestContext(), history_service());

  // No call to FillCache

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
  report->OnReceivedThreatDOMDetails(main_rfh(), params);

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForSerializedReport(
      report.get(), false /* did_proceed*/, -1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_MALWARE);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER3_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Test getting redirects from history service.
TEST_F(ThreatDetailsTest, HistoryServiceUrls) {
  // Add content to history service.
  // There are two redirect urls before reacing malware url:
  // kFirstRedirectURL -> kSecondRedirectURL -> kThreatURL
  GURL baseurl(kThreatURL);
  history::RedirectList redirects;
  redirects.push_back(GURL(kFirstRedirectURL));
  redirects.push_back(GURL(kSecondRedirectURL));
  AddPageToHistory(baseurl, &redirects);
  // Wait for history service operation finished.
  profile()->BlockUntilHistoryProcessesPendingRequests();

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::LOCAL_PVER3,
               true /* is_subresource */, GURL(kThreatURL), &resource);
  scoped_refptr<ThreatDetailsWrap> report = new ThreatDetailsWrap(
      ui_manager_.get(), web_contents(), resource, NULL, history_service());

  // The redirects collection starts after the IPC from the DOM is fired.
  std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node> params;
  report->OnReceivedThreatDOMDetails(main_rfh(), params);

  // Let the redirects callbacks complete.
  base::RunLoop().RunUntilIdle();

  std::string serialized = WaitForSerializedReport(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER3_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_parent_id(2);
  pb_resource->set_url(kThreatURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_parent_id(3);
  pb_resource->set_url(kSecondRedirectURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kFirstRedirectURL);

  VerifyResults(actual, expected);
}

}  // namespace safe_browsing
