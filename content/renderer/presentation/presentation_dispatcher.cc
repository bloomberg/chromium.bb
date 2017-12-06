// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_dispatcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnection.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnectionCallbacks.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationError.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationInfo.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationReceiver.h"
#include "third_party/WebKit/public/platform/modules/presentation/presentation.mojom.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace content {

namespace {

blink::WebPresentationError::ErrorType GetWebPresentationErrorType(
    PresentationErrorType errorType) {
  switch (errorType) {
    case PresentationErrorType::PRESENTATION_ERROR_NO_AVAILABLE_SCREENS:
      return blink::WebPresentationError::kErrorTypeNoAvailableScreens;
    case PresentationErrorType::
        PRESENTATION_ERROR_PRESENTATION_REQUEST_CANCELLED:
      return blink::WebPresentationError::
          kErrorTypePresentationRequestCancelled;
    case PresentationErrorType::PRESENTATION_ERROR_NO_PRESENTATION_FOUND:
      return blink::WebPresentationError::kErrorTypeNoPresentationFound;
    case PresentationErrorType::PRESENTATION_ERROR_PREVIOUS_START_IN_PROGRESS:
      return blink::WebPresentationError::kErrorTypePreviousStartInProgress;
    case PresentationErrorType::PRESENTATION_ERROR_UNKNOWN:
    default:
      return blink::WebPresentationError::kErrorTypeUnknown;
  }
}

}  // namespace

// TODO(mfoltz): Reorder definitions to match presentation_dispatcher.h.

PresentationDispatcher::PresentationDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame), receiver_(nullptr) {}

PresentationDispatcher::~PresentationDispatcher() {
}

void PresentationDispatcher::StartPresentation(
    const blink::WebVector<blink::WebURL>& presentationUrls,
    std::unique_ptr<blink::WebPresentationConnectionCallbacks> callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  std::vector<GURL> urls;
  for (const auto& url : presentationUrls)
    urls.push_back(url);

  // The dispatcher owns the service so |this| will be valid when
  // OnConnectionCreated() is called. |callback| needs to be alive and also
  // needs to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->StartPresentation(
      urls, base::BindOnce(&PresentationDispatcher::OnConnectionCreated,
                           base::Unretained(this), base::Passed(&callback)));
}

void PresentationDispatcher::ReconnectPresentation(
    const blink::WebVector<blink::WebURL>& presentationUrls,
    const blink::WebString& presentationId,
    std::unique_ptr<blink::WebPresentationConnectionCallbacks> callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  std::vector<GURL> urls;
  for (const auto& url : presentationUrls)
    urls.push_back(url);

  // The dispatcher owns the service so |this| will be valid when
  // OnConnectionCreated() is called. |callback| needs to be alive and also
  // needs to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->ReconnectPresentation(
      urls, presentationId.Utf8(),
      base::BindOnce(&PresentationDispatcher::OnConnectionCreated,
                     base::Unretained(this), base::Passed(&callback)));
}

void PresentationDispatcher::SetReceiver(
    blink::WebPresentationReceiver* receiver) {
  receiver_ = receiver;

  DCHECK(!render_frame() || render_frame()->IsMainFrame());

  // Init |receiver_| after loading document.
  if (receiver_ && render_frame() && render_frame()->GetWebFrame() &&
      !render_frame()->GetWebFrame()->IsLoading()) {
    receiver_->Init();
  }
}

void PresentationDispatcher::DidFinishDocumentLoad() {
  if (receiver_)
    receiver_->Init();
}

void PresentationDispatcher::OnDestruct() {
  delete this;
}

void PresentationDispatcher::WidgetWillClose() {
  if (receiver_)
    receiver_->OnReceiverTerminated();
}

void PresentationDispatcher::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_document_navigation) {
  if (!is_same_document_navigation)
    presentation_service_.reset();
}

void PresentationDispatcher::OnConnectionCreated(
    std::unique_ptr<blink::WebPresentationConnectionCallbacks> callback,
    const base::Optional<PresentationInfo>& presentation_info,
    const base::Optional<PresentationError>& error) {
  DCHECK(callback);
  if (error) {
    DCHECK(!presentation_info);
    callback->OnError(blink::WebPresentationError(
        GetWebPresentationErrorType(error->error_type),
        blink::WebString::FromUTF8(error->message)));
    return;
  }

  DCHECK(presentation_info);
  callback->OnSuccess(blink::WebPresentationInfo(
      presentation_info->presentation_url,
      blink::WebString::FromUTF8(presentation_info->presentation_id)));

  auto* connection = callback->GetConnection();
  if (connection)
    connection->Init();
}

void PresentationDispatcher::ConnectToPresentationServiceIfNeeded() {
  if (presentation_service_)
    return;

  render_frame()->GetRemoteInterfaces()->GetInterface(&presentation_service_);
}

}  // namespace content
