// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_
#define CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_

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
class RendererScheduler;
class WebSchedulerImpl;

// An implementation of blink::WebUnitTestSupport and BlinkPlatformImpl for
// tests.
class TestBlinkWebUnitTestSupport : public blink::WebUnitTestSupport,
                                    public BlinkPlatformImpl {
 public:
  TestBlinkWebUnitTestSupport();
  virtual ~TestBlinkWebUnitTestSupport();

  virtual blink::WebBlobRegistry* blobRegistry();
  virtual blink::WebClipboard* clipboard();
  virtual blink::WebFileUtilities* fileUtilities();
  virtual blink::WebIDBFactory* idbFactory();
  virtual blink::WebMimeRegistry* mimeRegistry();

  virtual blink::WebURLLoader* createURLLoader();
  virtual blink::WebString userAgent() override;
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

  virtual blink::WebGestureCurve* createFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) override;

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
  virtual bool getBlobItems(const blink::WebString& uuid,
                            blink::WebVector<blink::WebBlobData::Item*>* items);
  virtual blink::WebScheduler* scheduler();
  virtual blink::WebThread* currentThread();

 private:
  MockWebBlobRegistryImpl blob_registry_;
  SimpleWebMimeRegistryImpl mime_registry_;
  scoped_ptr<MockWebClipboardImpl> mock_clipboard_;
  WebFileUtilitiesImpl file_utilities_;
  base::ScopedTempDir file_system_root_;
  scoped_ptr<WebURLLoaderMockFactory> url_loader_factory_;
  cc_blink::WebCompositorSupportImpl compositor_support_;
  scoped_ptr<RendererScheduler> renderer_scheduler_;
  scoped_ptr<WebSchedulerImpl> web_scheduler_;
  scoped_ptr<blink::WebThread> web_thread_;

#if defined(OS_WIN) || defined(OS_MACOSX)
  blink::WebThemeEngine* active_theme_engine_;
#endif
  DISALLOW_COPY_AND_ASSIGN(TestBlinkWebUnitTestSupport);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_
