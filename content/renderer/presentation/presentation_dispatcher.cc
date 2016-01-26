// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_dispatcher.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/common/presentation_constants.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/presentation/presentation_connection_client.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationError.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace {

blink::WebPresentationError::ErrorType GetWebPresentationErrorTypeFromMojo(
    presentation::PresentationErrorType mojoErrorType) {
  switch (mojoErrorType) {
    case presentation::PresentationErrorType::NO_AVAILABLE_SCREENS:
      return blink::WebPresentationError::ErrorTypeNoAvailableScreens;
    case presentation::PresentationErrorType::SESSION_REQUEST_CANCELLED:
      return blink::WebPresentationError::ErrorTypeSessionRequestCancelled;
    case presentation::PresentationErrorType::NO_PRESENTATION_FOUND:
      return blink::WebPresentationError::ErrorTypeNoPresentationFound;
    case presentation::PresentationErrorType::UNKNOWN:
    default:
      return blink::WebPresentationError::ErrorTypeUnknown;
  }
}

blink::WebPresentationConnectionState GetWebPresentationConnectionStateFromMojo(
    presentation::PresentationConnectionState mojoSessionState) {
  switch (mojoSessionState) {
    // TODO(imcheng): Add Connecting state to Blink (crbug.com/575351).
    case presentation::PresentationConnectionState::CONNECTED:
      return blink::WebPresentationConnectionState::Connected;
    case presentation::PresentationConnectionState::CLOSED:
      return blink::WebPresentationConnectionState::Closed;
    case presentation::PresentationConnectionState::TERMINATED:
      return blink::WebPresentationConnectionState::Terminated;
    default:
      NOTREACHED();
      return blink::WebPresentationConnectionState::Terminated;
  }
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
    blink::WebPresentationConnectionClientCallbacks* callback) {
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
    blink::WebPresentationConnectionClientCallbacks* callback) {
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

  message_request_queue_.push(make_scoped_ptr(
      CreateSendTextMessageRequest(presentationUrl, presentationId, message)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::sendArrayBuffer(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    const uint8_t* data,
    size_t length) {
  DCHECK(data);
  if (length > kMaxPresentationSessionMessageSize) {
    // TODO(crbug.com/459008): Same as in sendString().
    LOG(WARNING) << "data size exceeded limit!";
    return;
  }

  message_request_queue_.push(make_scoped_ptr(CreateSendBinaryMessageRequest(
      presentationUrl, presentationId,
      presentation::PresentationMessageType::ARRAY_BUFFER, data, length)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::sendBlobData(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    const uint8_t* data,
    size_t length) {
  DCHECK(data);
  if (length > kMaxPresentationSessionMessageSize) {
    // TODO(crbug.com/459008): Same as in sendString().
    LOG(WARNING) << "data size exceeded limit!";
    return;
  }

  message_request_queue_.push(make_scoped_ptr(CreateSendBinaryMessageRequest(
      presentationUrl, presentationId,
      presentation::PresentationMessageType::BLOB, data, length)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::DoSendMessage(SendMessageRequest* request) {
  ConnectToPresentationServiceIfNeeded();

  presentation_service_->SendSessionMessage(
      std::move(request->session_info), std::move(request->message),
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

  presentation_service_->CloseConnection(presentationUrl.utf8(),
                                         presentationId.utf8());
}

void PresentationDispatcher::terminateSession(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId) {
  ConnectToPresentationServiceIfNeeded();

  presentation_service_->Terminate(presentationUrl.utf8(),
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
    availability_status_[availability_url] = make_scoped_ptr(status);
  } else {
    status = status_it->second.get();
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
  const std::string& availability_url = observer->url().string().utf8();
  auto status_it = availability_status_.find(availability_url);
  if (status_it == availability_status_.end()) {
    DLOG(WARNING) << "Start listening for availability for unknown URL "
                  << availability_url;
    return;
  }
  status_it->second->availability_observers.insert(observer);
  UpdateListeningState(status_it->second.get());
}

void PresentationDispatcher::stopListening(
    blink::WebPresentationAvailabilityObserver* observer) {
  const std::string& availability_url = observer->url().string().utf8();
  auto status_it = availability_status_.find(availability_url);
  if (status_it == availability_status_.end()) {
    DLOG(WARNING) << "Stop listening for availability for unknown URL "
                  << availability_url;
    return;
  }
  status_it->second->availability_observers.erase(observer);
  UpdateListeningState(status_it->second.get());
}

void PresentationDispatcher::setDefaultPresentationUrl(
    const blink::WebString& url) {
  ConnectToPresentationServiceIfNeeded();
  presentation_service_->SetDefaultPresentationURL(url.utf8());
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
  AvailabilityStatus* status = status_it->second.get();
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
  AvailabilityStatus* status = status_it->second.get();
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

  if (!session_info.is_null()) {
    presentation_service_->ListenForSessionMessages(session_info.Clone());
    controller_->didStartDefaultSession(
        new PresentationConnectionClient(std::move(session_info)));
  }
}

void PresentationDispatcher::OnSessionCreated(
    blink::WebPresentationConnectionClientCallbacks* callback,
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
  presentation_service_->ListenForSessionMessages(session_info.Clone());
  callback->onSuccess(blink::adoptWebPtr(
      new PresentationConnectionClient(std::move(session_info))));
}

void PresentationDispatcher::OnConnectionStateChanged(
    presentation::PresentationSessionInfoPtr connection,
    presentation::PresentationConnectionState state) {
  if (!controller_)
    return;

  DCHECK(!connection.is_null());
  controller_->didChangeSessionState(
      new PresentationConnectionClient(std::move(connection)),
      GetWebPresentationConnectionStateFromMojo(state));
}

void PresentationDispatcher::OnSessionMessagesReceived(
    presentation::PresentationSessionInfoPtr session_info,
    mojo::Array<presentation::SessionMessagePtr> messages) {
  if (!controller_)
    return;

  for (size_t i = 0; i < messages.size(); ++i) {
    // Note: Passing batches of messages to the Blink layer would be more
    // efficient.
    scoped_ptr<PresentationConnectionClient> session_client(
        new PresentationConnectionClient(session_info->url, session_info->id));
    switch (messages[i]->type) {
      case presentation::PresentationMessageType::TEXT: {
        controller_->didReceiveSessionTextMessage(
            session_client.release(),
            blink::WebString::fromUTF8(messages[i]->message));
        break;
      }
      case presentation::PresentationMessageType::ARRAY_BUFFER:
      case presentation::PresentationMessageType::BLOB: {
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
  presentation_service_->SetClient(binding_.CreateInterfacePtrAndBind());
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
    : session_info(std::move(session_info)), message(std::move(message)) {}

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
  session_message->type = presentation::PresentationMessageType::TEXT;
  session_message->message = message.utf8();
  return new SendMessageRequest(std::move(session_info),
                                std::move(session_message));
}

// static
PresentationDispatcher::SendMessageRequest*
PresentationDispatcher::CreateSendBinaryMessageRequest(
    const blink::WebString& presentationUrl,
    const blink::WebString& presentationId,
    presentation::PresentationMessageType type,
    const uint8_t* data,
    size_t length) {
  presentation::PresentationSessionInfoPtr session_info =
      presentation::PresentationSessionInfo::New();
  session_info->url = presentationUrl.utf8();
  session_info->id = presentationId.utf8();

  presentation::SessionMessagePtr session_message =
      presentation::SessionMessage::New();
  session_message->type = type;
  std::vector<uint8_t> tmp_data_vector(data, data + length);
  session_message->data.Swap(&tmp_data_vector);
  return new SendMessageRequest(std::move(session_info),
                                std::move(session_message));
}

PresentationDispatcher::AvailabilityStatus::AvailabilityStatus(
    const std::string& availability_url)
    : url(availability_url),
      last_known_availability(false),
      listening_state(ListeningState::INACTIVE) {}

PresentationDispatcher::AvailabilityStatus::~AvailabilityStatus() {
}

}  // namespace content
