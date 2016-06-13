// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "ios/public/provider/web/web_ui_ios_controller.h"
#include "ios/public/provider/web/web_ui_ios_controller_factory.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_ui_ios_data_source.h"
#include "ios/web/test/grit/test_resources.h"
#include "ios/web/test/mojo_test.mojom.h"
#include "ios/web/test/test_url_constants.h"
#import "ios/web/test/web_int_test.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace web {

namespace {

// Hostname for test WebUI page.
const char kTestWebUIURLHost[] = "testwebui";

// UI handler class which communicates with test WebUI page as follows:
// - page sends "syn" message to |TestUIHandler|
// - |TestUIHandler| replies with "ack" message
// - page replies back with "fin"
//
// Once "fin" is received |IsFinReceived()| call will return true, indicating
// that communication was successful. See test WebUI page code here:
// ios/web/test/data/mojo_test.js
class TestUIHandler : public TestUIHandlerMojo,
                      public shell::InterfaceFactory<TestUIHandlerMojo> {
 public:
  TestUIHandler() {}
  ~TestUIHandler() override {}

  // Returns true if "fin" has been received.
  bool IsFinReceived() { return fin_received_; }

  // TestUIHandlerMojo overrides.
  void SetClientPage(TestPagePtr page) override { page_ = std::move(page); }
  void HandleJsMessage(const mojo::String& message) override {
    if (message.get() == "syn") {
      // Received "syn" message from WebUI page, send "ack" as reply.
      DCHECK(!syn_received_);
      DCHECK(!fin_received_);
      syn_received_ = true;
      NativeMessageResultMojoPtr result(NativeMessageResultMojo::New());
      result->message = mojo::String::From("ack");
      page_->HandleNativeMessage(std::move(result));
    } else if (message.get() == "fin") {
      // Received "fin" from the WebUI page in response to "ack".
      DCHECK(syn_received_);
      DCHECK(!fin_received_);
      fin_received_ = true;
    } else {
      NOTREACHED();
    }
  }

 private:
  // shell::InterfaceFactory overrides.
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<TestUIHandlerMojo> request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  mojo::BindingSet<TestUIHandlerMojo> bindings_;
  TestPagePtr page_ = nullptr;
  // |true| if "syn" has been received.
  bool syn_received_ = false;
  // |true| if "fin" has been received.
  bool fin_received_ = false;
};

// Controller for test WebUI.
class TestUI : public WebUIIOSController {
 public:
  // Constructs controller from |web_ui| and |ui_handler| which will communicate
  // with test WebUI page.
  TestUI(WebUIIOS* web_ui, TestUIHandler* ui_handler)
      : WebUIIOSController(web_ui) {
    web::WebUIIOSDataSource* source =
        web::WebUIIOSDataSource::Create(kTestWebUIURLHost);

    source->AddResourcePath("mojo_test.js", IDR_MOJO_TEST_JS);
    source->AddResourcePath("ios/web/test/mojo_test.mojom",
                            IDR_MOJO_TEST_MOJO_JS);
    source->SetDefaultResource(IDR_MOJO_TEST_HTML);

    web::WebState* web_state = web_ui->GetWebState();
    web::WebUIIOSDataSource::Add(web_state->GetBrowserState(), source);

    web_state->GetMojoInterfaceRegistry()->AddInterface(ui_handler);
  }
};

// Factory that creates TestUI controller.
class TestWebUIControllerFactory : public WebUIIOSControllerFactory {
 public:
  // Constructs a controller factory which will eventually create |ui_handler|.
  explicit TestWebUIControllerFactory(TestUIHandler* ui_handler)
      : ui_handler_(ui_handler) {}

  // WebUIIOSControllerFactory overrides.
  WebUIIOSController* CreateWebUIIOSControllerForURL(
      WebUIIOS* web_ui,
      const GURL& url) const override {
    DCHECK_EQ(url.scheme(), kTestWebUIScheme);
    DCHECK_EQ(url.host(), kTestWebUIURLHost);
    return new TestUI(web_ui, ui_handler_);
  }

 private:
  // UI handler class which communicates with test WebUI page.
  TestUIHandler* ui_handler_;
};
}  // namespace

// A test fixture for verifying mojo comminication for WebUI.
class WebUIMojoTest : public WebIntTest {
 protected:
  WebUIMojoTest()
      : web_state_(new WebStateImpl(GetBrowserState())),
        ui_handler_(new TestUIHandler()) {
    web_state_->GetNavigationManagerImpl().InitializeSession(nil, nil, NO, 0);
    WebUIIOSControllerFactory::RegisterFactory(
        new TestWebUIControllerFactory(ui_handler_.get()));
  }

  // Returns WebState which loads test WebUI page.
  WebStateImpl* web_state() { return web_state_.get(); }
  // Returns UI handler which communicates with WebUI page.
  TestUIHandler* test_ui_handler() { return ui_handler_.get(); }

 private:
  std::unique_ptr<WebStateImpl> web_state_;
  std::unique_ptr<TestUIHandler> ui_handler_;
};

// Tests that JS can send messages to the native code and vice versa.
// TestUIHandler is used for communication and test suceeds only when
// |TestUIHandler| sucessfully receives "ack" message from WebUI page.
TEST_F(WebUIMojoTest, MessageExchange) {
  web_state()->SetWebUsageEnabled(true);
  web_state()->GetWebController().useMojoForWebUI = YES;
  web_state()->GetView();  // WebState won't load URL without view.
  NavigationManager::WebLoadParams load_params(GURL(
      url::SchemeHostPort(kTestWebUIScheme, kTestWebUIURLHost, 0).Serialize()));
  web_state()->GetNavigationManager()->LoadURLWithParams(load_params);

  // Wait until |TestUIHandler| receives "ack" message from WebUI page.
  base::test::ios::WaitUntilCondition(^{
    base::RunLoop().RunUntilIdle();
    return test_ui_handler()->IsFinReceived();
  });
}

}  // namespace web
