// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/manifest.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class ManifestBrowserTest : public ContentBrowserTest  {
 protected:
  ManifestBrowserTest() {}
  virtual ~ManifestBrowserTest() {}

  void GetManifestAndWait() {
    shell()->web_contents()->GetManifest(
        base::Bind(&ManifestBrowserTest::OnGetManifest,
                   base::Unretained(this)));

    message_loop_runner_ = new MessageLoopRunner();
    message_loop_runner_->Run();
  }

  void OnGetManifest(const Manifest& manifest) {
    manifest_ = manifest;
    message_loop_runner_->Quit();
  }

  const Manifest& manifest() const {
    return manifest_;
  }

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  Manifest manifest_;

  DISALLOW_COPY_AND_ASSIGN(ManifestBrowserTest);
};

// If a page has no manifest, requesting a manifest should return the empty
// manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, NoManifest) {
  GURL test_url = GetTestUrl("manifest", "no-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
}

// If a page manifest points to a 404 URL, requesting the manifest should return
// the empty manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, 404Manifest) {
  GURL test_url = GetTestUrl("manifest", "404-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
}

// If a page has an empty manifest, requesting the manifest should return the
// empty manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, EmptyManifest) {
  GURL test_url = GetTestUrl("manifest", "empty-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
}

// If a page's manifest can't be parsed correctly, requesting the manifest
// should return an empty manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, ParseErrorManifest) {
  GURL test_url = GetTestUrl("manifest", "parse-error-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
}

// If a page has a manifest that can be fetched and parsed, requesting the
// manifest should return a properly filled manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, DummyManifest) {
  GURL test_url = GetTestUrl("manifest", "dummy-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
}

// If a page changes manifest during its life-time, requesting the manifest
// should return the current manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, DynamicManifest) {
  GURL test_url = GetTestUrl("manifest", "dynamic-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  {
    GetManifestAndWait();
    EXPECT_TRUE(manifest().IsEmpty());
  }

  {
    std::string manifest_url =
        GetTestUrl("manifest", "dummy-manifest.json").spec();
    ASSERT_TRUE(content::ExecuteScript(
        shell()->web_contents(), "setManifestTo('" + manifest_url + "')"));

    GetManifestAndWait();
    EXPECT_FALSE(manifest().IsEmpty());
  }

  {
    std::string manifest_url =
        GetTestUrl("manifest", "empty-manifest.json").spec();
    ASSERT_TRUE(content::ExecuteScript(
        shell()->web_contents(), "setManifestTo('" + manifest_url + "')"));

    GetManifestAndWait();
    EXPECT_TRUE(manifest().IsEmpty());
  }
}

// If a page's manifest lives in a different origin, it should follow the CORS
// rules and requesting the manifest should return an empty manifest (unless the
// response contains CORS headers).
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, CORSManifest) {
  scoped_ptr<net::test_server::EmbeddedTestServer> cors_embedded_test_server(
      new net::test_server::EmbeddedTestServer);

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(cors_embedded_test_server->InitializeAndWaitUntilReady());
  ASSERT_NE(embedded_test_server()->port(), cors_embedded_test_server->port());

  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dynamic-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  std::string manifest_url =
      cors_embedded_test_server->GetURL("/manifest/dummy-manifest.json").spec();
  ASSERT_TRUE(content::ExecuteScript(shell()->web_contents(),
                                     "setManifestTo('" + manifest_url + "')"));

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
}

// If a page's manifest is in an unsecure origin while the page is in a secure
// origin, requesting the manifest should return the empty manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, MixedContentManifest) {
  scoped_ptr<net::SpawnedTestServer> https_server(new net::SpawnedTestServer(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::BaseTestServer::SSLOptions(net::BaseTestServer::SSLOptions::CERT_OK),
      base::FilePath(FILE_PATH_LITERAL("content/test/data"))));

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(https_server->Start());

  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dynamic-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  std::string manifest_url =
      https_server->GetURL("/manifest/dummy-manifest.json").spec();
  ASSERT_TRUE(content::ExecuteScript(shell()->web_contents(),
                                     "setManifestTo('" + manifest_url + "')"));

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
}

} // namespace content
