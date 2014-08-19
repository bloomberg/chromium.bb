// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_WEBKIT_PLATFORM_SUPPORT_H_
#define CONTENT_TEST_TEST_WEBKIT_PLATFORM_SUPPORT_H_

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "content/child/blink_platform_impl.h"
#include "content/child/simple_webmimeregistry_impl.h"
#include "content/child/webfileutilities_impl.h"
#include "content/test/mock_webblob_registry_impl.h"
#include "content/test/mock_webclipboard_impl.h"
#include "content/test/weburl_loader_mock_factory.h"
#include "third_party/WebKit/public/platform/WebUnitTestSupport.h"

namespace base {
class StatsTable;
}

namespace blink {
class WebLayerTreeView;
}

namespace content {

// An implementation of WebKitPlatformSupport for tests.
class TestWebKitPlatformSupport
    : public blink::WebUnitTestSupport,
      public BlinkPlatformImpl {
 public:
  TestWebKitPlatformSupport();
  virtual ~TestWebKitPlatformSupport();

  virtual blink::WebBlobRegistry* blobRegistry();
  virtual blink::WebClipboard* clipboard();
  virtual blink::WebFileUtilities* fileUtilities();
  virtual blink::WebIDBFactory* idbFactory();
  virtual blink::WebMimeRegistry* mimeRegistry();

  virtual blink::WebURLLoader* createURLLoader();
  virtual blink::WebString userAgent() OVERRIDE;
  virtual blink::WebData loadResource(const char* name);
  virtual blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name);
  virtual blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name,
      const blink::WebString& value);
  virtual blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name,
      const blink::WebString& value1,
      const blink::WebString& value2);
  virtual blink::WebString defaultLocale();

#if defined(OS_WIN) || defined(OS_MACOSX)
  void SetThemeEngine(blink::WebThemeEngine* engine);
  virtual blink::WebThemeEngine* themeEngine();
#endif

  virtual blink::WebCompositorSupport* compositorSupport();

  WebURLLoaderMockFactory* url_loader_factory() {
    return url_loader_factory_.get();
  }

  const base::FilePath& file_system_root() const {
    return file_system_root_.path();
  }

  virtual blink::WebGestureCurve* createFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) OVERRIDE;

  virtual blink::WebUnitTestSupport* unitTestSupport();

  // WebUnitTestSupport implementation
  virtual void registerMockedURL(const blink::WebURL& url,
                                 const blink::WebURLResponse& response,
                                 const blink::WebString& filePath);
  virtual void registerMockedErrorURL(const blink::WebURL& url,
                                      const blink::WebURLResponse& response,
                                      const blink::WebURLError& error);
  virtual void unregisterMockedURL(const blink::WebURL& url);
  virtual void unregisterAllMockedURLs();
  virtual void serveAsynchronousMockedRequests();
  virtual blink::WebString webKitRootDir();
  virtual blink::WebLayerTreeView* createLayerTreeViewForTesting();
  virtual blink::WebData readFromFile(const blink::WebString& path);

 private:
  MockWebBlobRegistryImpl blob_registry_;
  SimpleWebMimeRegistryImpl mime_registry_;
  scoped_ptr<MockWebClipboardImpl> mock_clipboard_;
  WebFileUtilitiesImpl file_utilities_;
  base::ScopedTempDir file_system_root_;
  scoped_ptr<WebURLLoaderMockFactory> url_loader_factory_;
  cc_blink::WebCompositorSupportImpl compositor_support_;
  scoped_ptr<base::StatsTable> stats_table_;

#if defined(OS_WIN) || defined(OS_MACOSX)
  blink::WebThemeEngine* active_theme_engine_;
#endif
  DISALLOW_COPY_AND_ASSIGN(TestWebKitPlatformSupport);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_WEBKIT_PLATFORM_SUPPORT_H_
