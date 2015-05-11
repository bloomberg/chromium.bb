// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_dispatcher.h"

#include "base/logging.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/common/presentation_constants.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/presentation/presentation_session_client.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationError.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace {

blink::WebPresentationError::ErrorType GetWebPresentationErrorTypeFromMojo(
    presentation::PresentationErrorType mojoErrorType) {
  switch (mojoErrorType) {
    case presentation::PRESENTATION_ERROR_TYPE_NO_AVAILABLE_SCREENS:
      return blink::WebPresentationError::ErrorTypeNoAvailableScreens;
    case presentation::PRESENTATION_ERROR_TYPE_SESSION_REQUEST_CANCELLED:
      return blink::WebPresentationError::ErrorTypeSessionRequestCancelled;
    case presentation::PRESENTATION_ERROR_TYPE_NO_PRESENTATION_FOUND:
      return blink::WebPresentationError::ErrorTypeNoPresentationFound;
    case presentation::PRESENTATION_ERROR_TYPE_UNKNOWN:
    default:
      return blink::WebPresentationError::ErrorTypeUnknown;
  }
}

blink::WebPresentationSessionState GetWebPresentationSessionStateFromMojo(
        presentation::PresentationSessionState mojoSessionState) {
  switch (mojoSessionState) {
    case presentation::PRESENTATION_SESSION_STATE_CONNECTED:
      return blink::WebPresentationSessionState::Connected;
    case presentation::PRESENTATION_SESSION_STATE_DISCONNECTED:
      return blink::WebPresentationSessionState::Disconnected;
  }

  NOTREACHED();
  return blink::WebPresentationSessionState::Disconnected;
}

GURL GetPresentationURLFromFrame(content::RenderFrame* frame) {
  DCHECK(frame);

  GURL url(frame->GetWebFrame()->document().defaultPresentationURL());
  return url.is_valid() ? url : GURL();
}

}  // namespace

namespace content {

PresentationDispatcher::PresentationDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      controller_(nullptr) {
}

PresentationDispatcher::~PresentationDispatcher() {
  // Controller should be destroyed before the dispatcher when frame is
  // destroyed.
  DCHECK(!controller_);
}

void PresentationDispatcher::setController(
    blink::WebPresentationController* controller) {
  // There shouldn't be any swapping from one non-null controller to another.
  DCHECK(controller != controller_ && (!controller || !controller_));
  controller_ = controller;
  // The controller is set to null when the frame is about to be detached.
  // Nothing is listening for screen availability anymore but the Mojo service
  // will know about the frame being detached anyway.
}

void PresentationDispatcher::updateAvailableChangeWatched(bool watched) {
  GURL presentation_url(GetPresentationURLFromFrame(render_frame()));
  DoUpdateAvailableChangeWatched(presentation_url.spec(), watched);
}

void PresentationDispatcher::DoUpdateAvailableChangeWatched(
    const std::string& presentation_url, bool watched) {
  ConnectToPresentationServiceIfNeeded();
  if (watched) {
    presentation_service_->ListenForScreenAvailability(
        presentation_url,
        base::Bind(&PresentationDispatcher::OnScreenAvailabilityChanged,
                 base::Unretained(this)));
  } else {
    presentation_service_->RemoveScreenAvailabilityListener(presentation_url);
  }
}

void PresentationDispatcher::startSession(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    blink::WebPresentationSessionClientCallbacks* callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  // The dispatcher owns the service so |this| will be valid when
  // OnSessionCreated() is called. |callback| needs to be alive and also needs
  // to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->StartSession(
      presentationUrl.utf8(),
      presentationId.utf8(),
      base::Bind(&PresentationDispatcher::OnSessionCreated,
          base::Unretained(this),
          base::Owned(callback)));
}

void PresentationDispatcher::joinSession(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    blink::WebPresentationSessionClientCallbacks* callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  // The dispatcher owns the service so |this| will be valid when
  // OnSessionCreated() is called. |callback| needs to be alive and also needs
  // to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->JoinSession(
      presentationUrl.utf8(),
      presentationId.utf8(),
      base::Bind(&PresentationDispatcher::OnSessionCreated,
          base::Unretained(this),
          base::Owned(callback)));
}

void PresentationDispatcher::sendString(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    const blink::WebString& message) {
  if (message.utf8().size() > kMaxPresentationSessionMessageSize) {
    // TODO(crbug.com/459008): Limit the size of individual messages to 64k
    // for now. Consider throwing DOMException or splitting bigger messages
    // into smaller chunks later.
    LOG(WARNING) << "message size exceeded limit!";
    return;
  }

  presentation::SessionMessage* session_message =
      new presentation::SessionMessage();
  session_message->presentation_url = presentationUrl.utf8();
  session_message->presentation_id = presentationId.utf8();
  session_message->type = presentation::PresentationMessageType::
                          PRESENTATION_MESSAGE_TYPE_TEXT;
  session_message->message = message.utf8();

  message_request_queue_.push(make_linked_ptr(session_message));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1) {
    const linked_ptr<presentation::SessionMessage>& request =
        message_request_queue_.front();
    DoSendMessage(*request);
  }
}

void PresentationDispatcher::sendArrayBuffer(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    const uint8* data,
    size_t length) {
  DCHECK(data);
  if (length > kMaxPresentationSessionMessageSize) {
    // TODO(crbug.com/459008): Same as in sendString().
    LOG(WARNING) << "data size exceeded limit!";
    return;
  }

  const std::vector<uint8> vector(data, data + length);
  presentation::SessionMessage* session_message =
      new presentation::SessionMessage();
  session_message->presentation_url = presentationUrl.utf8();
  session_message->presentation_id = presentationId.utf8();
  session_message->type = presentation::PresentationMessageType::
                          PRESENTATION_MESSAGE_TYPE_ARRAY_BUFFER;
  session_message->data = mojo::Array<uint8>::From(vector);

  message_request_queue_.push(make_linked_ptr(session_message));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1) {
    const linked_ptr<presentation::SessionMessage>& request =
        message_request_queue_.front();
    DoSendMessage(*request);
  }
}

void PresentationDispatcher::DoSendMessage(
    const presentation::SessionMessage& session_message) {
  ConnectToPresentationServiceIfNeeded();

  presentation::SessionMessagePtr message_request(
      presentation::SessionMessage::New());
  message_request->presentation_url = session_message.presentation_url;
  message_request->presentation_id = session_message.presentation_id;
  message_request->type = session_message.type;
  if (session_message.type == presentation::PresentationMessageType::
                              PRESENTATION_MESSAGE_TYPE_TEXT) {
    message_request->message = session_message.message;
  } else if (session_message.type == presentation::PresentationMessageType::
                                      PRESENTATION_MESSAGE_TYPE_ARRAY_BUFFER) {
    message_request->data = mojo::Array<uint8>::From(
        session_message.data.storage());
  }

  presentation_service_->SendSessionMessage(
      message_request.Pass(),
      base::Bind(&PresentationDispatcher::HandleSendMessageRequests,
                 base::Unretained(this)));
}

void PresentationDispatcher::HandleSendMessageRequests(bool success) {
  // In normal cases, message_request_queue_ should not be empty at this point
  // of time, but when DidCommitProvisionalLoad() is invoked before receiving
  // the callback for previous send mojo call, queue would have been emptied.
  if (message_request_queue_.empty())
    return;

  if (!success) {
    // PresentationServiceImpl is informing that Frame has been detached or
    // navigated away. Invalidate all pending requests.
    MessageRequestQueue empty;
    std::swap(message_request_queue_, empty);
    return;
  }

  message_request_queue_.pop();
  if (!message_request_queue_.empty()) {
    const linked_ptr<presentation::SessionMessage>& request =
        message_request_queue_.front();
    DoSendMessage(*request);
  }
}

void PresentationDispatcher::closeSession(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId) {
  ConnectToPresentationServiceIfNeeded();

  presentation_service_->CloseSession(
      presentationUrl.utf8(),
      presentationId.utf8());
}

void PresentationDispatcher::DidChangeDefaultPresentation() {
  GURL presentation_url(GetPresentationURLFromFrame(render_frame()));

  ConnectToPresentationServiceIfNeeded();
  presentation_service_->SetDefaultPresentationURL(
      presentation_url.spec(), mojo::String());
}

void PresentationDispatcher::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  // If not top-level navigation.
  if (frame->parent() || is_same_page_navigation)
    return;

  // Remove all pending send message requests.
  MessageRequestQueue empty;
  std::swap(message_request_queue_, empty);
}

void PresentationDispatcher::OnScreenAvailabilityChanged(
    const std::string& presentation_url, bool available) {
  if (!controller_)
    return;

  // Reset the callback to get the next event.
  DoUpdateAvailableChangeWatched(presentation_url,
                                 controller_->isAvailableChangeWatched());

  controller_->didChangeAvailability(available);
}

void PresentationDispatcher::OnDefaultSessionStarted(
    presentation::PresentationSessionInfoPtr session_info) {
  if (!controller_)
    return;

  // Reset the callback to get the next event.
  presentation_service_->ListenForDefaultSessionStart(base::Bind(
      &PresentationDispatcher::OnDefaultSessionStarted,
      base::Unretained(this)));

  if (!session_info.is_null()) {
    controller_->didStartDefaultSession(
        new PresentationSessionClient(session_info.Pass()));
  }
}

void PresentationDispatcher::OnSessionCreated(
    blink::WebPresentationSessionClientCallbacks* callback,
    presentation::PresentationSessionInfoPtr session_info,
    presentation::PresentationErrorPtr error) {
  DCHECK(callback);
  if (!error.is_null()) {
    DCHECK(session_info.is_null());
    callback->onError(new blink::WebPresentationError(
        GetWebPresentationErrorTypeFromMojo(error->error_type),
        blink::WebString::fromUTF8(error->message)));
    return;
  }

  DCHECK(!session_info.is_null());
  callback->onSuccess(new PresentationSessionClient(session_info.Pass()));
}

void PresentationDispatcher::OnSessionStateChange(
    presentation::PresentationSessionInfoPtr session_info,
    presentation::PresentationSessionState session_state) {
  if (!controller_)
    return;

  presentation_service_->ListenForSessionStateChange(base::Bind(
      &PresentationDispatcher::OnSessionStateChange,
      base::Unretained(this)));

  DCHECK(!session_info.is_null());
  controller_->didChangeSessionState(
      new PresentationSessionClient(session_info.Pass()),
      GetWebPresentationSessionStateFromMojo(session_state));
}

void PresentationDispatcher::OnSessionMessagesReceived(
    mojo::Array<presentation::SessionMessagePtr> messages) {
  // When messages is null, there is an error at presentation service side.
  if (!controller_ || messages.is_null())
    return;

  for (size_t i = 0; i < messages.size(); ++i) {
    if (messages[i]->type ==
        presentation::PresentationMessageType::PRESENTATION_MESSAGE_TYPE_TEXT) {
      controller_->didReceiveSessionTextMessage(
          new PresentationSessionClient(messages[i]->presentation_url,
                                        messages[i]->presentation_id),
          blink::WebString::fromUTF8(messages[i]->message));
    } else {
      // TODO(haibinlu): handle binary message
    }
  }

  presentation_service_->ListenForSessionMessages(
      base::Bind(&PresentationDispatcher::OnSessionMessagesReceived,
                 base::Unretained(this)));
}

void PresentationDispatcher::ConnectToPresentationServiceIfNeeded() {
  if (presentation_service_.get())
    return;

  render_frame()->GetServiceRegistry()->ConnectToRemoteService(
      &presentation_service_);
  // TODO(imcheng): Uncomment these once they are implemented on the browser
  // side. (crbug.com/459006)
  /*
  presentation_service_->ListenForDefaultSessionStart(base::Bind(
      &PresentationDispatcher::OnDefaultSessionStarted,
      base::Unretained(this)));
  presentation_service_->ListenForSessionStateChange(base::Bind(
      &PresentationDispatcher::OnSessionStateChange,
      base::Unretained(this)));
  presentation_service_->ListenForSessionMessages(
      base::Bind(&PresentationDispatcher::OnSessionMessagesReceived,
      base::Unretained(this)));
  */
}

}  // namespace content
