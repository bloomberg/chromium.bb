// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/dom_snapshot.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/devtools/domains/security.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/tab_socket_test.h"
#include "printing/features/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
#include "base/strings/string_number_conversions.h"
#include "pdf/pdf.h"
#include "printing/pdf_render_settings.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect.h"
#endif

using testing::ElementsAre;
using testing::UnorderedElementsAre;

namespace headless {

#define EXPECT_CHILD_CONTENTS_CREATED(obs)                                    \
  EXPECT_CALL((obs), OnChildContentsCreated(::testing::_, ::testing::_))      \
      .WillOnce(::testing::DoAll(::testing::SaveArg<0>(&((obs).last_parent)), \
                                 ::testing::SaveArg<1>(&((obs).last_child))))

class MockHeadlessBrowserContextObserver
    : public HeadlessBrowserContext::Observer {
 public:
  MOCK_METHOD2(OnChildContentsCreated,
               void(HeadlessWebContents*, HeadlessWebContents*));

  MockHeadlessBrowserContextObserver() {}
  virtual ~MockHeadlessBrowserContextObserver() {}

  HeadlessWebContents* last_parent;
  HeadlessWebContents* last_child;
};

class HeadlessWebContentsTest : public HeadlessBrowserTest {};

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, Navigation) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  EXPECT_THAT(browser_context->GetAllWebContents(),
              UnorderedElementsAre(web_contents));
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, WindowOpen) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  MockHeadlessBrowserContextObserver observer;
  browser_context->AddObserver(&observer);
  EXPECT_CHILD_CONTENTS_CREATED(observer);

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/window_open.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  EXPECT_EQ(2u, browser_context->GetAllWebContents().size());

  auto* parent = HeadlessWebContentsImpl::From(observer.last_parent);
  auto* child = HeadlessWebContentsImpl::From(observer.last_child);
  EXPECT_NE(nullptr, parent);
  EXPECT_NE(nullptr, child);
  EXPECT_NE(parent, child);

  // Mac doesn't have WindowTreeHosts.
  if (parent && child && parent->window_tree_host())
    EXPECT_NE(parent->window_tree_host(), child->window_tree_host());

  gfx::Rect expected_bounds(0, 0, 200, 100);
#if !defined(OS_MACOSX)
  EXPECT_EQ(expected_bounds, child->web_contents()->GetViewBounds());
  EXPECT_EQ(expected_bounds, child->web_contents()->GetContainerBounds());
#else   // !defined(OS_MACOSX)
  // Mac does not support GetViewBounds() and view positions are random.
  EXPECT_EQ(expected_bounds.size(),
            child->web_contents()->GetContainerBounds().size());
#endif  // !defined(OS_MACOSX)
}

class HeadlessWindowOpenTabSocketTest : public HeadlessBrowserTest,
                                        public HeadlessTabSocket::Listener,
                                        public HeadlessBrowserContext::Observer,
                                        public HeadlessWebContents::Observer,
                                        public runtime::Observer {
 public:
  HeadlessWindowOpenTabSocketTest()
      : devtools_client_(HeadlessDevToolsClient::Create()) {}

  void SetUp() override {
    options()->mojo_service_names.insert("headless::TabSocket");
    HeadlessBrowserTest::SetUp();
  }

  // HeadlessTabSocket::Listener implementation.
  void OnMessageFromContext(const std::string& message,
                            int execution_context_id) override {
    message_ = message;
    FinishAsynchronousTest();
  }

  // HeadlessBrowserContext::Observer implementation.
  void OnChildContentsCreated(HeadlessWebContents* parent,
                              HeadlessWebContents* child) override {
    EXPECT_EQ(nullptr, child_);
    child_ = child;
    child_->AddObserver(this);
  }

  // HeadlessWebContents::Observer implementation.
  void DevToolsTargetReady() override {
    child_->RemoveObserver(this);

    // Verify tab socket of child_contents works.
    child_->GetDevToolsTarget()->AttachClient(devtools_client_.get());

    devtools_client_->GetPage()->Enable();
    devtools_client_->GetPage()->GetExperimental()->GetResourceTree(
        page::GetResourceTreeParams::Builder().Build(),
        base::Bind(&HeadlessWindowOpenTabSocketTest::OnResourceTree,
                   base::Unretained(this)));
  }

  void OnResourceTree(std::unique_ptr<page::GetResourceTreeResult> result) {
    child_frame_id_ = result->GetFrameTree()->GetFrame()->GetId();
    devtools_client_->GetRuntime()->AddObserver(this);
    // This will trigger OnExecutionContextCreated getting called for all
    // existing contexts.
    devtools_client_->GetRuntime()->Enable();
  }

  // runtime::Observer implementation.
  void OnExecutionContextCreated(
      const runtime::ExecutionContextCreatedParams& params) override {
    const base::DictionaryValue* dictionary;
    std::string frame_id;
    if (params.GetContext()->HasAuxData() &&
        params.GetContext()->GetAuxData()->GetAsDictionary(&dictionary) &&
        dictionary->GetString("frameId", &frame_id) &&
        frame_id == *child_frame_id_) {
      child_frame_execution_context_id_ = params.GetContext()->GetId();

      HeadlessTabSocket* tab_socket = child_->GetHeadlessTabSocket();
      CHECK(tab_socket);
      tab_socket->InstallHeadlessTabSocketBindings(
          *child_frame_execution_context_id_,
          base::Bind(&HeadlessWindowOpenTabSocketTest::OnTabSocketInstalled,
                     base::Unretained(this)));
    }
  }

  void OnTabSocketInstalled(bool success) {
    ASSERT_TRUE(success);
    HeadlessTabSocket* tab_socket = child_->GetHeadlessTabSocket();
    CHECK(tab_socket);
    tab_socket->SendMessageToContext("One", *child_frame_execution_context_id_);
    tab_socket->SetListener(this);

    devtools_client_->GetRuntime()->Evaluate(
        R"(window.TabSocket.onmessage =
            function(message) {
              window.TabSocket.send('Embedder sent us: ' + message);
            };
          )",
        base::Bind(&HeadlessWindowOpenTabSocketTest::OnEvaluateResult,
                   base::Unretained(this)));
  }

  void OnEvaluateResult(std::unique_ptr<runtime::EvaluateResult> result) {
    child_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  }

 protected:
  std::string message_;
  base::Optional<std::string> child_frame_id_;
  base::Optional<int> child_frame_execution_context_id_;
  HeadlessWebContents* child_ = nullptr;
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
};

IN_PROC_BROWSER_TEST_F(HeadlessWindowOpenTabSocketTest,
                       WindowOpenWithTabSocket) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();
  browser_context->AddObserver(this);

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetAllowTabSockets(true)
          .SetInitialURL(embedded_test_server()->GetURL("/window_open.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  EXPECT_EQ(2u, browser_context->GetAllWebContents().size());
  EXPECT_NE(nullptr, child_);

  RunAsynchronousTest();
  EXPECT_EQ("Embedder sent us: One", message_);
}

class HeadlessNoDevToolsTabSocketTest : public HeadlessBrowserTest,
                                        public HeadlessTabSocket::Listener {
 public:
  HeadlessNoDevToolsTabSocketTest() {}

  void SetUp() override {
    options()->mojo_service_names.insert("headless::TabSocket");
    HeadlessBrowserTest::SetUp();
  }

  // HeadlessTabSocket::Listener implementation.
  void OnMessageFromContext(const std::string& message,
                            int execution_context_id) override {
    EXPECT_EQ(*execution_context_id_, execution_context_id);
    messages_.push_back(message);

    if (messages_.size() == 2) {
      EXPECT_THAT(messages_,
                  ElementsAre("Hello world!", "Embedder sent us: One"));
      FinishAsynchronousTest();
    }
  }

  void OnInstalledHeadlessTabSocket(base::Optional<int> execution_context_id) {
    EXPECT_TRUE(!!execution_context_id);
    if (!execution_context_id) {
      FinishAsynchronousTest();
    } else {
      execution_context_id_ = execution_context_id;
      tab_socket_->SendMessageToContext("One", *execution_context_id);
    }
  }

  std::vector<std::string> messages_;
  HeadlessTabSocket* tab_socket_;
  base::Optional<int> execution_context_id_;
};

IN_PROC_BROWSER_TEST_F(HeadlessNoDevToolsTabSocketTest, Test) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetAllowTabSockets(true)
          .SetInitialURL(embedded_test_server()->GetURL("/tabsocket.html"))
          .Build();

  tab_socket_ = web_contents->GetHeadlessTabSocket();
  CHECK(tab_socket_);
  tab_socket_->InstallMainFrameMainWorldHeadlessTabSocketBindings(
      base::Bind(&HeadlessNoDevToolsTabSocketTest::OnInstalledHeadlessTabSocket,
                 base::Unretained(this)));
  tab_socket_->SetListener(this);

  RunAsynchronousTest();
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, Focus) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  bool result;
  EXPECT_TRUE(EvaluateScript(web_contents, "document.hasFocus()")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsBoolean(&result));
  EXPECT_TRUE(result);

  HeadlessWebContents* web_contents2 =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents2));

  // Focus of different WebContents is independent.
  EXPECT_TRUE(EvaluateScript(web_contents, "document.hasFocus()")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsBoolean(&result));
  EXPECT_TRUE(result);
  EXPECT_TRUE(EvaluateScript(web_contents2, "document.hasFocus()")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsBoolean(&result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, HandleSSLError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(https_server.GetURL("/hello.html"))
          .Build();

  EXPECT_FALSE(WaitForLoad(web_contents));
}

namespace {
bool DecodePNG(std::string base64_data, SkBitmap* bitmap) {
  std::string png_data;
  if (!base::Base64Decode(base64_data, &png_data))
    return false;
  return gfx::PNGCodec::Decode(
      reinterpret_cast<unsigned const char*>(png_data.data()), png_data.size(),
      bitmap);
}
}  // namespace

// Parameter specifies whether --disable-gpu should be used.
class HeadlessWebContentsScreenshotTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    EnablePixelOutput();
    if (GetParam()) {
      UseSoftwareCompositing();
      SetUpWithoutGPU();
    } else {
      HeadlessAsyncDevTooledBrowserTest::SetUp();
    }
  }

  void RunDevTooledTest() override {
    std::unique_ptr<runtime::EvaluateParams> params =
        runtime::EvaluateParams::Builder()
            .SetExpression("document.body.style.background = '#0000ff'")
            .Build();
    devtools_client_->GetRuntime()->Evaluate(
        std::move(params),
        base::Bind(&HeadlessWebContentsScreenshotTest::OnPageSetupCompleted,
                   base::Unretained(this)));
  }

  void OnPageSetupCompleted(std::unique_ptr<runtime::EvaluateResult> result) {
    devtools_client_->GetPage()->GetExperimental()->CaptureScreenshot(
        page::CaptureScreenshotParams::Builder().Build(),
        base::Bind(&HeadlessWebContentsScreenshotTest::OnScreenshotCaptured,
                   base::Unretained(this)));
  }

  void OnScreenshotCaptured(
      std::unique_ptr<page::CaptureScreenshotResult> result) {
    std::string base64 = result->GetData();
    EXPECT_GT(base64.length(), 0U);
    SkBitmap result_bitmap;
    EXPECT_TRUE(DecodePNG(base64, &result_bitmap));

    EXPECT_EQ(800, result_bitmap.width());
    EXPECT_EQ(600, result_bitmap.height());
    SkColor actual_color = result_bitmap.getColor(400, 300);
    SkColor expected_color = SkColorSetRGB(0x00, 0x00, 0xff);
    EXPECT_EQ(expected_color, actual_color);
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_P(HeadlessWebContentsScreenshotTest);

// Instantiate test case for both software and gpu compositing modes.
INSTANTIATE_TEST_CASE_P(HeadlessWebContentsScreenshotTests,
                        HeadlessWebContentsScreenshotTest,
                        ::testing::Bool());

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
class HeadlessWebContentsPDFTest : public HeadlessAsyncDevTooledBrowserTest {
 public:
  const double kPaperWidth = 10;
  const double kPaperHeight = 15;
  const double kDocHeight = 50;
  // Number of color channels in a BGRA bitmap.
  const int kColorChannels = 4;
  const int kDpi = 300;

  void RunDevTooledTest() override {
    std::string height_expression = "document.body.style.height = '" +
                                    base::DoubleToString(kDocHeight) + "in'";
    std::unique_ptr<runtime::EvaluateParams> params =
        runtime::EvaluateParams::Builder()
            .SetExpression("document.body.style.background = '#123456';" +
                           height_expression)
            .Build();
    devtools_client_->GetRuntime()->Evaluate(
        std::move(params),
        base::Bind(&HeadlessWebContentsPDFTest::OnPageSetupCompleted,
                   base::Unretained(this)));
  }

  void OnPageSetupCompleted(std::unique_ptr<runtime::EvaluateResult> result) {
    devtools_client_->GetPage()->GetExperimental()->PrintToPDF(
        page::PrintToPDFParams::Builder()
            .SetPrintBackground(true)
            .SetPaperHeight(kPaperHeight)
            .SetPaperWidth(kPaperWidth)
            .SetMarginTop(0)
            .SetMarginBottom(0)
            .SetMarginLeft(0)
            .SetMarginRight(0)
            .Build(),
        base::Bind(&HeadlessWebContentsPDFTest::OnPDFCreated,
                   base::Unretained(this)));
  }

  void OnPDFCreated(std::unique_ptr<page::PrintToPDFResult> result) {
    std::string base64 = result->GetData();
    EXPECT_GT(base64.length(), 0U);
    std::string pdf_data;
    EXPECT_TRUE(base::Base64Decode(base64, &pdf_data));

    int num_pages;
    EXPECT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_data.data(), pdf_data.size(),
                                          &num_pages, nullptr));
    EXPECT_EQ(std::ceil(kDocHeight / kPaperHeight), num_pages);

    for (int i = 0; i < num_pages; i++) {
      double width_in_points;
      double height_in_points;
      EXPECT_TRUE(chrome_pdf::GetPDFPageSizeByIndex(
          pdf_data.data(), pdf_data.size(), i, &width_in_points,
          &height_in_points));
      EXPECT_EQ(static_cast<int>(width_in_points),
                static_cast<int>(kPaperWidth * printing::kPointsPerInch));
      EXPECT_EQ(static_cast<int>(height_in_points),
                static_cast<int>(kPaperHeight * printing::kPointsPerInch));

      gfx::Rect rect(kPaperWidth * kDpi, kPaperHeight * kDpi);
      printing::PdfRenderSettings settings(
          rect, gfx::Point(0, 0), kDpi, true,
          printing::PdfRenderSettings::Mode::NORMAL);
      std::vector<uint8_t> page_bitmap_data(kColorChannels *
                                            settings.area.size().GetArea());
      EXPECT_TRUE(chrome_pdf::RenderPDFPageToBitmap(
          pdf_data.data(), pdf_data.size(), i, page_bitmap_data.data(),
          settings.area.size().width(), settings.area.size().height(),
          settings.dpi, settings.autorotate));
      EXPECT_EQ(0x56, page_bitmap_data[0]);  // B
      EXPECT_EQ(0x34, page_bitmap_data[1]);  // G
      EXPECT_EQ(0x12, page_bitmap_data[2]);  // R
    }
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessWebContentsPDFTest);
#endif

class HeadlessWebContentsSecurityTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public security::ExperimentalObserver {
 public:
  void RunDevTooledTest() override {
    devtools_client_->GetSecurity()->GetExperimental()->AddObserver(this);
    devtools_client_->GetSecurity()->GetExperimental()->Enable(
        security::EnableParams::Builder().Build());
  }

  void OnSecurityStateChanged(
      const security::SecurityStateChangedParams& params) override {
    EXPECT_EQ(security::SecurityState::NEUTRAL, params.GetSecurityState());

    devtools_client_->GetSecurity()->GetExperimental()->Disable(
        security::DisableParams::Builder().Build());
    devtools_client_->GetSecurity()->GetExperimental()->RemoveObserver(this);
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessWebContentsSecurityTest);

class GetHeadlessTabSocketButNoTabSocket
    : public HeadlessAsyncDevTooledBrowserTest {
 public:
  void SetUp() override {
    options()->mojo_service_names.insert("headless::TabSocket");
    HeadlessAsyncDevTooledBrowserTest::SetUp();
  }

  void RunDevTooledTest() override {
    ASSERT_THAT(web_contents_->GetHeadlessTabSocket(), testing::IsNull());
    FinishAsynchronousTest();
  }

  bool GetAllowTabSockets() override { return false; }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(GetHeadlessTabSocketButNoTabSocket);

class MainWorldHeadlessTabSocketTest : public TabSocketTest {
 public:
  void RunTabSocketTest() override {
    CreateMainWorldTabSocket(
        main_frame_id(),
        base::Bind(
            &MainWorldHeadlessTabSocketTest::OnInstalledHeadlessTabSocket,
            base::Unretained(this)));
  }

  void OnInstalledHeadlessTabSocket(int execution_context_id) {
    devtools_client_->GetRuntime()->Evaluate(
        R"(window.TabSocket.onmessage =
            function(message) {
              window.TabSocket.send('Embedder sent us: ' + message);
            };
          )",
        base::Bind(&MainWorldHeadlessTabSocketTest::FailOnJsEvaluateException,
                   base::Unretained(this)));

    HeadlessTabSocket* headless_tab_socket =
        web_contents_->GetHeadlessTabSocket();
    DCHECK(headless_tab_socket);

    headless_tab_socket->SendMessageToContext("One", execution_context_id);
    headless_tab_socket->SendMessageToContext("Two", execution_context_id);
    headless_tab_socket->SendMessageToContext("Three", execution_context_id);
    headless_tab_socket->SetListener(this);
    main_frame_execution_context_id_ = execution_context_id;
  }

  void OnMessageFromContext(const std::string& message,
                            int execution_context_id) override {
    EXPECT_EQ(execution_context_id, *main_frame_execution_context_id_);
    messages_.push_back(message);
    if (messages_.size() == 3u) {
      EXPECT_THAT(messages_,
                  ElementsAre("Embedder sent us: One", "Embedder sent us: Two",
                              "Embedder sent us: Three"));
      FinishAsynchronousTest();
    }
  }

 private:
  std::vector<std::string> messages_;
  base::Optional<int> main_frame_execution_context_id_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(MainWorldHeadlessTabSocketTest);

class MainWorldHeadlessTabSocketBindingsNotInstalledTest
    : public TabSocketTest {
 public:
  void RunTabSocketTest() override {
    CreateIsolatedWorldTabSocket(
        "Test World", main_frame_id(),
        base::Bind(&MainWorldHeadlessTabSocketBindingsNotInstalledTest::
                       OnIsolatedWorldCreated,
                   base::Unretained(this)));
  }

  void OnIsolatedWorldCreated(int execution_context_id) {
    // We expect this to fail because TabSocket bindings where injected into the
    // isolated world not the main world.
    devtools_client_->GetRuntime()->Evaluate(
        "window.TabSocket.send('This should not work!');",
        base::Bind(&MainWorldHeadlessTabSocketBindingsNotInstalledTest::
                       ExpectJsException,
                   base::Unretained(this)));

    HeadlessTabSocket* headless_tab_socket =
        web_contents_->GetHeadlessTabSocket();
    DCHECK(headless_tab_socket);

    headless_tab_socket->SetListener(this);
  }

  void OnMessageFromContext(const std::string&, int) override {
    FinishAsynchronousTest();
    FAIL() << "Should not receive a message from the tab!";
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(
    MainWorldHeadlessTabSocketBindingsNotInstalledTest);

class IsolatedWorldHeadlessTabSocketTest : public TabSocketTest {
 public:
  void RunTabSocketTest() override {
    CreateIsolatedWorldTabSocket(
        "Test World", main_frame_id(),
        base::Bind(&IsolatedWorldHeadlessTabSocketTest::OnIsolatedWorldCreated,
                   base::Unretained(this)));
  }

  void OnIsolatedWorldCreated(int execution_context_id) {
    main_frame_execution_context_id_ = execution_context_id;

    HeadlessTabSocket* headless_tab_socket =
        web_contents_->GetHeadlessTabSocket();
    DCHECK(headless_tab_socket);
    headless_tab_socket->SendMessageToContext(
        "Hello!!!", *main_frame_execution_context_id_);
    headless_tab_socket->SetListener(this);

    devtools_client_->GetRuntime()->Evaluate(
        runtime::EvaluateParams::Builder()
            .SetExpression(
                R"(window.TabSocket.onmessage =
                    function(message) {
                      TabSocket.send('Embedder sent us: ' + message);
                    };
                  )")
            .SetContextId(GetV8ExecutionContextIdByWorldName("Test World"))
            .Build(),
        base::Bind(
            &IsolatedWorldHeadlessTabSocketTest::FailOnJsEvaluateException,
            base::Unretained(this)));
  }

  void OnMessageFromContext(const std::string& message,
                            int execution_context_id) override {
    EXPECT_EQ("Embedder sent us: Hello!!!", message);
    EXPECT_EQ(*main_frame_execution_context_id_, execution_context_id);
    FinishAsynchronousTest();
  }

  base::Optional<int> main_frame_execution_context_id_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(IsolatedWorldHeadlessTabSocketTest);

class MultipleIframesIsolatedWorldHeadlessTabSocketTest : public TabSocketTest {
 public:
  void RunTabSocketTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/two_iframes.html").spec());
  }

  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    devtools_client_->GetPage()->Disable();
    devtools_client_->GetPage()->RemoveObserver(this);
    devtools_client_->GetDOMSnapshot()->GetExperimental()->GetSnapshot(
        dom_snapshot::GetSnapshotParams::Builder()
            .SetComputedStyleWhitelist(std::vector<std::string>())
            .Build(),
        base::Bind(
            &MultipleIframesIsolatedWorldHeadlessTabSocketTest::OnSnapshot,
            base::Unretained(this)));
  }

  void OnSnapshot(std::unique_ptr<dom_snapshot::GetSnapshotResult> result) {
    bool seen_main_frame = false;
    for (const auto& node : *result->GetDomNodes()) {
      if (node->HasFrameId()) {
        std::string frame_name;
        if (node->GetNodeName() == "IFRAME") {
          // Use the iframe id attribute for the name.
          for (const auto& key_value : *node->GetAttributes()) {
            if (key_value->GetName() == "id") {
              frame_name = key_value->GetValue();
            }
          }
          CHECK(!frame_name.empty());
        } else {
          if (seen_main_frame)
            continue;
          seen_main_frame = true;
          frame_name = "main frame";
        }
        CreateIsolatedWorldTabSocket(
            frame_name, node->GetFrameId(),
            base::Bind(&MultipleIframesIsolatedWorldHeadlessTabSocketTest::
                           OnIsolatedWorldCreated,
                       base::Unretained(this), frame_name));
      }
    }
  }

  void OnIsolatedWorldCreated(std::string frame_name,
                              int execution_context_id) {
    HeadlessTabSocket* headless_tab_socket =
        web_contents_->GetHeadlessTabSocket();
    DCHECK(headless_tab_socket);
    headless_tab_socket->SendMessageToContext("Hello!!!", execution_context_id);
    headless_tab_socket->SetListener(this);

    devtools_client_->GetRuntime()->Evaluate(
        runtime::EvaluateParams::Builder()
            .SetExpression(base::StringPrintf(
                R"(window.TabSocket.onmessage =
                    function(message) {
                      TabSocket.send('Echo from %s: ' + message);
                    };
                  )",
                frame_name.c_str()))
            .SetContextId(execution_context_id)
            .Build(),
        base::Bind(&MultipleIframesIsolatedWorldHeadlessTabSocketTest::
                       FailOnJsEvaluateException,
                   base::Unretained(this)));
  }

  void OnMessageFromContext(const std::string& message,
                            int execution_context_id) override {
    messages_.push_back(message);
    if (messages_.size() < 3)
      return;
    EXPECT_THAT(messages_,
                UnorderedElementsAre("Echo from main frame: Hello!!!",
                                     "Echo from iframe1: Hello!!!",
                                     "Echo from iframe2: Hello!!!"));
    FinishAsynchronousTest();
  }

  std::vector<std::string> messages_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(
    MultipleIframesIsolatedWorldHeadlessTabSocketTest);

class SingleTabMultipleIsolatedWorldsHeadlessTabSocketTest
    : public TabSocketTest {
 public:
  void RunTabSocketTest() override {
    CreateIsolatedWorldTabSocket(
        "Isolated World 1", main_frame_id(),
        base::Bind(&SingleTabMultipleIsolatedWorldsHeadlessTabSocketTest::
                       OnIsolatedWorldCreated,
                   base::Unretained(this), "Isolated World 1"));

    CreateIsolatedWorldTabSocket(
        "Isolated World 2", main_frame_id(),
        base::Bind(&SingleTabMultipleIsolatedWorldsHeadlessTabSocketTest::
                       OnIsolatedWorldCreated,
                   base::Unretained(this), "Isolated World 2"));

    CreateIsolatedWorldTabSocket(
        "Isolated World 3", main_frame_id(),
        base::Bind(&SingleTabMultipleIsolatedWorldsHeadlessTabSocketTest::
                       OnIsolatedWorldCreated,
                   base::Unretained(this), "Isolated World 3"));
  }

  void OnIsolatedWorldCreated(std::string frame_name,
                              int execution_context_id) {
    HeadlessTabSocket* headless_tab_socket =
        web_contents_->GetHeadlessTabSocket();
    DCHECK(headless_tab_socket);
    headless_tab_socket->SendMessageToContext("Hello!!!", execution_context_id);
    headless_tab_socket->SetListener(this);

    devtools_client_->GetRuntime()->Evaluate(
        runtime::EvaluateParams::Builder()
            .SetExpression(base::StringPrintf(
                R"(window.TabSocket.onmessage =
                    function(message) {
                      TabSocket.send('Echo from %s: ' + message);
                    };
                  )",
                frame_name.c_str()))
            .SetContextId(execution_context_id)
            .Build(),
        base::Bind(&SingleTabMultipleIsolatedWorldsHeadlessTabSocketTest::
                       FailOnJsEvaluateException,
                   base::Unretained(this)));
  }

  void OnMessageFromContext(const std::string& message,
                            int execution_context_id) override {
    messages_.push_back(message);
    if (messages_.size() < 3)
      return;
    EXPECT_THAT(messages_,
                UnorderedElementsAre("Echo from Isolated World 1: Hello!!!",
                                     "Echo from Isolated World 2: Hello!!!",
                                     "Echo from Isolated World 3: Hello!!!"));
    FinishAsynchronousTest();
  }

  std::vector<std::string> messages_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(
    SingleTabMultipleIsolatedWorldsHeadlessTabSocketTest);

}  // namespace headless
