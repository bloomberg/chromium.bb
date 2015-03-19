// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/presentation/presentation_session_dispatcher.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {
class WebString;
}  // namespace blink

namespace content {

// PresentationDispatcher is a delegate for Presentation API messages used by
// Blink. It forwards the calls to the Mojo PresentationService.
class CONTENT_EXPORT PresentationDispatcher
    : public RenderFrameObserver,
      public NON_EXPORTED_BASE(blink::WebPresentationClient) {
 public:
  explicit PresentationDispatcher(RenderFrame* render_frame);
  ~PresentationDispatcher() override;

 private:
  // WebPresentationClient implementation.
  virtual void setController(
      blink::WebPresentationController* controller);
  virtual void updateAvailableChangeWatched(bool watched);
  virtual void startSession(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId,
      blink::WebPresentationSessionClientCallbacks* callback);
  virtual void joinSession(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId,
      blink::WebPresentationSessionClientCallbacks* callback);
  virtual void closeSession(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId);

  // RenderFrameObserver
  void DidChangeDefaultPresentation() override;

  void OnScreenAvailabilityChanged(
      const std::string& presentation_url,
      bool available);
  void OnSessionCreated(
      blink::WebPresentationSessionClientCallbacks* callback,
      presentation::PresentationSessionInfoPtr session_info,
      presentation::PresentationErrorPtr error);
  void OnDefaultSessionStarted(
      presentation::PresentationSessionInfoPtr session_info);

  void ConnectToPresentationServiceIfNeeded();

  void DoUpdateAvailableChangeWatched(
      const std::string& presentation_url,
      bool watched);

  // Used as a weak reference. Can be null since lifetime is bound to the frame.
  blink::WebPresentationController* controller_;
  presentation::PresentationServicePtr presentation_service_;

  // Wrappers around the PresentationSessionPtr corresponding to each Javascript
  // PresentationSession object passed back to the frame.
  // TODO(avayvod): Switch to base::IDMap when the lookup by presentation id is
  // needed.
  using PresentationSessionDispatchers =
      ScopedVector<PresentationSessionDispatcher>;
  PresentationSessionDispatchers presentation_session_dispatchers_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
