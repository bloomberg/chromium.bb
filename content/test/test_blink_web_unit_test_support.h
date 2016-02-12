// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_
#define CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "build/build_config.h"
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

namespace scheduler {
class RendererScheduler;
}

namespace content {

// An implementation of blink::WebUnitTestSupport and BlinkPlatformImpl for
// tests.
class TestBlinkWebUnitTestSupport : public blink::WebUnitTestSupport,
                                    public BlinkPlatformImpl {
 public:
  TestBlinkWebUnitTestSupport();
  ~TestBlinkWebUnitTestSupport() override;

  blink::WebBlobRegistry* blobRegistry() override;
  blink::WebClipboard* clipboard() override;
  blink::WebFileUtilities* fileUtilities() override;
  blink::WebIDBFactory* idbFactory() override;
  blink::WebMimeRegistry* mimeRegistry() override;

  blink::WebURLLoader* createURLLoader() override;
  blink::WebString userAgent() override;
  blink::WebData loadResource(const char* name) override;
  blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name) override;
  blink::WebString queryLocalizedString(blink::WebLocalizedString::Name name,
                                        const blink::WebString& value) override;
  blink::WebString queryLocalizedString(
      blink::WebLocalizedString::Name name,
      const blink::WebString& value1,
      const blink::WebString& value2) override;
  blink::WebString defaultLocale() override;

#if defined(OS_WIN) || defined(OS_MACOSX)
  void SetThemeEngine(blink::WebThemeEngine* engine);
  blink::WebThemeEngine* themeEngine() override;
#endif

  blink::WebCompositorSupport* compositorSupport() override;

  blink::WebGestureCurve* createFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) override;

  blink::WebUnitTestSupport* unitTestSupport() override;

  // WebUnitTestSupport implementation
  void registerMockedURL(const blink::WebURL& url,
                         const blink::WebURLResponse& response,
                         const blink::WebString& filePath) override;
  void registerMockedErrorURL(const blink::WebURL& url,
                              const blink::WebURLResponse& response,
                              const blink::WebURLError& error) override;
  void unregisterMockedURL(const blink::WebURL& url) override;
  void unregisterAllMockedURLs() override;
  void serveAsynchronousMockedRequests() override;
  void setLoaderDelegate(blink::WebURLLoaderTestDelegate* delegate) override;
  blink::WebLayerTreeView* createLayerTreeViewForTesting() override;
  blink::WebThread* currentThread() override;

  void getPluginList(bool refresh,
                     blink::WebPluginListBuilder* builder) override;

 private:
  MockWebBlobRegistryImpl blob_registry_;
  SimpleWebMimeRegistryImpl mime_registry_;
  scoped_ptr<MockWebClipboardImpl> mock_clipboard_;
  WebFileUtilitiesImpl file_utilities_;
  base::ScopedTempDir file_system_root_;
  scoped_ptr<WebURLLoaderMockFactory> url_loader_factory_;
  cc_blink::WebCompositorSupportImpl compositor_support_;
  scoped_ptr<scheduler::RendererScheduler> renderer_scheduler_;
  scoped_ptr<blink::WebThread> web_thread_;

#if defined(OS_WIN) || defined(OS_MACOSX)
  blink::WebThemeEngine* active_theme_engine_ = nullptr;
#endif
  DISALLOW_COPY_AND_ASSIGN(TestBlinkWebUnitTestSupport);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_
