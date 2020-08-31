// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

const char* kPermittedHost = "a.example";
const char* kOtherPermittedHost = "b.example";
const char* kNotPermittedHost = "c.example";
const char* kPermittedSubdomain = "sub.a.example";
const char* kNotPermittedSubdomain = "notallowedsub.a.example";
const char* kPermissionPattern1 = "https://a.example/*";
const char* kPermissionPattern1Sub = "https://sub.a.example/*";
const char* kPermissionPattern2 = "https://b.example/*";

// Path for URL of custom ControllableHttpResponse
const char* kFetchCookiesPath = "/respondwithcookies";
// CSP header to be applied to the extension and the child frames
const char* kCspHeader =
    "script-src 'self' https://a.example:* https://sub.a.example:* "
    "https://notallowedsub.a.example:* https://b.example:* "
    "https://c.example:*; object-src 'self'";

const char* kNoneCookie = "none=1";
const char* kLaxCookie = "lax=1";
const char* kStrictCookie = "strict=1";
const char* kUnspecifiedCookie = "unspecified=1";
const char* kSameSiteNoneAttribute = "; SameSite=None; Secure";
const char* kSameSiteLaxAttribute = "; SameSite=Lax";
const char* kSameSiteStrictAttribute = "; SameSite=Strict";

// Tests for special handling of SameSite cookies for extensions:
// A request should be treated as first-party for the purposes of SameSite
// cookies if either
//  1) the request initiator is an extension with access to the requested URL,
//  2) the site_for_cookies is an extension with access to the requested URL,
//     and the request initiator (if it exists) is same-site to the requested
//     URL and also the extension has access to it.
// See ChromeExtensionsRendererClient::WillSendRequest().
//
// The test fixture param is whether or not the new SameSite features are
// enabled.
class ExtensionSameSiteCookiesTest
    : public ExtensionBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ExtensionSameSiteCookiesTest()
      : test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::Feature> samesite_features = {
        net::features::kSameSiteByDefaultCookies,
        net::features::kCookiesWithoutSameSiteMustBeSecure};
    if (AreSameSiteFeaturesEnabled()) {
      feature_list_.InitWithFeatures(samesite_features /* enabled */, {});
    } else {
      feature_list_.InitWithFeatures({}, samesite_features /* disabled */);
    }
  }
  ~ExtensionSameSiteCookiesTest() = default;
  ExtensionSameSiteCookiesTest(const ExtensionSameSiteCookiesTest&) = delete;
  ExtensionSameSiteCookiesTest& operator=(const ExtensionSameSiteCookiesTest&) =
      delete;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    extension_dir_ = std::make_unique<TestExtensionDir>();
    extension_ = MakeExtension();
    http_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            test_server(), kFetchCookiesPath);
    host_resolver()->AddRule("*", "127.0.0.1");
    net::test_server::RegisterDefaultHandlers(test_server());
    ASSERT_TRUE(test_server()->Start());
  }

  // Ignore cert errors for HTTPS test server, in order to use hostnames other
  // than localhost or 127.0.0.1.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

 protected:
  // Sets an array of cookies with various SameSite values.
  // These cookies are set directly into the cookie store, simulating being set
  // from a "strictly same-site" request context.
  void SetCookies(const std::string& host) {
    GURL url = test_server()->GetURL(host, "/");
    content::SetCookie(browser()->profile(), url,
                       base::StrCat({kNoneCookie, kSameSiteNoneAttribute}));
    content::SetCookie(browser()->profile(), url,
                       base::StrCat({kLaxCookie, kSameSiteLaxAttribute}));
    content::SetCookie(browser()->profile(), url,
                       base::StrCat({kStrictCookie, kSameSiteStrictAttribute}));
    content::SetCookie(browser()->profile(), url, kUnspecifiedCookie);
  }

  // Expect that all cookies, including SameSite cookies, are present.
  void ExpectSameSiteCookies(const std::string& cookie_header) {
    EXPECT_THAT(cookie_header, testing::HasSubstr(kNoneCookie));
    EXPECT_THAT(cookie_header, testing::HasSubstr(kLaxCookie));
    EXPECT_THAT(cookie_header, testing::HasSubstr(kStrictCookie));
    EXPECT_THAT(cookie_header, testing::HasSubstr(kUnspecifiedCookie));
  }

  // Expect that only cookies without SameSite are present.
  void ExpectNoSameSiteCookies(const std::string& cookie_header) {
    EXPECT_THAT(cookie_header, testing::HasSubstr(kNoneCookie));
    EXPECT_THAT(cookie_header, testing::Not(testing::HasSubstr(kLaxCookie)));
    EXPECT_THAT(cookie_header, testing::Not(testing::HasSubstr(kStrictCookie)));
    if (AreSameSiteFeaturesEnabled()) {
      EXPECT_THAT(cookie_header,
                  testing::Not(testing::HasSubstr(kUnspecifiedCookie)));
    } else {
      EXPECT_THAT(cookie_header, testing::HasSubstr(kUnspecifiedCookie));
    }
  }

  // Navigates to the extension page in the main frame. Returns a pointer to the
  // RenderFrameHost of the main frame.
  content::RenderFrameHost* NavigateMainFrameToExtensionPage() {
    EXPECT_TRUE(content::NavigateToURL(
        web_contents(), extension_->GetResourceURL("/empty.html")));
    return web_contents()->GetMainFrame();
  }

  // Appends a child iframe via JS and waits for it to load. Returns a pointer
  // to the RenderFrameHost of the child frame. (Requests a page that responds
  // with the proper CSP header to allow scripts from the relevant origins.)
  content::RenderFrameHost* MakeChildFrame(content::RenderFrameHost* frame,
                                           const std::string& host) {
    EXPECT_FALSE(content::ChildFrameAt(frame, 0));
    GURL url = test_server()->GetURL(
        host,
        base::StrCat({"/set-header?Content-Security-Policy: ", kCspHeader}));
    const char kAppendFrameScriptTemplate[] = R"(
        var f = document.createElement('iframe');
        f.onload = (e) => window.domAutomationController.send(true);
        f.src = $1;
        document.body.appendChild(f);)";
    std::string append_frame_script =
        content::JsReplace(kAppendFrameScriptTemplate, url.spec());
    bool loaded = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(frame, append_frame_script,
                                                     &loaded));
    EXPECT_TRUE(loaded);
    content::RenderFrameHost* child_frame = content::ChildFrameAt(frame, 0);
    EXPECT_EQ(url, child_frame->GetLastCommittedURL());
    return child_frame;
  }

  // Makes a request to |host| from the context of |frame|, then returns the
  // cookies that were sent on that request.
  std::string FetchCookies(content::RenderFrameHost* frame,
                           const std::string& host) {
    GURL cookie_url = test_server()->GetURL(host, kFetchCookiesPath);
    const char kFetchCookiesScriptTemplate[] = R"(
        fetch($1, {method: 'GET', credentials: 'include'})
          .then((resp) => resp.text())
          .then((data) => window.domAutomationController.send(data));)";
    std::string fetch_cookies_script =
        content::JsReplace(kFetchCookiesScriptTemplate, cookie_url.spec());
    content::DOMMessageQueue messages(web_contents());
    ExecuteScriptAsync(frame, fetch_cookies_script);

    url::Origin initiator = frame->GetLastCommittedOrigin();
    WaitForRequestAndRespondWithCookies(initiator);

    std::string result;
    if (!messages.PopMessage(&result)) {
      EXPECT_TRUE(messages.WaitForMessage(&result));
    }
    return result;
  }

  // Responds to a request with the cookies that were sent with the request.
  // We can't simply use the default handler /echoheader?Cookie here, because it
  // doesn't send the appropriate Access-Control-Allow-Origin and
  // Access-Control-Allow-Credentials headers (which are required for this to
  // work since we are making cross-origin requests in these tests).
  void WaitForRequestAndRespondWithCookies(const url::Origin& initiator) {
    http_response_->WaitForRequest();

    // Remove the trailing slash from the URL.
    std::string origin = initiator.GetURL().spec();
    base::TrimString(origin, "/", &origin);

    // Get the 'Cookie' header that was sent in the request.
    std::string cookie_header;
    auto cookie_header_it = http_response_->http_request()->headers.find(
        net::HttpRequestHeaders::kCookie);
    if (cookie_header_it == http_response_->http_request()->headers.end()) {
      cookie_header = "No cookie header!";
    } else {
      cookie_header = cookie_header_it->second;
    }
    std::string content_length = base::NumberToString(cookie_header.length());

    // clang-format off
    http_response_->Send(
        base::StrCat({
        "HTTP/1.1 200 OK\r\n",
        "Content-Type: text/plain; charset=utf-8\r\n",
        "Content-Length: ", content_length, "\r\n",
        "Access-Control-Allow-Origin: ", origin, "\r\n",
        "Access-Control-Allow-Credentials: true\r\n",
        "\r\n",
        cookie_header}));
    // clang-format on
  }

  const Extension* MakeExtension() {
    ChromeTestExtensionLoader loader(profile());
    DictionaryBuilder manifest;
    manifest.Set("name", "SameSite cookies test extension")
        .Set("version", "1")
        .Set("manifest_version", 2)
        .Set("web_accessible_resources", ListBuilder().Append("*.html").Build())
        .Set("content_security_policy", kCspHeader)
        .Set("permissions", ListBuilder()
                                .Append(kPermissionPattern1)
                                .Append(kPermissionPattern1Sub)
                                .Append(kPermissionPattern2)
                                .Build());
    extension_dir_->WriteFile(FILE_PATH_LITERAL("empty.html"), "");
    extension_dir_->WriteFile(FILE_PATH_LITERAL("script.js"), "");
    extension_dir_->WriteManifest(manifest.ToJSON());

    const Extension* extension =
        loader.LoadExtension(extension_dir_->UnpackedPath()).get();

    DCHECK(extension);
    return extension;
  }

  bool AreSameSiteFeaturesEnabled() { return GetParam(); }

  // The test server needs to be HTTPS because a SameSite=None cookie must be
  // Secure.
  net::EmbeddedTestServer* test_server() { return &test_server_; }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::unique_ptr<net::test_server::ControllableHttpResponse> http_response_;
  net::EmbeddedTestServer test_server_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestExtensionDir> extension_dir_;
  const Extension* extension_ = nullptr;
};

// Tests where the extension page initiates the request.

// Extension initiates request to permitted host => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       ExtensionInitiatedPermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* frame = NavigateMainFrameToExtensionPage();
  std::string cookies = FetchCookies(frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension initiates request to disallowed host => SameSite cookies are not
// sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       ExtensionInitiatedNotPermitted) {
  SetCookies(kNotPermittedHost);
  content::RenderFrameHost* frame = NavigateMainFrameToExtensionPage();
  std::string cookies = FetchCookies(frame, kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Tests with one frame on an extension page which makes the request.

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSameSiteFrame) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  std::string cookies = FetchCookies(child_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site (initiator is a subdomain of the
// requested domain) => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSubdomainFrame) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedSubdomain);
  std::string cookies = FetchCookies(child_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are same-site (initiator is a superdomain of the
// requested domain) => SameSite cookies are sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedSuperdomainFrame) {
  SetCookies(kPermittedSubdomain);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  std::string cookies = FetchCookies(child_frame, kPermittedSubdomain);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are permitted,
// initiator and requested URL are cross-site => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       OnePermittedCrossSiteFrame) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kOtherPermittedHost);
  std::string cookies = FetchCookies(child_frame, kPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is permitted but requested URL is
// not => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       CrossSiteInitiatorPermittedRequestNotPermitted) {
  SetCookies(kNotPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  std::string cookies = FetchCookies(child_frame, kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is permitted but requested URL is
// not, even though they are same-site => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       SameSiteInitiatorPermittedRequestNotPermitted) {
  SetCookies(kNotPermittedSubdomain);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  std::string cookies = FetchCookies(child_frame, kNotPermittedSubdomain);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is not permitted but requested URL
// is permitted, even though they are same-site => SameSite cookies are not
// sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       SameSiteInitiatorNotPermittedRequestPermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kNotPermittedSubdomain);
  std::string cookies = FetchCookies(child_frame, kPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator and requested URL are same-site but
// not permitted => SameSite cookies are not sent.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       SameSiteInitiatorAndRequestNotPermitted) {
  SetCookies(kNotPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kNotPermittedHost);
  std::string cookies = FetchCookies(child_frame, kNotPermittedHost);
  ExpectNoSameSiteCookies(cookies);
}

// Tests where the initiator is a nested frame. Here it doesn't actually matter
// what the initiator is nested in, because we don't check.

// Extension is site_for_cookies, initiator is allowed frame nested inside a
// same-site allowed frame, request is to the same site => SameSite cookies are
// attached.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest, NestedSameSitePermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kPermittedHost);
  content::RenderFrameHost* nested_frame =
      MakeChildFrame(child_frame, kPermittedHost);
  std::string cookies = FetchCookies(nested_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is allowed frame nested inside a
// cross-site allowed frame, request is to the same site => SameSite cookies are
// attached.
// This is kind of an interesting case. Should we attach SameSite cookies here?
// If we only check first-partyness between each frame ancestor and the main
// frame, then we consider all of these frames first-party to the extension, so
// we should attach SameSite cookies here. (This is the current algorithm in the
// spec, which says to check each ancestor against the top frame:
// https://tools.ietf.org/html/draft-ietf-httpbis-rfc6265bis-03#section-5.2.1)
// If we also want to ensure first-partyness between each frame and its
// immediate parent, then we should not send SameSite cookies here. See
// crbug.com/1027258.
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest, NestedCrossSitePermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kOtherPermittedHost);
  content::RenderFrameHost* nested_frame =
      MakeChildFrame(child_frame, kPermittedHost);
  std::string cookies = FetchCookies(nested_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// The following tests are correct for current behavior, but should probably
// change in the future. We should be walking up the whole frame tree instead of
// only checking permissions and same-siteness for the initiator and request.

// Extension is site_for_cookies, initiator is allowed frame nested inside a
// cross-site disallowed frame, request is to the same site => SameSite cookies
// are attached (but ideally shouldn't be).
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       NestedCrossSiteNotPermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kNotPermittedHost);
  content::RenderFrameHost* nested_frame =
      MakeChildFrame(child_frame, kPermittedHost);
  std::string cookies = FetchCookies(nested_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

// Extension is site_for_cookies, initiator is allowed frame nested inside a
// same-site disallowed frame, request is to the same site => SameSite cookies
// are attached (but ideally shouldn't be).
IN_PROC_BROWSER_TEST_P(ExtensionSameSiteCookiesTest,
                       NestedSameSiteNotPermitted) {
  SetCookies(kPermittedHost);
  content::RenderFrameHost* main_frame = NavigateMainFrameToExtensionPage();
  content::RenderFrameHost* child_frame =
      MakeChildFrame(main_frame, kNotPermittedSubdomain);
  content::RenderFrameHost* nested_frame =
      MakeChildFrame(child_frame, kPermittedHost);
  std::string cookies = FetchCookies(nested_frame, kPermittedHost);
  ExpectSameSiteCookies(cookies);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionSameSiteCookiesTest,
                         ::testing::Bool());

}  // namespace

}  // namespace extensions
