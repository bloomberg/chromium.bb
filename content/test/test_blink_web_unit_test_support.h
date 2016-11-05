// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_
#define CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "content/child/blink_platform_impl.h"
#include "content/child/webfileutilities_impl.h"
#include "content/test/mock_webblob_registry_impl.h"
#include "content/test/mock_webclipboard_impl.h"
#include "third_party/WebKit/public/platform/WebURLLoaderMockFactory.h"

namespace blink {
namespace scheduler {
class RendererScheduler;
}
}

namespace cc {
class TestSharedBitmapManager;
}

namespace content {

// An implementation of BlinkPlatformImpl for tests.
class TestBlinkWebUnitTestSupport : public BlinkPlatformImpl {
 public:
  TestBlinkWebUnitTestSupport();
  ~TestBlinkWebUnitTestSupport() override;

  blink::WebBlobRegistry* getBlobRegistry() override;
  blink::WebClipboard* clipboard() override;
  blink::WebFileUtilities* fileUtilities() override;
  blink::WebIDBFactory* idbFactory() override;

  blink::WebURLLoader* createURLLoader() override;
  blink::WebString userAgent() override;
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

  blink::WebURLLoaderMockFactory* getURLLoaderMockFactory() override;

  blink::WebThread* currentThread() override;

  std::unique_ptr<cc::SharedBitmap> allocateSharedBitmap(
      const blink::WebSize& size) override;

  void getPluginList(bool refresh,
                     const blink::WebSecurityOrigin& mainFrameOrigin,
                     blink::WebPluginListBuilder* builder) override;

  blink::WebRTCCertificateGenerator* createRTCCertificateGenerator() override;

 private:
  MockWebBlobRegistryImpl blob_registry_;
  std::unique_ptr<MockWebClipboardImpl> mock_clipboard_;
  WebFileUtilitiesImpl file_utilities_;
  base::ScopedTempDir file_system_root_;
  std::unique_ptr<blink::WebURLLoaderMockFactory> url_loader_factory_;
  cc_blink::WebCompositorSupportImpl compositor_support_;
  std::unique_ptr<blink::scheduler::RendererScheduler> renderer_scheduler_;
  std::unique_ptr<blink::WebThread> web_thread_;
  std::unique_ptr<cc::TestSharedBitmapManager> shared_bitmap_manager_;

#if defined(OS_WIN) || defined(OS_MACOSX)
  blink::WebThemeEngine* active_theme_engine_ = nullptr;
#endif
  DISALLOW_COPY_AND_ASSIGN(TestBlinkWebUnitTestSupport);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BLINK_WEB_UNIT_TEST_SUPPORT_H_
