// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_RENDER_TEST_H_
#define HEADLESS_TEST_HEADLESS_RENDER_TEST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/util/testing/test_in_memory_protocol_handler.h"
#include "headless/test/headless_browser_test.h"
#include "net/test/embedded_test_server/http_request.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "url/gurl.h"

namespace headless {
class HeadlessDevToolsClient;
class VirtualTimeController;
namespace dom_snapshot {
class GetSnapshotResult;
}  // namespace dom_snapshot

// Base class for tests that render a particular page and verify the output.
class HeadlessRenderTest : public HeadlessAsyncDevTooledBrowserTest,
                           public HeadlessBrowserContext::Observer,
                           public page::ExperimentalObserver,
                           public TestInMemoryProtocolHandler::RequestDeferrer {
 public:
  typedef std::pair<std::string, page::FrameScheduledNavigationReason> Redirect;

  void RunDevTooledTest() override;

 protected:
  // Automatically waits in destructor until callback is called.
  class Sync {
   public:
    Sync() {}
    ~Sync() {
      base::MessageLoop::ScopedNestableTaskAllower nest_loop(
          base::MessageLoop::current());
      run_loop.Run();
    }
    operator base::Closure() { return run_loop.QuitClosure(); }

   private:
    base::RunLoop run_loop;
    DISALLOW_COPY_AND_ASSIGN(Sync);
  };

  HeadlessRenderTest();
  ~HeadlessRenderTest() override;

  // Marks that the test case reached the final conclusion.
  void SetTestCompleted() { state_ = FINISHED; }

  // The protocol handler used to respond to requests.
  TestInMemoryProtocolHandler* GetProtocolHandler() { return http_handler_; }

  // Do necessary preparations and return a URL to render.
  virtual GURL GetPageUrl(HeadlessDevToolsClient* client) = 0;

  // Check if the DOM snapshot is as expected.
  virtual void VerifyDom(dom_snapshot::GetSnapshotResult* dom_snapshot) = 0;

  // Called when all steps needed to load and present page are done.
  virtual void OnPageRenderCompleted();

  // Called if page rendering wasn't completed within reasonable time.
  virtual void OnTimeout();

  // Override to set specific options for requests.
  virtual void OverrideWebPreferences(WebPreferences* preferences);

  // Setting up the browsertest.
  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override;
  void PostRunAsynchronousTest() override;
  ProtocolHandlerMap GetProtocolHandlers() override;

  // HeadlessBrowserContext::Observer
  void UrlRequestFailed(net::URLRequest* request,
                        int net_error,
                        bool canceled_by_devtools) override;

  // page::ExperimentalObserver implementation:
  void OnLoadEventFired(const page::LoadEventFiredParams& params) override;
  void OnFrameStartedLoading(
      const page::FrameStartedLoadingParams& params) override;
  void OnFrameScheduledNavigation(
      const page::FrameScheduledNavigationParams& params) override;
  void OnFrameClearedScheduledNavigation(
      const page::FrameClearedScheduledNavigationParams& params) override;
  void OnFrameNavigated(const page::FrameNavigatedParams& params) override;

  // TestInMemoryProtocolHandler::RequestDeferrer
  void OnRequest(const GURL& url, base::Closure complete_request) override;

  std::map<std::string, std::vector<Redirect>> confirmed_frame_redirects_;
  std::map<std::string, Redirect> unconfirmed_frame_redirects_;
  std::map<std::string, std::vector<std::unique_ptr<page::Frame>>> frames_;
  std::string main_frame_;

 private:
  void HandleVirtualTimeExhausted();
  void OnGetDomSnapshotDone(
      std::unique_ptr<dom_snapshot::GetSnapshotResult> result);
  void HandleTimeout();

  enum State {
    INIT,       // Setting up the client, no navigation performed yet.
    STARTING,   // Navigation request issued but URL not being loaded yet.
    LOADING,    // URL was requested but resources are not fully loaded yet.
    RENDERING,  // Main resources were loaded but page is still being rendered.
    DONE,       // Page considered to be fully rendered.
    FINISHED,   // Test has finished.
  };
  State state_ = INIT;

  std::unique_ptr<VirtualTimeController> virtual_time_controller_;
  TestInMemoryProtocolHandler* http_handler_;  // NOT OWNED

  base::WeakPtrFactory<HeadlessRenderTest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessRenderTest);
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_RENDER_TEST_H_
