// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chromeos/extensions/file_browser_handler.h"
#include "chrome/browser/chromeos/extensions/file_browser_resource_throttle.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/value_builder.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::ListBuilder;
using testing::_;

namespace {

// Mock file browser handler event router to be used in the test.
class MockFileBrowserHandlerEventRouter
    : public FileBrowserResourceThrottle::FileBrowserHandlerEventRouter {
 public:
  virtual ~MockFileBrowserHandlerEventRouter() {}

  MOCK_METHOD5(DispatchMimeTypeHandlerEvent,
               void(int render_process_id,
                    int render_process_view,
                    const std::string& mime_type,
                    const GURL& request_url,
                    const std::string& extension_id));
};

// Resource controller to be used in the tests.
class MockResourceController : public content::ResourceController {
 public:
  virtual ~MockResourceController() {}
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD0(CancelAndIgnore, void());
  MOCK_METHOD1(CancelWithError, void(int error_code));
  MOCK_METHOD0(Resume, void());
};

class FileBrowserResourceThrottleTest : public testing::Test {
 public:
  typedef FileBrowserResourceThrottle::FileBrowserHandlerEventRouter
          HandlerEventRouter;

  FileBrowserResourceThrottleTest()
      : test_extension_id_("test_extension_id"),
        test_render_process_id_(2),
        test_render_view_id_(12),
        test_request_url_("http://some_url/file.txt"),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO, &message_loop_) {
  }

  virtual ~FileBrowserResourceThrottleTest() {}

  virtual void SetUp() OVERRIDE {
    (new FileBrowserHandlerParser)->Register();
    // Extension info map must be created before |CreateAndInstallTestExtension|
    // is called (the method will add created extension to the info map).
    extension_info_map_ = new ExtensionInfoMap();
    CreateAndInstallTestExtension();
    InitResourceController();
  }

  virtual void TearDown() OVERRIDE {
    FileBrowserHandler::set_extension_whitelisted_for_test(NULL);
    extensions::ManifestHandler::ClearRegistryForTesting();
  }

 protected:
  // Creates the test extension, and adds it to the |extension_info_map_|.
  // The extension has separate file browser handlers that can handle
  // 'plain/html' and 'plain/text' MIME types.
  void CreateAndInstallTestExtension() {
    // The extension must be white-listed in order to be successfully created.
    FileBrowserHandler::set_extension_whitelisted_for_test(
        &test_extension_id_);

    extension_ =
        ExtensionBuilder()
        .SetManifest(DictionaryBuilder()
                     .Set("name", "file browser handler test")
                     .Set("version", "1.0.0")
                     .Set("manifest_version", 2)
                     .Set("file_browser_handlers", ListBuilder()
                         .Append(DictionaryBuilder()
                             // Handler that handles 'plain/html', among others.
                             .Set("id", "ID_handle_html")
                             .Set("default_title", "Default title")
                             .Set("default_icon", "icon.png")
                             // file_filters_field is mandatory, even though
                             // it's not used in the tests.
                             .Set("file_filters", ListBuilder()
                                 .Append("filesystem:*.html"))
                             .Set("mime_types", ListBuilder()
                                 .Append("random/mime1")
                                 .Append("random/mime2")
                                 .Append("plain/html")))
                         .Append(DictionaryBuilder()
                             // Handler that handles only 'plain/text'.
                             .Set("id", "ID_handle_text")
                             .Set("default_title", "Default title")
                             .Set("default_icon", "icon.png")
                             // file_filters_field is mandatory, even though
                             // it's not used in the tests.
                             .Set("file_filters", ListBuilder()
                                 .Append("filesystem:*.txt"))
                             .Set("mime_types", ListBuilder()
                                 .Append("plain/text")))))
        .SetID(test_extension_id_)
        .Build();

    // 'Install' the extension.
    extension_info_map_->AddExtension(extension_,
                                      base::Time(),  // install time
                                      true);  // enable_incognito
  }

  // Initiates the default mock_resource_controller_ expectations.
  // By the default, resource controller should not be called at all.
  void InitResourceController() {
    EXPECT_CALL(mock_resource_controller_, Cancel()).Times(0);
    EXPECT_CALL(mock_resource_controller_, CancelAndIgnore()).Times(0);
    EXPECT_CALL(mock_resource_controller_, CancelWithError(_)).Times(0);
    EXPECT_CALL(mock_resource_controller_, Resume()).Times(0);
  }

  // Removes the test extension to |extension_info_map_|'s disabled extensions.
  void DisableTestExtension() {
    extension_info_map_->RemoveExtension(test_extension_id_,
                                         extension_misc::UNLOAD_REASON_DISABLE);
  }

  void ReloadTestExtensionIncognitoDisabled() {
    extension_info_map_->RemoveExtension(
        test_extension_id_, extension_misc::UNLOAD_REASON_UNINSTALL);
    extension_info_map_->AddExtension(extension_,
                                      base::Time(),  // install_time
                                      false);  // enable incognito
  }

  // Creates the resource throttle that should be tested.
  // It's setup with |mock_event_router| passed to the method and
  // |mock_resource_throttle_|.
  // |mock_event_router|'s expectations must be set before calling this method.
  scoped_ptr<FileBrowserResourceThrottle> CreateThrottleToTest(
      bool is_incognito,
      scoped_ptr<MockFileBrowserHandlerEventRouter> mock_event_router,
      const std::string& mime_type) {
    scoped_ptr<HandlerEventRouter> event_router(mock_event_router.release());
    scoped_ptr<FileBrowserResourceThrottle> test_throttle(
        FileBrowserResourceThrottle::CreateForTest(test_render_process_id_,
                                                   test_render_view_id_,
                                                   mime_type,
                                                   test_request_url_,
                                                   is_incognito,
                                                   extension_info_map_.get(),
                                                   event_router.Pass()));

    test_throttle->set_controller_for_testing(&mock_resource_controller_);

    return test_throttle.Pass();
  }

  // The test extension's id.
  std::string test_extension_id_;
  // The test extension.
  scoped_refptr<const Extension> extension_;

  // The extension info map used in the test. The extensions are considered
  // installed/disabled depending on the extension_info_map_ state.
  scoped_refptr<ExtensionInfoMap> extension_info_map_;

  MockResourceController mock_resource_controller_;

  // Parameters used to create the test resource throttle. Unlike, mime_type
  // these can be selected at random.
  int test_render_process_id_;
  int test_render_view_id_;
  GURL test_request_url_;

 private:
  // ExtensionInfoMap needs IO thread.
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

// Tests that the request gets canceled (mock_resource_controller_.Cancel() is
// called) and the event_router is invoked when a white-listed extension has a
// file browser handler that can handle the request's MIME type.
TEST_F(FileBrowserResourceThrottleTest, HandlerWhiteListed) {
  EXPECT_CALL(mock_resource_controller_, CancelAndIgnore()).Times(1);

  scoped_ptr<MockFileBrowserHandlerEventRouter> mock_event_router(
      new MockFileBrowserHandlerEventRouter());
  EXPECT_CALL(*mock_event_router,
      DispatchMimeTypeHandlerEvent(test_render_process_id_,
                                   test_render_view_id_,
                                   "plain/html",
                                   test_request_url_,
                                   test_extension_id_))
      .Times(1);

  scoped_ptr<FileBrowserResourceThrottle> throttle(
      CreateThrottleToTest(false, mock_event_router.Pass(), "plain/html"));

  bool defer = false;
  throttle->WillProcessResponse(&defer);
  EXPECT_FALSE(defer);
}

// Tests that the request gets canceled (mock_resource_controller_.Cancel() is
// called) and the event_router is invoked when a white-listed extension has a
// file browser handler that can handle the request's MIME type, even when the
// file browser handler is not first in the extension's file browser handler
// list.
TEST_F(FileBrowserResourceThrottleTest, SecondHandlerWhiteListed) {
  EXPECT_CALL(mock_resource_controller_, CancelAndIgnore()).Times(1);

  scoped_ptr<MockFileBrowserHandlerEventRouter> mock_event_router(
      new MockFileBrowserHandlerEventRouter());
  EXPECT_CALL(*mock_event_router,
      DispatchMimeTypeHandlerEvent(test_render_process_id_,
                                   test_render_view_id_,
                                   "plain/text",
                                   test_request_url_,
                                   test_extension_id_))
      .Times(1);

  scoped_ptr<FileBrowserResourceThrottle> throttle(
      CreateThrottleToTest(false, mock_event_router.Pass(), "plain/text"));

  bool defer = false;
  throttle->WillProcessResponse(&defer);
  EXPECT_FALSE(defer);
}

// Tests that the request is not canceled and the event router is not invoked
// if there is no file browser handlers registered for the request's MIME type.
TEST_F(FileBrowserResourceThrottleTest, NoWhiteListedHandler) {
  scoped_ptr<MockFileBrowserHandlerEventRouter> mock_event_router(
      new MockFileBrowserHandlerEventRouter());
  EXPECT_CALL(*mock_event_router, DispatchMimeTypeHandlerEvent(_, _, _, _, _))
      .Times(0);

  scoped_ptr<FileBrowserResourceThrottle> throttle(
      CreateThrottleToTest(false, mock_event_router.Pass(),
                           "random_mime_type"));

  bool defer = false;
  throttle->WillProcessResponse(&defer);
  EXPECT_FALSE(defer);
}

// Tests that the request is not canceled and the event router is not invoked
// if there is an extension with the file browser handler that can handle the
// request's MIME type, but the extension is disabled.
TEST_F(FileBrowserResourceThrottleTest, HandlerWhiteListedAndDisabled) {
  DisableTestExtension();

  scoped_ptr<MockFileBrowserHandlerEventRouter> mock_event_router(
      new MockFileBrowserHandlerEventRouter());
  EXPECT_CALL(*mock_event_router, DispatchMimeTypeHandlerEvent(_, _, _, _, _))
      .Times(0);

  scoped_ptr<FileBrowserResourceThrottle> throttle(
      CreateThrottleToTest(false, mock_event_router.Pass(), "plain/text"));

  bool defer = false;
  throttle->WillProcessResponse(&defer);
  EXPECT_FALSE(defer);
}

// Tests that the request is not canceled and the event router is not invoked
// in incognito mode if the extension that handles the request's MIME type is
// not incognito enabled.
TEST_F(FileBrowserResourceThrottleTest, IncognitoExtensionNotEnabled) {
  ReloadTestExtensionIncognitoDisabled();

  scoped_ptr<MockFileBrowserHandlerEventRouter> mock_event_router(
      new MockFileBrowserHandlerEventRouter());
  EXPECT_CALL(*mock_event_router, DispatchMimeTypeHandlerEvent(_, _, _, _, _))
      .Times(0);

  scoped_ptr<FileBrowserResourceThrottle> throttle(
      CreateThrottleToTest(true, mock_event_router.Pass(), "plain/text"));

  bool defer = false;
  throttle->WillProcessResponse(&defer);
  EXPECT_FALSE(defer);
}

// Tests that the request gets canceled (mock_resource_controller_.Cancel() is
// called) and the event_router is invoked in incognito when a white-listed
// extension that handles request's MIME type is incognito enabled.
TEST_F(FileBrowserResourceThrottleTest, IncognitoExtensionEnabled) {
  EXPECT_CALL(mock_resource_controller_, CancelAndIgnore()).Times(1);

  scoped_ptr<MockFileBrowserHandlerEventRouter> mock_event_router(
      new MockFileBrowserHandlerEventRouter());
  EXPECT_CALL(*mock_event_router,
      DispatchMimeTypeHandlerEvent(test_render_process_id_,
                                   test_render_view_id_,
                                   "plain/html",
                                   test_request_url_,
                                   test_extension_id_))
      .Times(1);

  scoped_ptr<FileBrowserResourceThrottle> throttle(
      CreateThrottleToTest(true, mock_event_router.Pass(), "plain/html"));

  bool defer = false;
  throttle->WillProcessResponse(&defer);
  EXPECT_FALSE(defer);
}

}  // namespace
