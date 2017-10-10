// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/dom_snapshot.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/devtools/domains/headless_experimental.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/devtools/domains/security.h"
#include "headless/public/devtools/domains/target.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_web_contents.h"
#include "headless/public/util/testing/test_in_memory_protocol_handler.h"
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
using testing::ElementsAreArray;
using testing::Not;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

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

  browser_context->RemoveObserver(&observer);
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

  browser_context->RemoveObserver(this);
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

// Regression test for https://crbug.com/733569.
class HeadlessWebContentsRequestStorageQuotaTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public runtime::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    base::RunLoop run_loop;
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    devtools_client_->GetRuntime()->AddObserver(this);
    devtools_client_->GetRuntime()->Enable(run_loop.QuitClosure());
    run_loop.Run();

    // Should not crash and call console.log() if quota request succeeds.
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/request_storage_quota.html").spec());
  }

  void OnConsoleAPICalled(
      const runtime::ConsoleAPICalledParams& params) override {
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessWebContentsRequestStorageQuotaTest);

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, BrowserTabChangeContent) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::string script = "window.location = '" +
                       embedded_test_server()->GetURL("/hello.html").spec() +
                       "';";
  EXPECT_FALSE(EvaluateScript(web_contents, script)->HasExceptionDetails());

  // This will time out if the previous script did not work.
  EXPECT_TRUE(WaitForLoad(web_contents));
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, BrowserOpenInTab) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  MockHeadlessBrowserContextObserver observer;
  browser_context->AddObserver(&observer);
  EXPECT_CHILD_CONTENTS_CREATED(observer);

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/link.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  EXPECT_EQ(1u, browser_context->GetAllWebContents().size());
  // Simulates a middle-button click on a link to ensure that the
  // link is opened in a new tab by the browser and not by the renderer.
  std::string script =
      "var event = new MouseEvent('click', {'button': 1});"
      "document.getElementsByTagName('a')[0].dispatchEvent(event);";
  EXPECT_FALSE(EvaluateScript(web_contents, script)->HasExceptionDetails());

  // Check that we have a new tab.
  EXPECT_EQ(2u, browser_context->GetAllWebContents().size());
  browser_context->RemoveObserver(&observer);
}

namespace {
const char* kRequestOrderTestPage = R"(
<html>
  <body>
    <img src="img0">
    <img src="img1">
    <img src="img2">
    <img src="img3">
    <img src="img4">
    <img src="img5">
    <img src="img6">
    <script src='script7' async></script>
    <script>
      var xhr = new XMLHttpRequest();
      xhr.open('GET', 'xhr8');
      xhr.send();
    </script>
    <iframe src=frame9></iframe>
    <img src="img10">
    <img src="img11">
  </body>
</html> )";

const char* kRequestOrderTestPageUrls[] = {
    "http://foo.com/index.html", "http://foo.com/img0",
    "http://foo.com/img1",       "http://foo.com/img2",
    "http://foo.com/img3",       "http://foo.com/img4",
    "http://foo.com/img5",       "http://foo.com/img6",
    "http://foo.com/script7",    "http://foo.com/img10",
    "http://foo.com/img11",      "http://foo.com/xhr8",
    "http://foo.com/frame9"};
}  // namespace

class ResourceSchedulerTest : public HeadlessAsyncDevTooledBrowserTest,
                              public page::Observer {
 public:
  void SetUp() override {
    options()->enable_resource_scheduler = GetEnableResourceScheduler();
    HeadlessBrowserTest::SetUp();
  }

  void RunDevTooledTest() override {
    http_handler_->SetHeadlessBrowserContext(browser_context_);
    devtools_client_->GetPage()->AddObserver(this);

    base::RunLoop run_loop;
    devtools_client_->GetPage()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();

    devtools_client_->GetPage()->Navigate("http://foo.com/index.html");
  }

  virtual bool GetEnableResourceScheduler() = 0;

  ProtocolHandlerMap GetProtocolHandlers() override {
    ProtocolHandlerMap protocol_handlers;
    std::unique_ptr<TestInMemoryProtocolHandler> http_handler(
        new TestInMemoryProtocolHandler(browser()->BrowserIOThread(),
                                        /* simulate_slow_fetch */ true));
    http_handler_ = http_handler.get();
    http_handler_->InsertResponse("http://foo.com/index.html",
                                  {kRequestOrderTestPage, "text/html"});
    protocol_handlers[url::kHttpScheme] = std::move(http_handler);
    return protocol_handlers;
  }

  const TestInMemoryProtocolHandler* http_handler() const {
    return http_handler_;
  }

 private:
  TestInMemoryProtocolHandler* http_handler_;  // NOT OWNED
};

// TODO(alexclarke): Fix the flakes. http://crbug.com/766884
class DISABLED_DisableResourceSchedulerTest : public ResourceSchedulerTest {
 public:
  bool GetEnableResourceScheduler() override { return false; }

  void OnLoadEventFired(const page::LoadEventFiredParams&) override {
    EXPECT_THAT(http_handler()->urls_requested(),
                ElementsAreArray(kRequestOrderTestPageUrls));
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(DISABLED_DisableResourceSchedulerTest);

class EnableResourceSchedulerTest : public ResourceSchedulerTest {
 public:
  bool GetEnableResourceScheduler() override {
    return true;  // The default value.
  }

  void OnLoadEventFired(const page::LoadEventFiredParams&) override {
    // We expect a different resource order when the ResourceScheduler is used.
    EXPECT_THAT(http_handler()->urls_requested(),
                Not(ElementsAreArray(kRequestOrderTestPageUrls)));
    // However all the same urls should still be requested.
    EXPECT_THAT(http_handler()->urls_requested(),
                UnorderedElementsAreArray(kRequestOrderTestPageUrls));
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(EnableResourceSchedulerTest);

// BeginFrameControl is not supported on MacOS.
#if !defined(OS_MACOSX)

class HeadlessWebContentsBeginFrameControlTest
    : public HeadlessBrowserTest,
      public headless_experimental::ExperimentalObserver,
      public page::Observer {
 public:
  HeadlessWebContentsBeginFrameControlTest()
      : browser_devtools_client_(HeadlessDevToolsClient::Create()),
        devtools_client_(HeadlessDevToolsClient::Create()) {}

  void SetUp() override {
    EnablePixelOutput();
    HeadlessBrowserTest::SetUp();
  }

 protected:
  virtual std::string GetTestHtmlFile() = 0;
  virtual void OnNeedsBeginFrame() {}
  virtual void OnFrameFinished(
      std::unique_ptr<headless_experimental::BeginFrameResult> result) {}

  void RunTest() {
    browser_context_ = browser()->CreateBrowserContextBuilder().Build();
    browser()->SetDefaultBrowserContext(browser_context_);
    browser()->GetDevToolsTarget()->AttachClient(
        browser_devtools_client_.get());

    EXPECT_TRUE(embedded_test_server()->Start());

    browser_devtools_client_->GetTarget()->GetExperimental()->CreateTarget(
        target::CreateTargetParams::Builder()
            .SetUrl("about://blank")
            .SetWidth(200)
            .SetHeight(200)
            .SetEnableBeginFrameControl(true)
            .Build(),
        base::Bind(
            &HeadlessWebContentsBeginFrameControlTest::OnCreateTargetResult,
            base::Unretained(this)));

    RunAsynchronousTest();

    browser()->GetDevToolsTarget()->DetachClient(
        browser_devtools_client_.get());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(cc::switches::kRunAllCompositorStagesBeforeDraw);
  }

  void OnCreateTargetResult(
      std::unique_ptr<target::CreateTargetResult> result) {
    web_contents_ = HeadlessWebContentsImpl::From(
        browser()->GetWebContentsForDevToolsAgentHostId(result->GetTargetId()));

    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    devtools_client_->GetHeadlessExperimental()->GetExperimental()->AddObserver(
        this);
    devtools_client_->GetHeadlessExperimental()->GetExperimental()->Enable(
        headless_experimental::EnableParams::Builder().Build());

    devtools_client_->GetPage()->GetExperimental()->StopLoading(
        page::StopLoadingParams::Builder().Build(),
        base::Bind(&HeadlessWebContentsBeginFrameControlTest::LoadingStopped,
                   base::Unretained(this)));
  }

  void LoadingStopped(std::unique_ptr<page::StopLoadingResult>) {
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable(
        base::Bind(&HeadlessWebContentsBeginFrameControlTest::PageDomainEnabled,
                   base::Unretained(this)));
  }

  void PageDomainEnabled() {
    devtools_client_->GetPage()->Navigate(
        page::NavigateParams::Builder()
            .SetUrl(embedded_test_server()->GetURL(GetTestHtmlFile()).spec())
            .Build());
  }

  // page::Observer implementation:
  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    TRACE_EVENT0("headless",
                 "HeadlessWebContentsBeginFrameControlTest::OnLoadEventFired");
    devtools_client_->GetPage()->Disable();
    devtools_client_->GetPage()->RemoveObserver(this);
    page_ready_ = true;
    if (needs_begin_frames_) {
      DCHECK(!frame_in_flight_);
      OnNeedsBeginFrame();
    }
  }

  // headless_experimental::ExperimentalObserver implementation:
  void OnNeedsBeginFramesChanged(
      const headless_experimental::NeedsBeginFramesChangedParams& params)
      override {
    TRACE_EVENT1(
        "headless",
        "HeadlessWebContentsBeginFrameControlTest::OnNeedsBeginFramesChanged",
        "needs_begin_frames", params.GetNeedsBeginFrames());
    needs_begin_frames_ = params.GetNeedsBeginFrames();
    if (needs_begin_frames_ && !frame_in_flight_ && page_ready_)
      OnNeedsBeginFrame();
  }

  void OnMainFrameReadyForScreenshots(
      const headless_experimental::MainFrameReadyForScreenshotsParams& params)
      override {
    TRACE_EVENT0("headless",
                 "HeadlessWebContentsBeginFrameControlTest::"
                 "OnMainFrameReadyForScreenshots");
    main_frame_ready_ = true;
  }

  void BeginFrame(bool screenshot) {
    if (!needs_begin_frames_ && !screenshot)
      return;

    frame_in_flight_ = true;

    auto builder = headless_experimental::BeginFrameParams::Builder();
    if (screenshot) {
      DCHECK(main_frame_ready_);
      builder.SetScreenshot(
          headless_experimental::ScreenshotParams::Builder().Build());
    }

    devtools_client_->GetHeadlessExperimental()->GetExperimental()->BeginFrame(
        builder.Build(),
        base::Bind(&HeadlessWebContentsBeginFrameControlTest::FrameFinished,
                   base::Unretained(this)));
  }

  void FrameFinished(
      std::unique_ptr<headless_experimental::BeginFrameResult> result) {
    TRACE_EVENT2("headless",
                 "HeadlessWebContentsBeginFrameControlTest::FrameFinished",
                 "has_damage", result->GetHasDamage(), "has_screenshot_data",
                 result->HasScreenshotData());

    // Post OnFrameFinished call so that any pending OnNeedsBeginFramesChanged
    // call will be executed first.
    browser()->BrowserMainThread()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &HeadlessWebContentsBeginFrameControlTest::NotifyOnFrameFinished,
            base::Unretained(this), std::move(result)));
  }

  void NotifyOnFrameFinished(
      std::unique_ptr<headless_experimental::BeginFrameResult> result) {
    frame_in_flight_ = false;
    OnFrameFinished(std::move(result));
  }

  void PostFinishAsynchronousTest() {
    devtools_client_->GetHeadlessExperimental()
        ->GetExperimental()
        ->RemoveObserver(this);

    browser()->BrowserMainThread()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &HeadlessWebContentsBeginFrameControlTest::FinishAsynchronousTest,
            base::Unretained(this)));
  }

  HeadlessBrowserContext* browser_context_ = nullptr;  // Not owned.
  HeadlessWebContentsImpl* web_contents_ = nullptr;    // Not owned.

  bool page_ready_ = false;
  bool needs_begin_frames_ = false;
  bool frame_in_flight_ = false;
  bool main_frame_ready_ = false;
  std::unique_ptr<HeadlessDevToolsClient> browser_devtools_client_;
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
};

class HeadlessWebContentsBeginFrameControlBasicTest
    : public HeadlessWebContentsBeginFrameControlTest {
 public:
  HeadlessWebContentsBeginFrameControlBasicTest() {}

 protected:
  std::string GetTestHtmlFile() override {
    // Blue background.
    return "/blue_page.html";
  }

  void OnNeedsBeginFrame() override { BeginFrame(false); }

  void OnFrameFinished(std::unique_ptr<headless_experimental::BeginFrameResult>
                           result) override {
    if (!sent_screenshot_request_) {
      // Once no more BeginFrames are needed and the main frame is ready,
      // capture a screenshot.
      sent_screenshot_request_ = !needs_begin_frames_ && main_frame_ready_;
      BeginFrame(sent_screenshot_request_);
    } else {
      EXPECT_TRUE(result->GetHasDamage());
      EXPECT_TRUE(result->HasScreenshotData());
      if (result->HasScreenshotData()) {
        std::string base64 = result->GetScreenshotData();
        EXPECT_LT(0u, base64.length());
        SkBitmap result_bitmap;
        EXPECT_TRUE(DecodePNG(base64, &result_bitmap));

        EXPECT_EQ(200, result_bitmap.width());
        EXPECT_EQ(200, result_bitmap.height());
        SkColor expected_color = SkColorSetRGB(0x00, 0x00, 0xff);
        SkColor actual_color = result_bitmap.getColor(100, 100);
        EXPECT_EQ(expected_color, actual_color);
      }

      // Post completion to avoid deleting the WebContents on the same callstack
      // as frame finished callback.
      PostFinishAsynchronousTest();
    }
  }

  bool sent_screenshot_request_ = false;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessWebContentsBeginFrameControlBasicTest);

// TODO(eseckler).
class HeadlessWebContentsBeginFrameControlViewportTest
    : public HeadlessWebContentsBeginFrameControlTest {
 public:
  HeadlessWebContentsBeginFrameControlViewportTest() {}

 protected:
  std::string GetTestHtmlFile() override {
    // Draws a 100x100px blue box at 200x200px.
    return "/blue_box.html";
  }

  void OnNeedsBeginFrame() override { BeginFrame(false); }

  void OnFrameFinished(std::unique_ptr<headless_experimental::BeginFrameResult>
                           result) override {
    if (!sent_screenshot_request_) {
      // Once no more BeginFrames are needed and the main frame is ready,
      // set the view size and position and then capture a screenshot.
      if (!needs_begin_frames_ && main_frame_ready_) {
        if (did_set_up_viewport_) {
          // Finally, capture a screenshot.
          sent_screenshot_request_ = true;
          BeginFrame(true);
        } else {
          SetUpViewport();
        }
        return;
      }

      BeginFrame(false);
    } else {
      EXPECT_TRUE(result->GetHasDamage());
      EXPECT_TRUE(result->HasScreenshotData());
      if (result->HasScreenshotData()) {
        std::string base64 = result->GetScreenshotData();
        EXPECT_LT(0u, base64.length());
        SkBitmap result_bitmap;
        EXPECT_TRUE(DecodePNG(base64, &result_bitmap));

        EXPECT_EQ(200, result_bitmap.width());
        EXPECT_EQ(200, result_bitmap.height());
        SkColor expected_color = SkColorSetRGB(0x00, 0x00, 0xff);

        SkColor actual_color = result_bitmap.getColor(100, 100);
        EXPECT_EQ(expected_color, actual_color);
        actual_color = result_bitmap.getColor(0, 0);
        EXPECT_EQ(expected_color, actual_color);
        actual_color = result_bitmap.getColor(0, 199);
        EXPECT_EQ(expected_color, actual_color);
        actual_color = result_bitmap.getColor(199, 0);
        EXPECT_EQ(expected_color, actual_color);
        actual_color = result_bitmap.getColor(199, 199);
        EXPECT_EQ(expected_color, actual_color);
      }

      // Post completion to avoid deleting the WebContents on the same callstack
      // as frame finished callback.
      PostFinishAsynchronousTest();
    }
  }

  void SetUpViewport() {
    did_set_up_viewport_ = 1;
    // We should be needing BeginFrames again because of the viewport change.
    devtools_client_->GetEmulation()
        ->GetExperimental()
        ->SetDeviceMetricsOverride(
            emulation::SetDeviceMetricsOverrideParams::Builder()
                .SetWidth(0)
                .SetHeight(0)
                .SetDeviceScaleFactor(0)
                .SetMobile(false)
                .SetViewport(page::Viewport::Builder()
                                 .SetX(200)
                                 .SetY(200)
                                 .SetWidth(100)
                                 .SetHeight(100)
                                 .SetScale(2)
                                 .Build())
                .Build());
  }

  bool sent_screenshot_request_ = false;
  bool did_set_up_viewport_ = false;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(
    HeadlessWebContentsBeginFrameControlViewportTest);

#endif  // !defined(OS_MACOSX)

}  // namespace headless
