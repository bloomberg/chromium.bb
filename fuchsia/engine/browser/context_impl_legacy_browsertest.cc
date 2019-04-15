// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/common.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

using testing::_;
using testing::Field;
using testing::InvokeWithoutArgs;

// Use a shorter name for NavigationEvent, because it is
// referenced frequently in this file.
using NavigationDetails = chromium::web::NavigationEvent;

namespace {

void OnCookiesReceived(net::CookieList* output,
                       base::OnceClosure on_received_cb,
                       const net::CookieList& cookies,
                       const net::CookieStatusList& excluded_cookies) {
  *output = cookies;
  std::move(on_received_cb).Run();
}

}  // namespace

// Defines a suite of tests that exercise browser-level configuration and
// functionality.
class ContextImplLegacyTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  ContextImplLegacyTest() = default;
  ~ContextImplLegacyTest() override = default;

 protected:
  // Creates a Frame with |navigation_listener_| attached.
  chromium::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateLegacyFrame(&navigation_listener_);
  }

  // Synchronously gets a list of cookies for this BrowserContext.
  net::CookieList GetCookies() {
    net::CookieStore* cookie_store =
        content::BrowserContext::GetDefaultStoragePartition(
            context_impl()->browser_context_for_test())
            ->GetURLRequestContext()
            ->GetURLRequestContext()
            ->cookie_store();

    base::RunLoop run_loop;
    net::CookieList cookies;
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &net::CookieStore::GetAllCookiesAsync,
            base::Unretained(cookie_store),
            base::BindOnce(&OnCookiesReceived, base::Unretained(&cookies),
                           run_loop.QuitClosure())));
    run_loop.Run();
    return cookies;
  }

  cr_fuchsia::TestNavigationListener navigation_listener_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextImplLegacyTest);
};

// Verifies that the BrowserContext has a working cookie store by setting
// cookies in the content layer and then querying the CookieStore afterward.
IN_PROC_BROWSER_TEST_F(ContextImplLegacyTest, VerifyPersistentCookieStore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL cookie_url(embedded_test_server()->GetURL("/set-cookie?foo=bar"));
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr nav;
  frame->GetNavigationController(nav.NewRequest());

  nav->LoadUrl(cookie_url.spec(), chromium::web::LoadUrlParams());
  navigation_listener_.RunUntilNavigationEquals(cookie_url, {});

  auto cookies = GetCookies();
  bool found = false;
  for (auto c : cookies) {
    if (c.Name() == "foo" && c.Value() == "bar") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);

  // Check that the cookie persists beyond the lifetime of the Frame by
  // releasing the Frame and re-querying the CookieStore.
  frame.Unbind();
  base::RunLoop().RunUntilIdle();

  found = false;
  for (auto c : cookies) {
    if (c.Name() == "foo" && c.Value() == "bar") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

// Suite for tests which run the BrowserContext in incognito mode (no data
// directory).
class IncognitoContextImplLegacyTest : public ContextImplLegacyTest {
 public:
  IncognitoContextImplLegacyTest() = default;
  ~IncognitoContextImplLegacyTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(kIncognitoSwitch);
    ContextImplLegacyTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IncognitoContextImplLegacyTest);
};

// Verify that the browser can be initialized without a persistent data
// directory.
IN_PROC_BROWSER_TEST_F(IncognitoContextImplLegacyTest, NavigateFrame) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  controller->LoadUrl(url::kAboutBlankURL, chromium::web::LoadUrlParams());
  navigation_listener_.RunUntilNavigationEquals(GURL(url::kAboutBlankURL), {});

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(IncognitoContextImplLegacyTest,
                       VerifyInMemoryCookieStore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL cookie_url(embedded_test_server()->GetURL("/set-cookie?foo=bar"));
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr nav;
  frame->GetNavigationController(nav.NewRequest());

  nav->LoadUrl(cookie_url.spec(), chromium::web::LoadUrlParams());
  navigation_listener_.RunUntilNavigationEquals(cookie_url, {});

  auto cookies = GetCookies();
  bool found = false;
  for (auto c : cookies) {
    if (c.Name() == "foo" && c.Value() == "bar") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}
