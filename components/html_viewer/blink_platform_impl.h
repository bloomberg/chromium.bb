// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_PLATFORM_IMPL_H_
#define COMPONENTS_HTML_VIEWER_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_local_storage.h"
#include "base/timer/timer.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "components/html_viewer/mock_web_blob_registry_impl.h"
#include "components/html_viewer/web_mime_registry_impl.h"
#include "components/html_viewer/web_notification_manager_impl.h"
#include "components/html_viewer/web_theme_engine_impl.h"
#include "components/webcrypto/webcrypto_impl.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/services/network/public/interfaces/web_socket_factory.mojom.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebScrollbarBehavior.h"

namespace mojo {
class Shell;
}

namespace scheduler {
class RendererScheduler;
class WebThreadImplForRendererScheduler;
}

namespace html_viewer {

class GlobalState;
class WebClipboardImpl;
class WebCookieJarImpl;

class BlinkPlatformImpl : public blink::Platform {
 public:
  // |shell| may be null in tests.
  BlinkPlatformImpl(GlobalState* global_state,
                    mojo::Shell* shell,
                    scheduler::RendererScheduler* renderer_scheduler);
  ~BlinkPlatformImpl() override;

  // blink::Platform methods:
  blink::WebCookieJar* cookieJar() override;
  blink::WebClipboard* clipboard() override;
  blink::WebMimeRegistry* mimeRegistry() override;
  blink::WebThemeEngine* themeEngine() override;
  blink::WebString defaultLocale() override;
  blink::WebBlobRegistry* blobRegistry() override;
  double currentTimeSeconds() override;
  double monotonicallyIncreasingTimeSeconds() override;
  bool isThreadedCompositingEnabled() override;
  blink::WebCompositorSupport* compositorSupport() override;
  uint32_t getUniqueIdForProcess() override;
  void createMessageChannel(blink::WebMessagePortChannel** channel1,
                            blink::WebMessagePortChannel** channel2) override;
  blink::WebURLLoader* createURLLoader() override;
  blink::WebSocketHandle* createWebSocketHandle() override;
  blink::WebString userAgent() override;
  blink::WebData parseDataURL(const blink::WebURL& url,
                              blink::WebString& mime_type,
                              blink::WebString& charset) override;
  bool isReservedIPAddress(const blink::WebString& host) const override;
  blink::WebURLError cancelledError(const blink::WebURL& url) const override;
  blink::WebThread* createThread(const char* name) override;
  blink::WebThread* currentThread() override;
  void yieldCurrentThread() override;
  blink::WebWaitableEvent* createWaitableEvent(
      blink::WebWaitableEvent::ResetPolicy policy,
      blink::WebWaitableEvent::InitialState state) override;
  blink::WebWaitableEvent* waitMultipleEvents(
      const blink::WebVector<blink::WebWaitableEvent*>& events) override;
  blink::WebScrollbarBehavior* scrollbarBehavior() override;
  blink::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      blink::WebGraphicsContext3D* share_context) override;
  blink::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      blink::WebGraphicsContext3D* share_context,
      blink::WebGraphicsContext3D::WebGraphicsInfo* gl_info) override;
  blink::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes) override;
  blink::WebGraphicsContext3DProvider*
  createSharedOffscreenGraphicsContext3DProvider() override;
  blink::WebData loadResource(const char* name) override;
  blink::WebGestureCurve* createFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) override;
  blink::WebCrypto* crypto() override;
  blink::WebNotificationManager* notificationManager() override;

 private:
  void UpdateWebThreadTLS(blink::WebThread* thread);

  static void DestroyCurrentThread(void*);

  GlobalState* global_state_;
  mojo::Shell* shell_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_ptr<blink::WebThread> main_thread_;
  base::ThreadLocalStorage::Slot current_thread_slot_;
  cc_blink::WebCompositorSupportImpl compositor_support_;
  WebThemeEngineImpl theme_engine_;
  WebMimeRegistryImpl mime_registry_;
  webcrypto::WebCryptoImpl web_crypto_;
  WebNotificationManagerImpl web_notification_manager_;
  blink::WebScrollbarBehavior scrollbar_behavior_;
  mojo::WebSocketFactoryPtr web_socket_factory_;
  mojo::URLLoaderFactoryPtr url_loader_factory_;
  MockWebBlobRegistryImpl blob_registry_;
  scoped_ptr<WebCookieJarImpl> cookie_jar_;
  scoped_ptr<WebClipboardImpl> clipboard_;

  DISALLOW_COPY_AND_ASSIGN(BlinkPlatformImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_BLINK_PLATFORM_IMPL_H_
