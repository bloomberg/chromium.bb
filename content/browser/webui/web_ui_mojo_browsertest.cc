// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_registry.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/data/web_ui_test_mojo_bindings.mojom.h"
#include "mojo/common/test/test_utils.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/js/bindings/constants.h"

namespace content {
namespace {

bool got_message = false;

// The bindings for the page are generated from a .mojom file. This code looks
// up the generated file from disk and returns it.
bool GetResource(const std::string& id,
                 const WebUIDataSource::GotDataCallback& callback) {
  // These are handled by the WebUIDataSource that AddMojoDataSource() creates.
  if (id == mojo::kBufferModuleName ||
      id == mojo::kCodecModuleName ||
      id == mojo::kConnectionModuleName ||
      id == mojo::kConnectorModuleName ||
      id == mojo::kUnicodeModuleName ||
      id == mojo::kRouterModuleName ||
      id == mojo::kValidatorModuleName)
    return false;

  std::string contents;
  CHECK(base::ReadFileToString(mojo::test::GetFilePathForJSResource(id),
                               &contents,
                               std::string::npos)) << id;
  base::RefCountedString* ref_contents = new base::RefCountedString;
  ref_contents->data() = contents;
  callback.Run(ref_contents);
  return true;
}

class BrowserTargetImpl : public mojo::InterfaceImpl<BrowserTarget> {
 public:
  explicit BrowserTargetImpl(base::RunLoop* run_loop) : run_loop_(run_loop) {}

  virtual ~BrowserTargetImpl() {}

  // mojo::InterfaceImpl<BrowserTarget> overrides:
  virtual void PingResponse() OVERRIDE {
    NOTREACHED();
  }

 protected:
  base::RunLoop* run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserTargetImpl);
};

class PingBrowserTargetImpl : public BrowserTargetImpl {
 public:
  explicit PingBrowserTargetImpl(base::RunLoop* run_loop)
      : BrowserTargetImpl(run_loop) {}

  virtual ~PingBrowserTargetImpl() {}

  // mojo::InterfaceImpl<BrowserTarget> overrides:
  virtual void OnConnectionEstablished() OVERRIDE {
    client()->Ping();
  }

  // Quit the RunLoop when called.
  virtual void PingResponse() OVERRIDE {
    got_message = true;
    run_loop_->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PingBrowserTargetImpl);
};

// WebUIController that sets up mojo bindings.
class TestWebUIController : public WebUIController {
 public:
   TestWebUIController(WebUI* web_ui, base::RunLoop* run_loop)
      : WebUIController(web_ui),
        run_loop_(run_loop) {
    content::WebUIDataSource* data_source =
        WebUIDataSource::AddMojoDataSource(
            web_ui->GetWebContents()->GetBrowserContext());
    data_source->SetRequestFilter(base::Bind(&GetResource));
  }

 protected:
  base::RunLoop* run_loop_;
  scoped_ptr<BrowserTargetImpl> browser_target_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIController);
};

// TestWebUIController that additionally creates the ping test BrowserTarget
// implementation at the right time.
class PingTestWebUIController : public TestWebUIController {
 public:
   PingTestWebUIController(WebUI* web_ui, base::RunLoop* run_loop)
       : TestWebUIController(web_ui, run_loop) {
   }
   virtual ~PingTestWebUIController() {}

  // WebUIController overrides:
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE {
    render_view_host->GetMainFrame()->GetServiceRegistry()->
        AddService<BrowserTarget>(base::Bind(
            &PingTestWebUIController::CreateHandler, base::Unretained(this)));
  }

  void CreateHandler(mojo::InterfaceRequest<BrowserTarget> request) {
    browser_target_.reset(mojo::WeakBindToRequest(
        new PingBrowserTargetImpl(run_loop_), &request));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PingTestWebUIController);
};

// WebUIControllerFactory that creates TestWebUIController.
class TestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() : run_loop_(NULL) {}

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }

  virtual WebUIController* CreateWebUIControllerForURL(
      WebUI* web_ui, const GURL& url) const OVERRIDE {
    if (url.query() == "ping")
      return new PingTestWebUIController(web_ui, run_loop_);
    return NULL;
  }
  virtual WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
      const GURL& url) const OVERRIDE {
    return reinterpret_cast<WebUI::TypeID>(1);
  }
  virtual bool UseWebUIForURL(BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE {
    return true;
  }
  virtual bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                                      const GURL& url) const OVERRIDE {
    return true;
  }

 private:
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestWebUIControllerFactory);
};

class WebUIMojoTest : public ContentBrowserTest {
 public:
  WebUIMojoTest() {
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  virtual ~WebUIMojoTest() {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  TestWebUIControllerFactory* factory() { return &factory_; }

 private:
  TestWebUIControllerFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUIMojoTest);
};

// Loads a webui page that contains mojo bindings and verifies a message makes
// it from the browser to the page and back.
IN_PROC_BROWSER_TEST_F(WebUIMojoTest, EndToEndPing) {
  // Currently there is no way to have a generated file included in the isolate
  // files. If the bindings file doesn't exist assume we're on such a bot and
  // pass.
  // TODO(sky): remove this conditional when isolates support copying from gen.
  const base::FilePath test_file_path(
      mojo::test::GetFilePathForJSResource(
          "content/test/data/web_ui_test_mojo_bindings.mojom"));
  if (!base::PathExists(test_file_path)) {
    LOG(WARNING) << " mojom binding file doesn't exist, assuming on isolate";
    return;
  }

  got_message = false;
  ASSERT_TRUE(test_server()->Start());
  base::RunLoop run_loop;
  factory()->set_run_loop(&run_loop);
  GURL test_url(test_server()->GetURL("files/web_ui_mojo.html?ping"));
  NavigateToURL(shell(), test_url);
  // RunLoop is quit when message received from page.
  run_loop.Run();
  EXPECT_TRUE(got_message);

  // Check that a second render frame in the same renderer process works
  // correctly.
  Shell* other_shell = CreateBrowser();
  got_message = false;
  base::RunLoop other_run_loop;
  factory()->set_run_loop(&other_run_loop);
  NavigateToURL(other_shell, test_url);
  // RunLoop is quit when message received from page.
  other_run_loop.Run();
  EXPECT_TRUE(got_message);
  EXPECT_EQ(shell()->web_contents()->GetRenderProcessHost(),
            other_shell->web_contents()->GetRenderProcessHost());
}

}  // namespace
}  // namespace content
