// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_dispatcher.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/common/presentation_constants.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/presentation/presentation_session_client.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationError.h"
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

}  // namespace

namespace content {

PresentationDispatcher::PresentationDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      controller_(nullptr),
      binding_(this) {
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

void PresentationDispatcher::startSession(
    const blink::WebString& presentationUrl,
    blink::WebPresentationSessionClientCallbacks* callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  // The dispatcher owns the service so |this| will be valid when
  // OnSessionCreated() is called. |callback| needs to be alive and also needs
  // to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->StartSession(
      presentationUrl.utf8(),
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

  message_request_queue_.push(make_linked_ptr(
      CreateSendTextMessageRequest(presentationUrl, presentationId, message)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
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

  message_request_queue_.push(make_linked_ptr(
      CreateSendBinaryMessageRequest(presentationUrl, presentationId,
                                     presentation::PresentationMessageType::
                                         PRESENTATION_MESSAGE_TYPE_ARRAY_BUFFER,
                                     data, length)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::sendBlobData(
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

  message_request_queue_.push(make_linked_ptr(CreateSendBinaryMessageRequest(
      presentationUrl, presentationId,
      presentation::PresentationMessageType::PRESENTATION_MESSAGE_TYPE_BLOB,
      data, length)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::DoSendMessage(SendMessageRequest* request) {
  ConnectToPresentationServiceIfNeeded();

  presentation_service_->SendSessionMessage(
      request->session_info.Pass(), request->message.Pass(),
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
    DoSendMessage(message_request_queue_.front().get());
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

void PresentationDispatcher::getAvailability(
    const blink::WebString& availabilityUrl,
    blink::WebPresentationAvailabilityCallbacks* callbacks) {
  const std::string& availability_url = availabilityUrl.utf8();
  AvailabilityStatus* status = nullptr;
  auto status_it = availability_status_.find(availability_url);
  if (status_it == availability_status_.end()) {
    status = new AvailabilityStatus(availability_url);
    availability_status_.set(availability_url, make_scoped_ptr(status));
  } else {
    status = status_it->second;
  }
  DCHECK(status);

  if (status->listening_state == ListeningState::ACTIVE) {
    callbacks->onSuccess(status->last_known_availability);
    delete callbacks;
    return;
  }

  status->availability_callbacks.Add(callbacks);
  UpdateListeningState(status);
}

void PresentationDispatcher::startListening(
    blink::WebPresentationAvailabilityObserver* observer) {
  if (default_presentation_url_.empty())
    return;
  startListening(blink::WebString::fromUTF8(default_presentation_url_),
                 observer);
}

void PresentationDispatcher::startListening(
    const blink::WebString& availabilityUrl,
    blink::WebPresentationAvailabilityObserver* observer) {
  auto status_it = availability_status_.find(availabilityUrl.utf8());
  if (status_it == availability_status_.end()) {
    DLOG(WARNING) << "Start listening for availability for unknown URL "
                  << availabilityUrl.utf8();
    return;
  }
  status_it->second->availability_observers.insert(observer);
  UpdateListeningState(status_it->second);
}

void PresentationDispatcher::stopListening(
    blink::WebPresentationAvailabilityObserver* observer) {
  if (default_presentation_url_.empty())
    return;
  stopListening(blink::WebString::fromUTF8(default_presentation_url_),
                observer);
}

void PresentationDispatcher::stopListening(
    const blink::WebString& availabilityUrl,
    blink::WebPresentationAvailabilityObserver* observer) {
  auto status_it = availability_status_.find(availabilityUrl.utf8());
  if (status_it == availability_status_.end()) {
    DLOG(WARNING) << "Stop listening for availability for unknown URL "
                  << availabilityUrl.utf8();
    return;
  }
  status_it->second->availability_observers.erase(observer);
  UpdateListeningState(status_it->second);
}

void PresentationDispatcher::setDefaultPresentationUrl(
    const blink::WebString& url) {
  default_presentation_url_ = url.utf8();
  ConnectToPresentationServiceIfNeeded();
  presentation_service_->SetDefaultPresentationURL(default_presentation_url_);
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

void PresentationDispatcher::OnScreenAvailabilityUpdated(
    const mojo::String& url, bool available) {
  const std::string& availability_url = url.get();
  auto status_it = availability_status_.find(availability_url);
  if (status_it == availability_status_.end())
    return;
  AvailabilityStatus* status = status_it->second;
  DCHECK(status);

  if (status->listening_state == ListeningState::WAITING)
    status->listening_state = ListeningState::ACTIVE;

  for (auto observer : status->availability_observers)
    observer->availabilityChanged(available);

  for (AvailabilityCallbacksMap::iterator iter(&status->availability_callbacks);
       !iter.IsAtEnd(); iter.Advance()) {
    iter.GetCurrentValue()->onSuccess(available);
  }
  status->last_known_availability = available;
  status->availability_callbacks.Clear();
  UpdateListeningState(status);
}

void PresentationDispatcher::OnScreenAvailabilityNotSupported(
    const mojo::String& url) {
  const std::string& availability_url = url.get();
  auto status_it = availability_status_.find(availability_url);
  if (status_it == availability_status_.end())
    return;
  AvailabilityStatus* status = status_it->second;
  DCHECK(status);
  DCHECK(status->listening_state == ListeningState::WAITING);

  const blink::WebString& not_supported_error = blink::WebString::fromUTF8(
      "getAvailability() isn't supported at the moment. It can be due to "
      "a permanent or temporary system limitation. It is recommended to "
      "try to blindly start a session in that case.");
  for (AvailabilityCallbacksMap::iterator iter(&status->availability_callbacks);
       !iter.IsAtEnd(); iter.Advance()) {
    iter.GetCurrentValue()->onError(blink::WebPresentationError(
        blink::WebPresentationError::ErrorTypeAvailabilityNotSupported,
        not_supported_error));
  }
  status->availability_callbacks.Clear();
  UpdateListeningState(status);
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
        new PresentationSessionClient(session_info.Clone()));
    presentation_service_->ListenForSessionMessages(session_info.Pass());
  }
}

void PresentationDispatcher::OnSessionCreated(
    blink::WebPresentationSessionClientCallbacks* callback,
    presentation::PresentationSessionInfoPtr session_info,
    presentation::PresentationErrorPtr error) {
  DCHECK(callback);
  if (!error.is_null()) {
    DCHECK(session_info.is_null());
    callback->onError(blink::WebPresentationError(
        GetWebPresentationErrorTypeFromMojo(error->error_type),
        blink::WebString::fromUTF8(error->message)));
    return;
  }

  DCHECK(!session_info.is_null());
  callback->onSuccess(
      blink::adoptWebPtr(new PresentationSessionClient(session_info.Clone())));
  presentation_service_->ListenForSessionMessages(session_info.Pass());
}

void PresentationDispatcher::OnSessionStateChanged(
    presentation::PresentationSessionInfoPtr session_info,
    presentation::PresentationSessionState session_state) {
  if (!controller_)
    return;

  DCHECK(!session_info.is_null());
  controller_->didChangeSessionState(
      new PresentationSessionClient(session_info.Pass()),
      GetWebPresentationSessionStateFromMojo(session_state));
}

void PresentationDispatcher::OnSessionMessagesReceived(
    presentation::PresentationSessionInfoPtr session_info,
    mojo::Array<presentation::SessionMessagePtr> messages) {
  if (!controller_)
    return;

  for (size_t i = 0; i < messages.size(); ++i) {
    // Note: Passing batches of messages to the Blink layer would be more
    // efficient.
    scoped_ptr<PresentationSessionClient> session_client(
        new PresentationSessionClient(session_info->url, session_info->id));
    switch (messages[i]->type) {
      case presentation::PresentationMessageType::
          PRESENTATION_MESSAGE_TYPE_TEXT: {
        controller_->didReceiveSessionTextMessage(
            session_client.release(),
            blink::WebString::fromUTF8(messages[i]->message));
        break;
      }
      case presentation::PresentationMessageType::
          PRESENTATION_MESSAGE_TYPE_ARRAY_BUFFER:
      case presentation::PresentationMessageType::
          PRESENTATION_MESSAGE_TYPE_BLOB: {
        controller_->didReceiveSessionBinaryMessage(
            session_client.release(), &(messages[i]->data.front()),
            messages[i]->data.size());
        break;
      }
      default: {
        NOTREACHED();
        break;
      }
    }
  }
}

void PresentationDispatcher::ConnectToPresentationServiceIfNeeded() {
  if (presentation_service_.get())
    return;

  render_frame()->GetServiceRegistry()->ConnectToRemoteService(
      mojo::GetProxy(&presentation_service_));
  presentation::PresentationServiceClientPtr client_ptr;
  binding_.Bind(GetProxy(&client_ptr));
  presentation_service_->SetClient(client_ptr.Pass());

  presentation_service_->ListenForDefaultSessionStart(base::Bind(
      &PresentationDispatcher::OnDefaultSessionStarted,
      base::Unretained(this)));
  presentation_service_->ListenForSessionStateChange();
}

void PresentationDispatcher::UpdateListeningState(AvailabilityStatus* status) {
  bool should_listen = !status->availability_callbacks.IsEmpty() ||
                       !status->availability_observers.empty();
  bool is_listening = status->listening_state != ListeningState::INACTIVE;

  if (should_listen == is_listening)
    return;

  ConnectToPresentationServiceIfNeeded();
  if (should_listen) {
    status->listening_state = ListeningState::WAITING;
    presentation_service_->ListenForScreenAvailability(status->url);
  } else {
    status->listening_state = ListeningState::INACTIVE;
    presentation_service_->StopListeningForScreenAvailability(status->url);
  }
}

PresentationDispatcher::SendMessageRequest::SendMessageRequest(
    presentation::PresentationSessionInfoPtr session_info,
    presentation::SessionMessagePtr message)
    : session_info(session_info.Pass()), message(message.Pass()) {}

PresentationDispatcher::SendMessageRequest::~SendMessageRequest() {}

// static
PresentationDispatcher::SendMessageRequest*
PresentationDispatcher::CreateSendTextMessageRequest(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    const blink::WebString& message) {
  presentation::PresentationSessionInfoPtr session_info =
      presentation::PresentationSessionInfo::New();
  session_info->url = presentationUrl.utf8();
  session_info->id = presentationId.utf8();

  presentation::SessionMessagePtr session_message =
      presentation::SessionMessage::New();
  session_message->type =
      presentation::PresentationMessageType::PRESENTATION_MESSAGE_TYPE_TEXT;
  session_message->message = message.utf8();
  return new SendMessageRequest(session_info.Pass(), session_message.Pass());
}

// static
PresentationDispatcher::SendMessageRequest*
PresentationDispatcher::CreateSendBinaryMessageRequest(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    presentation::PresentationMessageType type,
    const uint8* data,
    size_t length) {
  presentation::PresentationSessionInfoPtr session_info =
      presentation::PresentationSessionInfo::New();
  session_info->url = presentationUrl.utf8();
  session_info->id = presentationId.utf8();

  presentation::SessionMessagePtr session_message =
      presentation::SessionMessage::New();
  session_message->type = type;
  std::vector<uint8> tmp_data_vector(data, data + length);
  session_message->data.Swap(&tmp_data_vector);
  return new SendMessageRequest(session_info.Pass(), session_message.Pass());
}

PresentationDispatcher::AvailabilityStatus::AvailabilityStatus(
    const std::string& availability_url) :
  url(availability_url), last_known_availability(false) {
}

PresentationDispatcher::AvailabilityStatus::~AvailabilityStatus() {
}

}  // namespace content
