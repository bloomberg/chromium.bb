// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_webkit_platform_support.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "content/test/mock_webclipboard_impl.h"
#include "content/test/web_gesture_curve_mock.h"
#include "content/test/web_layer_tree_view_impl_for_testing.h"
#include "content/test/weburl_loader_mock_factory.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "v8/include/v8.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/renderer/compositor_bindings/web_compositor_support_impl.h"

#if defined(OS_WIN)
#include "third_party/WebKit/public/platform/win/WebThemeEngine.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using webkit::WebLayerTreeViewImplForTesting;

namespace content {

TestWebKitPlatformSupport::TestWebKitPlatformSupport() {
  url_loader_factory_.reset(new WebURLLoaderMockFactory());
  mock_clipboard_.reset(new MockWebClipboardImpl());
  v8::V8::SetCounterFunction(base::StatsTable::FindLocation);


#if defined(OS_WIN)
  // Ensure we pick up the default theme engine.
  SetThemeEngine(NULL);
#endif

  // Test shell always exposes the GC.
  webkit_glue::SetJavaScriptFlags(" --expose-gc");
}

TestWebKitPlatformSupport::~TestWebKitPlatformSupport() {
  url_loader_factory_.reset();
  mock_clipboard_.reset();
  WebKit::shutdown();
}

WebKit::WebMimeRegistry* TestWebKitPlatformSupport::mimeRegistry() {
  return &mime_registry_;
}

WebKit::WebClipboard* TestWebKitPlatformSupport::clipboard() {
  // Mock out clipboard calls so that tests don't mess
  // with each other's copies/pastes when running in parallel.
  return mock_clipboard_.get();
}

WebKit::WebURLLoader* TestWebKitPlatformSupport::createURLLoader() {
  return url_loader_factory_->CreateURLLoader(
      webkit_glue::WebKitPlatformSupportImpl::createURLLoader());
}

WebKit::WebString TestWebKitPlatformSupport::queryLocalizedString(
    WebKit::WebLocalizedString::Name name) {
  // Returns placeholder strings to check if they are correctly localized.
  switch (name) {
    case WebKit::WebLocalizedString::OtherDateLabel:
      return ASCIIToUTF16("<<OtherDateLabel>>");
    case WebKit::WebLocalizedString::OtherMonthLabel:
      return ASCIIToUTF16("<<OtherMonthLabel>>");
    case WebKit::WebLocalizedString::OtherTimeLabel:
      return ASCIIToUTF16("<<OtherTimeLabel>>");
    case WebKit::WebLocalizedString::OtherWeekLabel:
      return ASCIIToUTF16("<<OtherWeekLabel>>");
    case WebKit::WebLocalizedString::CalendarClear:
      return ASCIIToUTF16("<<CalendarClear>>");
    case WebKit::WebLocalizedString::CalendarToday:
      return ASCIIToUTF16("<<CalendarToday>>");
    case WebKit::WebLocalizedString::ThisMonthButtonLabel:
      return ASCIIToUTF16("<<ThisMonthLabel>>");
    case WebKit::WebLocalizedString::ThisWeekButtonLabel:
      return ASCIIToUTF16("<<ThisWeekLabel>>");
    case WebKit::WebLocalizedString::WeekFormatTemplate:
      return ASCIIToUTF16("Week $2, $1");
    default:
      return WebKitPlatformSupportImpl::queryLocalizedString(name);
  }
}

WebKit::WebString TestWebKitPlatformSupport::queryLocalizedString(
    WebKit::WebLocalizedString::Name name,
    const WebKit::WebString& value) {
  if (name == WebKit::WebLocalizedString::ValidationRangeUnderflow)
    return ASCIIToUTF16("range underflow");
  if (name == WebKit::WebLocalizedString::ValidationRangeOverflow)
    return ASCIIToUTF16("range overflow");
  return WebKitPlatformSupportImpl::queryLocalizedString(name, value);
}

WebKit::WebString TestWebKitPlatformSupport::queryLocalizedString(
    WebKit::WebLocalizedString::Name name,
    const WebKit::WebString& value1,
    const WebKit::WebString& value2) {
  if (name == WebKit::WebLocalizedString::ValidationTooLong)
    return ASCIIToUTF16("too long");
  if (name == WebKit::WebLocalizedString::ValidationStepMismatch)
    return ASCIIToUTF16("step mismatch");
  return WebKitPlatformSupportImpl::queryLocalizedString(name, value1, value2);
}

WebKit::WebString TestWebKitPlatformSupport::defaultLocale() {
  return ASCIIToUTF16("en-US");
}

#if defined(OS_WIN) || defined(OS_MACOSX)
void TestWebKitPlatformSupport::SetThemeEngine(WebKit::WebThemeEngine* engine) {
  active_theme_engine_ = engine ?
      engine : WebKitPlatformSupportChildImpl::themeEngine();
}

WebKit::WebThemeEngine* TestWebKitPlatformSupport::themeEngine() {
  return active_theme_engine_;
}
#endif

WebKit::WebCompositorSupport* TestWebKitPlatformSupport::compositorSupport() {
  return &compositor_support_;
}

base::string16 TestWebKitPlatformSupport::GetLocalizedString(int message_id) {
  return base::string16();
}

base::StringPiece TestWebKitPlatformSupport::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return base::StringPiece();
}

webkit_glue::ResourceLoaderBridge*
TestWebKitPlatformSupport::CreateResourceLoader(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info) {
  NOTREACHED();
  return NULL;
}

webkit_glue::WebSocketStreamHandleBridge*
TestWebKitPlatformSupport::CreateWebSocketStreamBridge(
    WebKit::WebSocketStreamHandle* handle,
    webkit_glue::WebSocketStreamHandleDelegate* delegate) {
  NOTREACHED();
  return NULL;
}

WebKit::WebGestureCurve* TestWebKitPlatformSupport::createFlingAnimationCurve(
    int device_source,
    const WebKit::WebFloatPoint& velocity,
    const WebKit::WebSize& cumulative_scroll) {
  // Caller will retain and release.
  return new WebGestureCurveMock(velocity, cumulative_scroll);
}

WebKit::WebUnitTestSupport* TestWebKitPlatformSupport::unitTestSupport() {
  return this;
}

void TestWebKitPlatformSupport::registerMockedURL(
    const WebKit::WebURL& url,
    const WebKit::WebURLResponse& response,
    const WebKit::WebString& file_path) {
  url_loader_factory_->RegisterURL(url, response, file_path);
}

void TestWebKitPlatformSupport::registerMockedErrorURL(
    const WebKit::WebURL& url,
    const WebKit::WebURLResponse& response,
    const WebKit::WebURLError& error) {
  url_loader_factory_->RegisterErrorURL(url, response, error);
}

void TestWebKitPlatformSupport::unregisterMockedURL(const WebKit::WebURL& url) {
  url_loader_factory_->UnregisterURL(url);
}

void TestWebKitPlatformSupport::unregisterAllMockedURLs() {
  url_loader_factory_->UnregisterAllURLs();
}

void TestWebKitPlatformSupport::serveAsynchronousMockedRequests() {
  url_loader_factory_->ServeAsynchronousRequests();
}

WebKit::WebString TestWebKitPlatformSupport::webKitRootDir() {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.Append(FILE_PATH_LITERAL("third_party/WebKit"));
  path = base::MakeAbsoluteFilePath(path);
  CHECK(!path.empty());
  std::string path_ascii = path.MaybeAsASCII();
  CHECK(!path_ascii.empty());
  return WebKit::WebString::fromUTF8(path_ascii.c_str());
}

WebKit::WebLayerTreeView*
TestWebKitPlatformSupport::createLayerTreeViewForTesting() {
  scoped_ptr<WebLayerTreeViewImplForTesting> view(
      new WebLayerTreeViewImplForTesting());

  if (!view->Initialize())
    return NULL;
  return view.release();
}

WebKit::WebLayerTreeView*
TestWebKitPlatformSupport::createLayerTreeViewForTesting(TestViewType type) {
  DCHECK_EQ(TestViewTypeUnitTest, type);
  return createLayerTreeViewForTesting();
}

WebKit::WebData TestWebKitPlatformSupport::readFromFile(
    const WebKit::WebString& path) {
  base::FilePath file_path = base::FilePath::FromUTF16Unsafe(path);

  std::string buffer;
  base::ReadFileToString(file_path, &buffer);

  return WebKit::WebData(buffer.data(), buffer.size());
}

}  // namespace content
