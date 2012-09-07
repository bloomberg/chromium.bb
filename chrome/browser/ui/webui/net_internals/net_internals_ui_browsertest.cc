// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_internals/net_internals_ui_browsertest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "googleurl/src/gurl.h"
#include "net/base/address_list.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_proc.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/http/http_pipelined_host_capability.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::WebUIMessageHandler;

namespace {

// Called on IO thread.  Adds an entry to the cache for the specified hostname.
// Either |net_error| must be net::OK, or |address| must be NULL.
void AddCacheEntryOnIOThread(net::URLRequestContextGetter* context_getter,
                             const std::string& hostname,
                             const std::string& ip_literal,
                             int net_error,
                             int expire_days_from_now) {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  net::HostCache* cache = context->host_resolver()->GetHostCache();
  ASSERT_TRUE(cache);

  net::HostCache::Key key(hostname, net::ADDRESS_FAMILY_UNSPECIFIED, 0);
  base::TimeDelta ttl = base::TimeDelta::FromDays(expire_days_from_now);

  net::AddressList address_list;
  if (net_error == net::OK) {
    // If |net_error| does not indicate an error, convert |ip_literal| to a
    // net::AddressList, so it can be used with the cache.
    int rv = net::SystemHostResolverProc(ip_literal,
                                         net::ADDRESS_FAMILY_UNSPECIFIED,
                                         0,
                                         &address_list,
                                         NULL);
    ASSERT_EQ(net::OK, rv);
  } else {
    ASSERT_TRUE(ip_literal.empty());
  }

  // Add entry to the cache.
  cache->Set(net::HostCache::Key(hostname, net::ADDRESS_FAMILY_UNSPECIFIED, 0),
             net_error,
             address_list,
             base::TimeTicks::Now(),
             ttl);
}

// Called on IO thread.  Adds an entry to the list of known HTTP pipelining
// hosts.
void AddDummyHttpPipelineFeedbackOnIOThread(
    net::URLRequestContextGetter* context_getter,
    const std::string& hostname,
    int port,
    net::HttpPipelinedHostCapability capability) {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  net::HttpNetworkSession* http_network_session =
      context->http_transaction_factory()->GetSession();
  net::HttpServerProperties* http_server_properties =
      http_network_session->http_server_properties();
  net::HostPortPair origin(hostname, port);
  http_server_properties->SetPipelineCapability(origin, capability);
}

// Called on IO thread.  Adds an entry to the list of known HTTP pipelining
// hosts.
void EnableHttpPipeliningOnIOThread(
    net::URLRequestContextGetter* context_getter, bool enable) {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  net::HttpNetworkSession* http_network_session =
      context->http_transaction_factory()->GetSession();
  http_network_session->set_http_pipelining_enabled(enable);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetInternalsTest::MessageHandler
////////////////////////////////////////////////////////////////////////////////

// Class to handle messages from the renderer needed by certain tests.
class NetInternalsTest::MessageHandler : public content::WebUIMessageHandler {
 public:
  explicit MessageHandler(NetInternalsTest* net_internals_test);

 private:
  virtual void RegisterMessages() OVERRIDE;

  // Runs NetInternalsTest.callback with the given value.
  void RunJavascriptCallback(base::Value* value);

  // Takes a string and provides the corresponding URL from the test server,
  // which must already have been started.
  void GetTestServerURL(const base::ListValue* list_value);

  // Called on UI thread.  Adds an entry to the cache for the specified
  // hostname by posting a task to the IO thread.  Takes the host name,
  // ip address, net error code, and expiration time in days from now
  // as parameters.  If the error code indicates failure, the ip address
  // must be an empty string.
  void AddCacheEntry(const base::ListValue* list_value);

  // Opens a page in a new tab that prerenders the given URL.
  void PrerenderPage(const base::ListValue* list_value);

  // Navigates to the prerender in the background tab. This assumes that
  // there is a "Click()" function in the background tab which will navigate
  // there, and that the background tab exists at slot 1.
  void NavigateToPrerender(const base::ListValue* list_value);

  // Creates an incognito browser.  Once creation is complete, passes a
  // message to the Javascript test harness.
  void CreateIncognitoBrowser(const base::ListValue* list_value);

  // Closes an incognito browser created with CreateIncognitoBrowser.
  void CloseIncognitoBrowser(const base::ListValue* list_value);

  // Takes in a boolean and enables/disabled HTTP pipelining accordingly.
  void EnableHttpPipelining(const base::ListValue* list_value);

  // Called on UI thread. Adds an entry to the list of known HTTP pipelining
  // hosts.
  void AddDummyHttpPipelineFeedback(const base::ListValue* list_value);

  Browser* browser() { return net_internals_test_->browser(); }

  NetInternalsTest* net_internals_test_;
  Browser* incognito_browser_;

  DISALLOW_COPY_AND_ASSIGN(MessageHandler);
};

NetInternalsTest::MessageHandler::MessageHandler(
    NetInternalsTest* net_internals_test)
    : net_internals_test_(net_internals_test),
      incognito_browser_(NULL) {
}

void NetInternalsTest::MessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getTestServerURL",
      base::Bind(&NetInternalsTest::MessageHandler::GetTestServerURL,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("addCacheEntry",
      base::Bind(&NetInternalsTest::MessageHandler::AddCacheEntry,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("prerenderPage",
      base::Bind(&NetInternalsTest::MessageHandler::PrerenderPage,
                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback("navigateToPrerender",
      base::Bind(&NetInternalsTest::MessageHandler::NavigateToPrerender,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("createIncognitoBrowser",
      base::Bind(&NetInternalsTest::MessageHandler::CreateIncognitoBrowser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("closeIncognitoBrowser",
      base::Bind(&NetInternalsTest::MessageHandler::CloseIncognitoBrowser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("enableHttpPipelining",
      base::Bind(&NetInternalsTest::MessageHandler::EnableHttpPipelining,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("addDummyHttpPipelineFeedback",
      base::Bind(
          &NetInternalsTest::MessageHandler::AddDummyHttpPipelineFeedback,
          base::Unretained(this)));
}

void NetInternalsTest::MessageHandler::RunJavascriptCallback(
    Value* value) {
  web_ui()->CallJavascriptFunction("NetInternalsTest.callback", *value);
}

void NetInternalsTest::MessageHandler::GetTestServerURL(
    const ListValue* list_value) {
  ASSERT_TRUE(net_internals_test_->StartTestServer());
  std::string path;
  ASSERT_TRUE(list_value->GetString(0, &path));
  GURL url = net_internals_test_->test_server()->GetURL(path);
  scoped_ptr<Value> url_value(Value::CreateStringValue(url.spec()));
  RunJavascriptCallback(url_value.get());
}

void NetInternalsTest::MessageHandler::AddCacheEntry(
    const ListValue* list_value) {
  std::string hostname;
  std::string ip_literal;
  double net_error;
  double expire_days_from_now;
  ASSERT_TRUE(list_value->GetString(0, &hostname));
  ASSERT_TRUE(list_value->GetString(1, &ip_literal));
  ASSERT_TRUE(list_value->GetDouble(2, &net_error));
  ASSERT_TRUE(list_value->GetDouble(3, &expire_days_from_now));
  ASSERT_TRUE(browser());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AddCacheEntryOnIOThread,
                 make_scoped_refptr(browser()->profile()->GetRequestContext()),
                 hostname,
                 ip_literal,
                 static_cast<int>(net_error),
                 static_cast<int>(expire_days_from_now)));
}

void NetInternalsTest::MessageHandler::PrerenderPage(
    const ListValue* list_value) {
  std::string prerender_url;
  ASSERT_TRUE(list_value->GetString(0, &prerender_url));
  GURL loader_url =
      net_internals_test_->CreatePrerenderLoaderUrl(GURL(prerender_url));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(loader_url),
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
}

void NetInternalsTest::MessageHandler::NavigateToPrerender(
    const ListValue* list_value) {
  content::RenderViewHost* host =
      chrome::GetWebContentsAt(browser(), 1)->GetRenderViewHost();
  host->ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16("Click()"));
}

void NetInternalsTest::MessageHandler::CreateIncognitoBrowser(
    const ListValue* list_value) {
  ASSERT_FALSE(incognito_browser_);
  incognito_browser_ = net_internals_test_->CreateIncognitoBrowser();

  // Tell the test harness that creation is complete.
  StringValue command_value("onIncognitoBrowserCreatedForTest");
  web_ui()->CallJavascriptFunction("g_browser.receive", command_value);
}

void NetInternalsTest::MessageHandler::CloseIncognitoBrowser(
    const ListValue* list_value) {
  ASSERT_TRUE(incognito_browser_);
  chrome::CloseAllTabs(incognito_browser_);
  // Closing all a Browser's tabs will ultimately result in its destruction,
  // thought it may not have been destroyed yet.
  incognito_browser_ = NULL;
}

void NetInternalsTest::MessageHandler::EnableHttpPipelining(
    const ListValue* list_value) {
  bool enable;
  ASSERT_TRUE(list_value->GetBoolean(0, &enable));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&EnableHttpPipeliningOnIOThread,
                 make_scoped_refptr(browser()->profile()->GetRequestContext()),
                 enable));
}

void NetInternalsTest::MessageHandler::AddDummyHttpPipelineFeedback(
    const ListValue* list_value) {
  std::string hostname;
  double port;
  std::string raw_capability;
  net::HttpPipelinedHostCapability capability;
  ASSERT_TRUE(list_value->GetString(0, &hostname));
  ASSERT_TRUE(list_value->GetDouble(1, &port));
  ASSERT_TRUE(list_value->GetString(2, &raw_capability));
  if (raw_capability == "capable") {
    capability = net::PIPELINE_CAPABLE;
  } else if (raw_capability == "incapable") {
    capability = net::PIPELINE_INCAPABLE;
  } else {
    FAIL() << "Unexpected capability string: " << raw_capability;
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AddDummyHttpPipelineFeedbackOnIOThread,
                 make_scoped_refptr(browser()->profile()->GetRequestContext()),
                 hostname,
                 static_cast<int>(port),
                 capability));
}

////////////////////////////////////////////////////////////////////////////////
// NetInternalsTest
////////////////////////////////////////////////////////////////////////////////

NetInternalsTest::NetInternalsTest()
    : test_server_started_(false) {
  message_handler_.reset(new MessageHandler(this));
}

NetInternalsTest::~NetInternalsTest() {
}

void NetInternalsTest::SetUpCommandLine(CommandLine* command_line) {
  WebUIBrowserTest::SetUpCommandLine(command_line);
  // Needed to test the prerender view.
  command_line->AppendSwitchASCII(switches::kPrerenderMode,
                                  switches::kPrerenderModeSwitchValueEnabled);
}

void NetInternalsTest::SetUpOnMainThread() {
  // Increase the memory allowed in a prerendered page above normal settings,
  // as debug builds use more memory and often go over the usual limit.
  Profile* profile = browser()->profile();
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile);
  prerender_manager->mutable_config().max_bytes = 1000 * 1024 * 1024;
}

content::WebUIMessageHandler* NetInternalsTest::GetMockMessageHandler() {
  return message_handler_.get();
}

GURL NetInternalsTest::CreatePrerenderLoaderUrl(
    const GURL& prerender_url) {
  EXPECT_TRUE(StartTestServer());
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      make_pair("REPLACE_WITH_PRERENDER_URL", prerender_url.spec()));
  replacement_text.push_back(
      make_pair("REPLACE_WITH_DESTINATION_URL", prerender_url.spec()));
  std::string replacement_path;
  EXPECT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_loader.html",
      replacement_text,
      &replacement_path));
  GURL url_loader = test_server()->GetURL(replacement_path);
  return url_loader;
}

bool NetInternalsTest::StartTestServer() {
  if (test_server_started_)
    return true;
  test_server_started_ = test_server()->Start();
  return test_server_started_;
}
