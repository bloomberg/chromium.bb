// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_dom_feature_extractor.h"

#include <string.h>  // for memcpy()
#include <map>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/string_util.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/sandbox_init_wrapper.h"
#include "chrome/renderer/mock_render_process.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/render_view_visitor.h"
#include "chrome/renderer/renderer_main_platform_delegate.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"

using ::testing::ContainerEq;

namespace safe_browsing {

class PhishingDOMFeatureExtractorTest : public ::testing::Test,
                                        public IPC::Channel::Listener,
                                        public RenderViewVisitor {
 public:
  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message) {
    IPC_BEGIN_MESSAGE_MAP(PhishingDOMFeatureExtractorTest, message)
      IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnRenderViewReady)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DidStopLoading, OnDidStopLoading)
      IPC_MESSAGE_HANDLER(ViewHostMsg_RequestResource, OnRequestResource)
    IPC_END_MESSAGE_MAP()
  }

  // RenderViewVisitor implementation.
  virtual bool Visit(RenderView* render_view) {
    view_ = render_view;
    return false;
  }

 protected:
  virtual void SetUp() {
    // Set up the renderer.  This code is largely adapted from
    // render_view_test.cc and renderer_main.cc.  Note that we use a
    // MockRenderProcess (because we don't need to use IPC for painting),
    // but we use a real RenderThread so that we can use the ResourceDispatcher
    // to fetch network resources.  These are then served canned content
    // in OnRequestResource().
    sandbox_init_wrapper_.reset(new SandboxInitWrapper);
    command_line_.reset(new CommandLine(CommandLine::ARGUMENTS_ONLY));
    params_.reset(new MainFunctionParams(*command_line_,
                                         *sandbox_init_wrapper_, NULL));
    platform_.reset(new RendererMainPlatformDelegate(*params_));
    platform_->PlatformInitialize();

    // We use a new IPC channel name for each test that runs.
    // This is necessary because the renderer-side IPC channel is not
    // shut down when the RenderThread goes away, so attempting to reuse
    // the channel name gives an error (see ChildThread::~ChildThread()).
    std::string thread_name = StringPrintf(
        "phishing_dom_feature_Extractor_unittest.%d",
        next_thread_id_++);
    channel_.reset(new IPC::Channel(thread_name,
                                    IPC::Channel::MODE_SERVER, this));
    ASSERT_TRUE(channel_->Connect());

    webkit_glue::SetJavaScriptFlags("--expose-gc");
    mock_process_.reset(new MockRenderProcess);
    render_thread_ = new RenderThread(thread_name);
    mock_process_->set_main_thread(render_thread_);

    // Tell the renderer to create a view, then wait until it's ready.
    // We can't call View::Create() directly here or else we won't get
    // RenderProcess's lazy initialization of WebKit.
    view_ = NULL;
    ViewMsg_New_Params params;
    params.parent_window = 0;
    params.view_id = kViewId;
    params.session_storage_namespace_id = kInvalidSessionStorageNamespaceId;
    ASSERT_TRUE(channel_->Send(new ViewMsg_New(params)));
    msg_loop_.Run();

    extractor_.reset(new PhishingDOMFeatureExtractor(view_));
  }

  virtual void TearDown() {
    // Try very hard to collect garbage before shutting down.
    GetMainFrame()->collectGarbage();
    GetMainFrame()->collectGarbage();

    ASSERT_TRUE(channel_->Send(new ViewMsg_Close(kViewId)));
    do {
      msg_loop_.RunAllPending();
      view_ = NULL;
      RenderView::ForEach(this);
    } while (view_);

    mock_process_.reset();
    msg_loop_.RunAllPending();
    platform_->PlatformUninitialize();
    platform_.reset();
    command_line_.reset();
    sandbox_init_wrapper_.reset();
  }

  // Returns the main WebFrame for our RenderView.
  WebKit::WebFrame* GetMainFrame() {
    return view_->webview()->mainFrame();
  }

  // Loads |url| into the RenderView, waiting for the load to finish.
  void LoadURL(const std::string& url) {
    GetMainFrame()->loadRequest(WebKit::WebURLRequest(GURL(url)));
    msg_loop_.Run();
  }

  // Runs the DOMFeatureExtractor on the RenderView, waiting for the
  // completion callback.  Returns the success boolean from the callback.
  bool ExtractFeatures(FeatureMap* features) {
    success_ = false;
    extractor_->ExtractFeatures(
        features,
        NewCallback(this, &PhishingDOMFeatureExtractorTest::ExtractionDone));
    msg_loop_.Run();
    return success_;
  }

  // Completion callback for feature extraction.
  void ExtractionDone(bool success) {
    success_ = success;
    msg_loop_.Quit();
  }

  // IPC message handlers below

  // Notification that page load has finished.  Exit the message loop
  // so that the test can continue.
  void OnDidStopLoading() {
    msg_loop_.Quit();
  }

  // Notification that the renderer wants to load a resource.
  // If the requested url is in responses_, we send the renderer a 200
  // and the supplied content, otherwise we send it a 404 error.
  void OnRequestResource(const IPC::Message& message,
                         int request_id,
                         const ViewHostMsg_Resource_Request& request_data) {
    std::string headers, body;
    std::map<std::string, std::string>::const_iterator it =
        responses_.find(request_data.url.spec());
    if (it == responses_.end()) {
      headers = "HTTP/1.1 404 Not Found\0Content-Type:text/html\0\0";
      body = "content not found";
    } else {
      headers = "HTTP/1.1 200 OK\0Content-Type:text/html\0\0";
      body = it->second;
    }

    ResourceResponseHead response_head;
    response_head.headers = new net::HttpResponseHeaders(headers);
    response_head.mime_type = "text/html";
    ASSERT_TRUE(channel_->Send(new ViewMsg_Resource_ReceivedResponse(
        message.routing_id(), request_id, response_head)));

    base::SharedMemory shared_memory;
    ASSERT_TRUE(shared_memory.Create(std::wstring(), false,
                                     false, body.size()));
    ASSERT_TRUE(shared_memory.Map(body.size()));
    memcpy(shared_memory.memory(), body.data(), body.size());

    base::SharedMemoryHandle handle;
    ASSERT_TRUE(shared_memory.GiveToProcess(base::Process::Current().handle(),
                                            &handle));
    ASSERT_TRUE(channel_->Send(new ViewMsg_Resource_DataReceived(
        message.routing_id(), request_id, handle, body.size())));

    ASSERT_TRUE(channel_->Send(new ViewMsg_Resource_RequestComplete(
        message.routing_id(),
        request_id,
        URLRequestStatus(),
        std::string())));
  }

  // Notification that the render view we've created is ready to use.
  void OnRenderViewReady() {
    // Grab a pointer to the new view using RenderViewVisitor.
    ASSERT_TRUE(!view_);
    RenderView::ForEach(this);
    ASSERT_TRUE(view_);
    msg_loop_.Quit();
  }

  static int next_thread_id_;  // incrementing counter for thread ids
  static const int32 kViewId = 5;  // arbitrary id for our testing view

  MessageLoopForIO msg_loop_;
  // channel that the renderer uses to talk to the browser.
  // For this test, we will handle the browser end of the channel.
  scoped_ptr<IPC::Channel> channel_;
  RenderThread* render_thread_;  // owned by mock_process_
  scoped_ptr<MockRenderProcess> mock_process_;
  RenderView* view_;  // not owned, deletes itself on close
  scoped_ptr<RendererMainPlatformDelegate> platform_;
  scoped_ptr<MainFunctionParams> params_;
  scoped_ptr<CommandLine> command_line_;
  scoped_ptr<SandboxInitWrapper> sandbox_init_wrapper_;

  scoped_ptr<PhishingDOMFeatureExtractor> extractor_;
  // Map of URL -> response body for network requests from the renderer.
  // Any URLs not in this map are served a 404 error.
  std::map<std::string, std::string> responses_;
  bool success_;  // holds the success value from ExtractFeatures
};

int PhishingDOMFeatureExtractorTest::next_thread_id_ = 0;

TEST_F(PhishingDOMFeatureExtractorTest, FormFeatures) {
  responses_["http://host.com/"] =
      "<html><head><body>"
      "<form action=\"query\"><input type=text><input type=checkbox></form>"
      "<form action=\"http://cgi.host.com/submit\"></form>"
      "<form action=\"http://other.com/\"></form>"
      "<form action=\"query\"></form>"
      "<form></form></body></html>";

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);
  expected_features.AddBooleanFeature(features::kPageHasCheckInputs);

  FeatureMap features;
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  responses_["http://host.com/"] =
      "<html><head><body>"
      "<input type=\"radio\"><input type=password></body></html>";

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasRadioInputs);
  expected_features.AddBooleanFeature(features::kPageHasPswdInputs);

  features.Clear();
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  responses_["http://host.com/"] =
      "<html><head><body><input></body></html>";

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);

  features.Clear();
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  responses_["http://host.com/"] =
      "<html><head><body><input type=\"invalid\"></body></html>";

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);

  features.Clear();
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
}

TEST_F(PhishingDOMFeatureExtractorTest, LinkFeatures) {
  responses_["http://www.host.com/"] =
      "<html><head><body>"
      "<a href=\"http://www2.host.com/abc\">link</a>"
      "<a name=page_anchor></a>"
      "<a href=\"http://www.chromium.org/\">chromium</a>"
      "</body></html";

  FeatureMap expected_features;
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.5);
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.0);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("chromium.org"));

  FeatureMap features;
  LoadURL("http://www.host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  responses_.clear();
  responses_["https://www.host.com/"] =
      "<html><head><body>"
      "<a href=\"login\">this is secure</a>"
      "<a href=\"http://host.com\">not secure</a>"
      "<a href=\"https://www2.host.com/login\">also secure</a>"
      "<a href=\"http://chromium.org/\">also not secure</a>"
      "</body></html>";

  expected_features.Clear();
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.25);
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.5);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("chromium.org"));

  features.Clear();
  LoadURL("https://www.host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
}

TEST_F(PhishingDOMFeatureExtractorTest, ScriptAndImageFeatures) {
  responses_["http://host.com/"] =
      "<html><head><script></script><script></script></head></html>";

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);

  FeatureMap features;
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  responses_["http://host.com/"] =
      "<html><head><script></script><script></script><script></script>"
      "<script></script><script></script><script></script><script></script>"
      "</head><body><img src=\"blah.gif\">"
      "<img src=\"http://host2.com/blah.gif\"></body></html>";

  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTSix);
  expected_features.AddRealFeature(features::kPageImgOtherDomainFreq, 0.5);

  features.Clear();
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
}

TEST_F(PhishingDOMFeatureExtractorTest, SubFrames) {
  // Test that features are aggregated across all frames.
  responses_["http://host.com/"] =
      "<html><body><input type=text><a href=\"info.html\">link</a>"
      "<iframe src=\"http://host2.com/\"></iframe>"
      "<iframe src=\"http://host3.com/\"></iframe>"
      "</body></html>";

  responses_["http://host2.com/"] =
      "<html><head><script></script><body>"
      "<form action=\"http://host4.com/\"><input type=checkbox></form>"
      "<form action=\"http://host2.com/submit\"></form>"
      "<a href=\"http://www.host2.com/home\">link</a>"
      "<iframe src=\"nested.html\"></iframe>"
      "<body></html>";

  responses_["http://host2.com/nested.html"] =
      "<html><body><input type=password>"
      "<a href=\"https://host4.com/\">link</a>"
      "<a href=\"relative\">another</a>"
      "</body></html>";

  responses_["http://host3.com/"] =
      "<html><head><script></script><body>"
      "<img src=\"http://host.com/123.png\">"
      "</body></html>";

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageHasForms);
  // Form action domains are compared to the URL of the document they're in,
  // not the URL of the toplevel page.  So http://host2.com/ has two form
  // actions, one of which is external.
  expected_features.AddRealFeature(features::kPageActionOtherDomainFreq, 0.5);
  expected_features.AddBooleanFeature(features::kPageHasTextInputs);
  expected_features.AddBooleanFeature(features::kPageHasPswdInputs);
  expected_features.AddBooleanFeature(features::kPageHasCheckInputs);
  expected_features.AddRealFeature(features::kPageExternalLinksFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageLinkDomain +
                                      std::string("host4.com"));
  expected_features.AddRealFeature(features::kPageSecureLinksFreq, 0.25);
  expected_features.AddBooleanFeature(features::kPageNumScriptTagsGTOne);
  expected_features.AddRealFeature(features::kPageImgOtherDomainFreq, 1.0);

  FeatureMap features;
  LoadURL("http://host.com/");
  ASSERT_TRUE(ExtractFeatures(&features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
}

// TODO(bryner): Test extraction with multiple passes, including the case where
// the node we stopped on is removed from the document.

}  // namespace safe_browsing
