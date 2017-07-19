// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#import "ios/testing/wait_util.h"
#include "ios/web/grit/ios_web_resources.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state_interface_provider.h"
#include "ios/web/public/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"
#include "ios/web/test/grit/test_resources.h"
#include "ios/web/test/mojo_test.mojom.h"
#include "ios/web/test/test_url_constants.h"
#import "ios/web/test/web_int_test.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace web {

namespace {

// Hostname for test WebUI page.
const char kTestWebUIURLHost[] = "testwebui";

// Timeout in seconds to wait for a sucessful message exchange between native
// code and a web page using Mojo.
const NSTimeInterval kMessageTimeout = 5.0;

// UI handler class which communicates with test WebUI page as follows:
// - page sends "syn" message to |TestUIHandler|
// - |TestUIHandler| replies with "ack" message
// - page replies back with "fin"
//
// Once "fin" is received |IsFinReceived()| call will return true, indicating
// that communication was successful. See test WebUI page code here:
// ios/web/test/data/mojo_test.js
class TestUIHandler : public TestUIHandlerMojo {
 public:
  TestUIHandler() {}
  ~TestUIHandler() override {}

  // Returns true if "fin" has been received.
  bool IsFinReceived() { return fin_received_; }

  // TestUIHandlerMojo overrides.
  void SetClientPage(TestPagePtr page) override { page_ = std::move(page); }
  void HandleJsMessage(const std::string& message) override {
    if (message == "syn") {
      // Received "syn" message from WebUI page, send "ack" as reply.
      DCHECK(!syn_received_);
      DCHECK(!fin_received_);
      syn_received_ = true;
      NativeMessageResultMojoPtr result(NativeMessageResultMojo::New());
      result->message = "ack";
      page_->HandleNativeMessage(std::move(result));
    } else if (message == "fin") {
      // Received "fin" from the WebUI page in response to "ack".
      DCHECK(syn_received_);
      DCHECK(!fin_received_);
      fin_received_ = true;
    } else {
      NOTREACHED();
    }
  }

  void BindTestUIHandlerMojoRequest(
      const service_manager::BindSourceInfo& souce_info,
      TestUIHandlerMojoRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
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
    source->AddResourcePath("mojo_bindings.js", IDR_IOS_MOJO_BINDINGS_JS);
    source->AddResourcePath("mojo_test.mojom.js", IDR_MOJO_TEST_MOJO_JS);
    source->SetDefaultResource(IDR_MOJO_TEST_HTML);

    web::WebState* web_state = web_ui->GetWebState();
    web::WebUIIOSDataSource::Add(web_state->GetBrowserState(), source);

    web_state->GetWebStateInterfaceProvider()->registry()->AddInterface(
        base::Bind(&TestUIHandler::BindTestUIHandlerMojoRequest,
                   base::Unretained(ui_handler)));
  }
};

// Factory that creates TestUI controller.
class TestWebUIControllerFactory : public WebUIIOSControllerFactory {
 public:
  // Constructs a controller factory which will eventually create |ui_handler|.
  explicit TestWebUIControllerFactory(TestUIHandler* ui_handler)
      : ui_handler_(ui_handler) {}

  // WebUIIOSControllerFactory overrides.
  std::unique_ptr<WebUIIOSController> CreateWebUIIOSControllerForURL(
      WebUIIOS* web_ui,
      const GURL& url) const override {
    DCHECK_EQ(url.scheme(), kTestWebUIScheme);
    DCHECK_EQ(url.host(), kTestWebUIURLHost);
    return base::MakeUnique<TestUI>(web_ui, ui_handler_);
  }

 private:
  // UI handler class which communicates with test WebUI page.
  TestUIHandler* ui_handler_;
};
}  // namespace

// A test fixture for verifying mojo comminication for WebUI.
class WebUIMojoTest : public WebIntTest {
 protected:
  void SetUp() override {
    WebIntTest::SetUp();
    ui_handler_ = base::MakeUnique<TestUIHandler>();
    web::WebState::CreateParams params(GetBrowserState());
    web_state_ = base::MakeUnique<web::WebStateImpl>(params);
    web_state_->GetNavigationManagerImpl().InitializeSession();
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
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_MessageExchange MessageExchange
#else
#define MAYBE_MessageExchange FLAKY_MessageExchange
#endif
// TODO(crbug.com/720098): Enable this test on device.
TEST_F(WebUIMojoTest, MAYBE_MessageExchange) {
  web_state()->SetWebUsageEnabled(true);
  web_state()->GetView();  // WebState won't load URL without view.
  GURL url(
      url::SchemeHostPort(kTestWebUIScheme, kTestWebUIURLHost, 0).Serialize());
  NavigationManager::WebLoadParams load_params(url);
  web_state()->GetNavigationManager()->LoadURLWithParams(load_params);

  // Wait until |TestUIHandler| receives "fin" message from WebUI page.
  bool fin_received = testing::WaitUntilConditionOrTimeout(kMessageTimeout, ^{
    // Flush any pending tasks. Don't RunUntilIdle() because
    // RunUntilIdle() is incompatible with mojo::SimpleWatcher's
    // automatic arming behavior, which Mojo JS still depends upon.
    //
    // TODO(crbug.com/701875): Introduce the full watcher API to JS and get rid
    // of this hack.
    base::RunLoop loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  loop.QuitClosure());
    loop.Run();
    return test_ui_handler()->IsFinReceived();
  });

  ASSERT_TRUE(fin_received);
  EXPECT_FALSE(web_state()->IsLoading());
  EXPECT_EQ(url, web_state()->GetLastCommittedURL());
}

}  // namespace web
