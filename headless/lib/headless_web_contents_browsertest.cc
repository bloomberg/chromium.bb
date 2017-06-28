// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/devtools/domains/security.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_tab_socket.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
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
}

class HeadlessWindowOpenTabSocketTest : public HeadlessBrowserTest,
                                        public HeadlessTabSocket::Listener,
                                        public HeadlessBrowserContext::Observer,
                                        public HeadlessWebContents::Observer {
 public:
  HeadlessWindowOpenTabSocketTest()
      : devtools_client_(HeadlessDevToolsClient::Create()) {}

  void SetUp() override {
    options()->mojo_service_names.insert("headless::TabSocket");
    HeadlessBrowserTest::SetUp();
  }

  // HeadlessTabSocket::Listener implementation.
  void OnMessageFromTab(const std::string& message) override {
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

    HeadlessTabSocket* tab_socket = child_->GetHeadlessTabSocket();
    tab_socket->SendMessageToTab("One");
    tab_socket->SetListener(this);
  }

 protected:
  std::string message_;
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
          .SetTabSocketType(
              HeadlessWebContents::Builder::TabSocketType::MAIN_WORLD)
          .SetInitialURL(embedded_test_server()->GetURL("/window_open.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  EXPECT_EQ(2u, browser_context->GetAllWebContents().size());
  EXPECT_NE(nullptr, child_);

  RunAsynchronousTest();
  EXPECT_EQ("Embedder sent us: One", message_);
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

  HeadlessWebContents::Builder::TabSocketType GetTabSocketType() override {
    return HeadlessWebContents::Builder::TabSocketType::NONE;
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(GetHeadlessTabSocketButNoTabSocket);

class HeadlessMainWorldTabSocketTest : public HeadlessAsyncDevTooledBrowserTest,
                                       public HeadlessTabSocket::Listener {
 public:
  void SetUp() override {
    options()->mojo_service_names.insert("headless::TabSocket");
    HeadlessAsyncDevTooledBrowserTest::SetUp();
  }

  void RunDevTooledTest() override {
    devtools_client_->GetRuntime()->Evaluate(
        R"(window.TabSocket.onmessage =
            function(message) {
              window.TabSocket.send('Embedder sent us: ' + message);
            };
          )");

    HeadlessTabSocket* headless_tab_socket =
        web_contents_->GetHeadlessTabSocket();
    DCHECK(headless_tab_socket);

    headless_tab_socket->SendMessageToTab("One");
    headless_tab_socket->SendMessageToTab("Two");
    headless_tab_socket->SendMessageToTab("Three");
    headless_tab_socket->SetListener(this);
  }

  void OnMessageFromTab(const std::string& message) override {
    messages_.push_back(message);
    if (messages_.size() == 3u) {
      EXPECT_THAT(messages_,
                  ElementsAre("Embedder sent us: One", "Embedder sent us: Two",
                              "Embedder sent us: Three"));
      FinishAsynchronousTest();
    }
  }

  HeadlessWebContents::Builder::TabSocketType GetTabSocketType() override {
    return HeadlessWebContents::Builder::TabSocketType::MAIN_WORLD;
  }

 private:
  std::vector<std::string> messages_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessMainWorldTabSocketTest);

class HeadlessMainWorldTabSocketNotThereTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public HeadlessTabSocket::Listener {
 public:
  void SetUp() override {
    options()->mojo_service_names.insert("headless::TabSocket");
    HeadlessAsyncDevTooledBrowserTest::SetUp();
  }

  void RunDevTooledTest() override {
    // We expect this to fail because the TabSocket is being injected into
    // isolated worlds.
    devtools_client_->GetRuntime()->Evaluate(
        "window.TabSocket.send('This should not work!');",
        base::Bind(&HeadlessMainWorldTabSocketNotThereTest::EvaluateResult,
                   base::Unretained(this)));

    HeadlessTabSocket* headless_tab_socket =
        web_contents_->GetHeadlessTabSocket();
    DCHECK(headless_tab_socket);

    headless_tab_socket->SetListener(this);
  }

  void EvaluateResult(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_TRUE(result->HasExceptionDetails());
    FinishAsynchronousTest();
  }

  void OnMessageFromTab(const std::string&) override {
#ifdef OS_WIN
    FinishAsynchronousTest();
    FAIL() << "Should not receive a message from the tab!";
#else
    FAIL() << "Should not receive a message from the tab!";
    FinishAsynchronousTest();
#endif
  }

  HeadlessWebContents::Builder::TabSocketType GetTabSocketType() override {
    return HeadlessWebContents::Builder::TabSocketType::ISOLATED_WORLD;
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessMainWorldTabSocketNotThereTest);

class HeadlessIsolatedWorldTabSocketTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public HeadlessTabSocket::Listener,
      public runtime::Observer {
 public:
  void SetUp() override {
    options()->mojo_service_names.insert("headless::TabSocket");
    HeadlessAsyncDevTooledBrowserTest::SetUp();
  }

  void RunDevTooledTest() override {
    devtools_client_->GetRuntime()->AddObserver(this);
    devtools_client_->GetRuntime()->Enable();

    devtools_client_->GetPage()->GetExperimental()->GetResourceTree(
        page::GetResourceTreeParams::Builder().Build(),
        base::Bind(&HeadlessIsolatedWorldTabSocketTest::OnResourceTree,
                   base::Unretained(this)));

    HeadlessTabSocket* headless_tab_socket =
        web_contents_->GetHeadlessTabSocket();
    DCHECK(headless_tab_socket);
    headless_tab_socket->SendMessageToTab("Hello!!!");
    headless_tab_socket->SetListener(this);
  }

  void OnResourceTree(std::unique_ptr<page::GetResourceTreeResult> result) {
    main_frame_id_ = result->GetFrameTree()->GetFrame()->GetId();
    devtools_client_->GetPage()->GetExperimental()->CreateIsolatedWorld(
        page::CreateIsolatedWorldParams::Builder()
            .SetFrameId(main_frame_id_)
            .Build());
  }

  void OnExecutionContextCreated(
      const runtime::ExecutionContextCreatedParams& params) override {
    const base::DictionaryValue* dictionary;
    std::string frame_id;
    bool is_main_world;
    // If the isolated world was created then eval some script in it.
    if (params.GetContext()->HasAuxData() &&
        params.GetContext()->GetAuxData()->GetAsDictionary(&dictionary) &&
        dictionary->GetString("frameId", &frame_id) &&
        frame_id == main_frame_id_ &&
        dictionary->GetBoolean("isDefault", &is_main_world) && !is_main_world) {
      devtools_client_->GetRuntime()->Evaluate(
          runtime::EvaluateParams::Builder()
              .SetExpression(
                  R"(window.TabSocket.onmessage =
                      function(message) {
                        TabSocket.send('Embedder sent us: ' + message);
                      };
                    )")
              .SetContextId(params.GetContext()->GetId())
              .Build());
    }
  }

  void OnMessageFromTab(const std::string& message) override {
    EXPECT_EQ("Embedder sent us: Hello!!!", message);
    FinishAsynchronousTest();
  }

  HeadlessWebContents::Builder::TabSocketType GetTabSocketType() override {
    return HeadlessWebContents::Builder::TabSocketType::ISOLATED_WORLD;
  }

 private:
  std::string main_frame_id_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessIsolatedWorldTabSocketTest);

class HeadlessIsolatedWorldTabSocketNotThereTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public HeadlessTabSocket::Listener,
      public runtime::Observer {
 public:
  void SetUp() override {
    options()->mojo_service_names.insert("headless::TabSocket");
    HeadlessAsyncDevTooledBrowserTest::SetUp();
  }

  void RunDevTooledTest() override {
    devtools_client_->GetRuntime()->AddObserver(this);
    devtools_client_->GetRuntime()->Enable();

    devtools_client_->GetPage()->GetExperimental()->GetResourceTree(
        page::GetResourceTreeParams::Builder().Build(),
        base::Bind(&HeadlessIsolatedWorldTabSocketNotThereTest::OnResourceTree,
                   base::Unretained(this)));

    HeadlessTabSocket* headless_tab_socket =
        web_contents_->GetHeadlessTabSocket();
    DCHECK(headless_tab_socket);
    headless_tab_socket->SendMessageToTab("Hello!!!");
    headless_tab_socket->SetListener(this);
  }

  void OnResourceTree(std::unique_ptr<page::GetResourceTreeResult> result) {
    main_frame_id_ = result->GetFrameTree()->GetFrame()->GetId();
    devtools_client_->GetPage()->GetExperimental()->CreateIsolatedWorld(
        page::CreateIsolatedWorldParams::Builder()
            .SetFrameId(main_frame_id_)
            .Build());
  }

  void OnExecutionContextCreated(
      const runtime::ExecutionContextCreatedParams& params) override {
    const base::DictionaryValue* dictionary;
    std::string frame_id;
    bool is_main_world;
    // If the isolated world was created then eval some script in it.
    if (params.GetContext()->HasAuxData() &&
        params.GetContext()->GetAuxData()->GetAsDictionary(&dictionary) &&
        dictionary->GetString("frameId", &frame_id) &&
        frame_id == main_frame_id_ &&
        dictionary->GetBoolean("isDefault", &is_main_world) && !is_main_world) {
      // We expect this to fail because the TabSocket is being injected into the
      // main world.
      devtools_client_->GetRuntime()->Evaluate(
          runtime::EvaluateParams::Builder()
              .SetExpression("window.TabSocket.send('This should not work!');")
              .SetContextId(params.GetContext()->GetId())
              .Build(),
          base::Bind(
              &HeadlessIsolatedWorldTabSocketNotThereTest::EvaluateResult,
              base::Unretained(this)));
    }
  }

  void EvaluateResult(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_TRUE(result->HasExceptionDetails());
    FinishAsynchronousTest();
  }

  void OnMessageFromTab(const std::string&) override {
#ifdef OS_WIN
    FinishAsynchronousTest();
    FAIL() << "Should not receive a message from the tab!";
#else
    FAIL() << "Should not receive a message from the tab!";
    FinishAsynchronousTest();
#endif
  }

  HeadlessWebContents::Builder::TabSocketType GetTabSocketType() override {
    return HeadlessWebContents::Builder::TabSocketType::MAIN_WORLD;
  }

 private:
  std::string main_frame_id_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessIsolatedWorldTabSocketNotThereTest);

}  // namespace headless
