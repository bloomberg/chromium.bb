// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/id_map.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/presentation_info.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationClient.h"
#include "third_party/WebKit/public/platform/modules/presentation/presentation.mojom.h"
#include "url/gurl.h"

namespace blink {
class WebPresentationReceiver;
class WebString;
class WebURL;
template <typename T>
class WebVector;
}  // namespace blink

namespace content {

// PresentationDispatcher is a delegate for Presentation API used by
// Blink. It forwards the calls to the Mojo PresentationService.
class CONTENT_EXPORT PresentationDispatcher
    : public RenderFrameObserver,
      public blink::WebPresentationClient {
 public:
  explicit PresentationDispatcher(RenderFrame* render_frame);
  ~PresentationDispatcher() override;

 private:
  friend class TestPresentationDispatcher;
  friend class PresentationDispatcherTest;
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestStartPresentation);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestStartPresentationError);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestReconnectPresentation);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestReconnectPresentationError);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestReconnectPresentationNoConnection);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestOnReceiverConnectionAvailable);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestCloseConnection);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestTerminatePresentation);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestListenForScreenAvailability);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestSetDefaultPresentationUrls);

  // WebPresentationClient implementation.
  void SetReceiver(blink::WebPresentationReceiver*) override;
  void StartPresentation(
      const blink::WebVector<blink::WebURL>& presentationUrls,
      std::unique_ptr<blink::WebPresentationConnectionCallbacks> callback)
      override;
  void ReconnectPresentation(
      const blink::WebVector<blink::WebURL>& presentationUrls,
      const blink::WebString& presentationId,
      std::unique_ptr<blink::WebPresentationConnectionCallbacks> callback)
      override;

  // RenderFrameObserver implementation.
  void DidFinishDocumentLoad() override;
  void OnDestruct() override;
  void WidgetWillClose() override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_document_navigation) override;

  void OnConnectionCreated(
      std::unique_ptr<blink::WebPresentationConnectionCallbacks> callback,
      const base::Optional<PresentationInfo>& presentation_info,
      const base::Optional<PresentationError>& error);

  virtual void ConnectToPresentationServiceIfNeeded();

  // Used as a weak reference. Can be null since lifetime is bound to the frame.
  blink::WebPresentationReceiver* receiver_;
  blink::mojom::PresentationServicePtr presentation_service_;

  DISALLOW_COPY_AND_ASSIGN(PresentationDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
