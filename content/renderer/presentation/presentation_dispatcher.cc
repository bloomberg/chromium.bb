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
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/presentation_constants.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationError.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationReceiver.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationSessionInfo.h"
#include "third_party/WebKit/public/platform/modules/presentation/presentation.mojom.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace mojo {

// Temporary type converter since Presentation API has not been Onion Soup-ed.
template <>
struct TypeConverter<blink::WebPresentationSessionInfo,
                     blink::mojom::PresentationSessionInfoPtr> {
  static blink::WebPresentationSessionInfo Convert(
      const blink::mojom::PresentationSessionInfoPtr& input) {
    return blink::WebPresentationSessionInfo(
        blink::WebURL(input->url), blink::WebString::fromUTF8(input->id));
  }
};

}  // namespace mojo

namespace {

blink::WebPresentationError::ErrorType GetWebPresentationErrorTypeFromMojo(
    blink::mojom::PresentationErrorType mojoErrorType) {
  switch (mojoErrorType) {
    case blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS:
      return blink::WebPresentationError::ErrorTypeNoAvailableScreens;
    case blink::mojom::PresentationErrorType::SESSION_REQUEST_CANCELLED:
      return blink::WebPresentationError::ErrorTypeSessionRequestCancelled;
    case blink::mojom::PresentationErrorType::NO_PRESENTATION_FOUND:
      return blink::WebPresentationError::ErrorTypeNoPresentationFound;
    case blink::mojom::PresentationErrorType::PREVIOUS_START_IN_PROGRESS:
      return blink::WebPresentationError::ErrorTypePreviousStartInProgress;
    case blink::mojom::PresentationErrorType::UNKNOWN:
    default:
      return blink::WebPresentationError::ErrorTypeUnknown;
  }
}

blink::WebPresentationConnectionState GetWebPresentationConnectionStateFromMojo(
    blink::mojom::PresentationConnectionState mojoSessionState) {
  switch (mojoSessionState) {
    case blink::mojom::PresentationConnectionState::CONNECTING:
      return blink::WebPresentationConnectionState::Connecting;
    case blink::mojom::PresentationConnectionState::CONNECTED:
      return blink::WebPresentationConnectionState::Connected;
    case blink::mojom::PresentationConnectionState::CLOSED:
      return blink::WebPresentationConnectionState::Closed;
    case blink::mojom::PresentationConnectionState::TERMINATED:
      return blink::WebPresentationConnectionState::Terminated;
    default:
      NOTREACHED();
      return blink::WebPresentationConnectionState::Terminated;
  }
}

blink::WebPresentationConnectionCloseReason
GetWebPresentationConnectionCloseReasonFromMojo(
    blink::mojom::PresentationConnectionCloseReason mojoConnectionCloseReason) {
  switch (mojoConnectionCloseReason) {
    case blink::mojom::PresentationConnectionCloseReason::CONNECTION_ERROR:
      return blink::WebPresentationConnectionCloseReason::Error;
    case blink::mojom::PresentationConnectionCloseReason::CLOSED:
      return blink::WebPresentationConnectionCloseReason::Closed;
    case blink::mojom::PresentationConnectionCloseReason::WENT_AWAY:
      return blink::WebPresentationConnectionCloseReason::WentAway;
    default:
      NOTREACHED();
      return blink::WebPresentationConnectionCloseReason::Error;
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
    const blink::WebVector<blink::WebURL>& presentationUrls,
    std::unique_ptr<blink::WebPresentationConnectionCallback> callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  std::vector<GURL> urls;
  for (const auto& url : presentationUrls)
    urls.push_back(url);

  // The dispatcher owns the service so |this| will be valid when
  // OnSessionCreated() is called. |callback| needs to be alive and also needs
  // to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->StartSession(
      urls, base::Bind(&PresentationDispatcher::OnSessionCreated,
                       base::Unretained(this), base::Passed(&callback)));
}

void PresentationDispatcher::joinSession(
    const blink::WebVector<blink::WebURL>& presentationUrls,
    const blink::WebString& presentationId,
    std::unique_ptr<blink::WebPresentationConnectionCallback> callback) {
  DCHECK(callback);
  ConnectToPresentationServiceIfNeeded();

  std::vector<GURL> urls;
  for (const auto& url : presentationUrls)
    urls.push_back(url);

  // The dispatcher owns the service so |this| will be valid when
  // OnSessionCreated() is called. |callback| needs to be alive and also needs
  // to be destroyed so we transfer its ownership to the mojo callback.
  presentation_service_->JoinSession(
      urls, presentationId.utf8(),
      base::Bind(&PresentationDispatcher::OnSessionCreated,
                 base::Unretained(this), base::Passed(&callback)));
}

void PresentationDispatcher::sendString(const blink::WebURL& presentationUrl,
                                        const blink::WebString& presentationId,
                                        const blink::WebString& message) {
  if (message.utf8().size() > kMaxPresentationConnectionMessageSize) {
    // TODO(crbug.com/459008): Limit the size of individual messages to 64k
    // for now. Consider throwing DOMException or splitting bigger messages
    // into smaller chunks later.
    LOG(WARNING) << "message size exceeded limit!";
    return;
  }

  message_request_queue_.push(base::WrapUnique(
      CreateSendTextMessageRequest(presentationUrl, presentationId, message)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::sendArrayBuffer(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId,
    const uint8_t* data,
    size_t length) {
  DCHECK(data);
  if (length > kMaxPresentationConnectionMessageSize) {
    // TODO(crbug.com/459008): Same as in sendString().
    LOG(WARNING) << "data size exceeded limit!";
    return;
  }

  message_request_queue_.push(base::WrapUnique(CreateSendBinaryMessageRequest(
      presentationUrl, presentationId,
      blink::mojom::PresentationMessageType::BINARY, data, length)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::sendBlobData(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId,
    const uint8_t* data,
    size_t length) {
  DCHECK(data);
  if (length > kMaxPresentationConnectionMessageSize) {
    // TODO(crbug.com/459008): Same as in sendString().
    LOG(WARNING) << "data size exceeded limit!";
    return;
  }

  message_request_queue_.push(base::WrapUnique(CreateSendBinaryMessageRequest(
      presentationUrl, presentationId,
      blink::mojom::PresentationMessageType::BINARY, data, length)));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::DoSendMessage(SendMessageRequest* request) {
  ConnectToPresentationServiceIfNeeded();

  presentation_service_->SendConnectionMessage(
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
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId) {
  ConnectToPresentationServiceIfNeeded();
  presentation_service_->CloseConnection(presentationUrl,
                                         presentationId.utf8());
}

void PresentationDispatcher::terminateSession(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId) {
  ConnectToPresentationServiceIfNeeded();
  presentation_service_->Terminate(presentationUrl, presentationId.utf8());
}

void PresentationDispatcher::getAvailability(
    const blink::WebVector<blink::WebURL>& availabilityUrls,
    std::unique_ptr<blink::WebPresentationAvailabilityCallbacks> callback) {
  DCHECK(!availabilityUrls.isEmpty());

  std::vector<GURL> urls;
  for (const auto& availability_url : availabilityUrls)
    urls.push_back(availability_url);

  auto screen_availability = GetScreenAvailability(urls);
  // Reject Promise if screen availability is unsupported for all URLs.
  if (screen_availability == ScreenAvailability::UNSUPPORTED) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            &blink::WebPresentationAvailabilityCallbacks::onError,
            base::Passed(&callback),
            blink::WebPresentationError(
                blink::WebPresentationError::ErrorTypeAvailabilityNotSupported,
                "Screen availability monitoring not supported")));
    // Do not listen to urls if we reject the promise.
    return;
  }

  auto* listener = GetAvailabilityListener(urls);
  if (!listener) {
    listener = new AvailabilityListener(urls);
    availability_set_.insert(base::WrapUnique(listener));
  }

  if (screen_availability != ScreenAvailability::UNKNOWN) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&blink::WebPresentationAvailabilityCallbacks::onSuccess,
                   base::Passed(&callback),
                   screen_availability == ScreenAvailability::AVAILABLE));
  } else {
    listener->availability_callbacks.Add(std::move(callback));
  }

  for (const auto& availabilityUrl : urls)
    StartListeningToURL(availabilityUrl);
}

void PresentationDispatcher::startListening(
    blink::WebPresentationAvailabilityObserver* observer) {
  std::vector<GURL> urls;
  for (const auto& url : observer->urls())
    urls.push_back(url);

  auto* listener = GetAvailabilityListener(urls);
  if (!listener) {
    listener = new AvailabilityListener(urls);
    availability_set_.insert(base::WrapUnique(listener));
  }

  listener->availability_observers.insert(observer);
  for (const auto& availabilityUrl : urls)
    StartListeningToURL(availabilityUrl);
}

void PresentationDispatcher::stopListening(
    blink::WebPresentationAvailabilityObserver* observer) {
  std::vector<GURL> urls;
  for (const auto& url : observer->urls())
    urls.push_back(url);

  auto* listener = GetAvailabilityListener(urls);
  if (!listener) {
    DLOG(WARNING) << "Stop listening for availability for unknown URLs.";
    return;
  }

  listener->availability_observers.erase(observer);
  for (const auto& availabilityUrl : urls)
    MaybeStopListeningToURL(availabilityUrl);

  TryRemoveAvailabilityListener(listener);
}

void PresentationDispatcher::setDefaultPresentationUrls(
    const blink::WebVector<blink::WebURL>& presentationUrls) {
  ConnectToPresentationServiceIfNeeded();

  std::vector<GURL> urls;
  for (const auto& url : presentationUrls)
    urls.push_back(url);

  presentation_service_->SetDefaultPresentationUrls(urls);
}

void PresentationDispatcher::setReceiver(
    blink::WebPresentationReceiver* receiver) {
  ConnectToPresentationServiceIfNeeded();
  receiver_ = receiver;
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

void PresentationDispatcher::OnDestruct() {
  delete this;
}

void PresentationDispatcher::OnScreenAvailabilityUpdated(const GURL& url,
                                                         bool available) {
  auto* listening_status = GetListeningStatus(url);
  if (!listening_status)
    return;

  if (listening_status->listening_state == ListeningState::WAITING)
    listening_status->listening_state = ListeningState::ACTIVE;

  auto new_screen_availability = available ? ScreenAvailability::AVAILABLE
                                           : ScreenAvailability::UNAVAILABLE;
  if (listening_status->last_known_availability == new_screen_availability)
    return;

  listening_status->last_known_availability = new_screen_availability;

  std::set<AvailabilityListener*> modified_listeners;
  for (auto& listener : availability_set_) {
    if (!base::ContainsValue(listener->urls, url))
      continue;

    auto screen_availability = GetScreenAvailability(listener->urls);
    DCHECK(screen_availability == ScreenAvailability::AVAILABLE ||
           screen_availability == ScreenAvailability::UNAVAILABLE);
    bool is_available = (screen_availability == ScreenAvailability::AVAILABLE);

    for (auto* observer : listener->availability_observers)
      observer->availabilityChanged(is_available);

    for (AvailabilityCallbacksMap::iterator iter(
             &listener->availability_callbacks);
         !iter.IsAtEnd(); iter.Advance()) {
      iter.GetCurrentValue()->onSuccess(is_available);
    }
    listener->availability_callbacks.Clear();

    for (const auto& availabilityUrl : listener->urls)
      MaybeStopListeningToURL(availabilityUrl);

    modified_listeners.insert(listener.get());
  }

  for (auto* listener : modified_listeners)
    TryRemoveAvailabilityListener(listener);
}

void PresentationDispatcher::OnScreenAvailabilityNotSupported(const GURL& url) {
  auto* listening_status = GetListeningStatus(url);
  if (!listening_status)
    return;

  if (listening_status->listening_state == ListeningState::WAITING)
    listening_status->listening_state = ListeningState::ACTIVE;

  if (listening_status->last_known_availability ==
      ScreenAvailability::UNSUPPORTED) {
    return;
  }

  listening_status->last_known_availability = ScreenAvailability::UNSUPPORTED;

  const blink::WebString& not_supported_error = blink::WebString::fromUTF8(
      "getAvailability() isn't supported at the moment. It can be due to "
      "a permanent or temporary system limitation. It is recommended to "
      "try to blindly start a session in that case.");

  std::set<AvailabilityListener*> modified_listeners;
  for (auto& listener : availability_set_) {
    if (!base::ContainsValue(listener->urls, url))
      continue;

    // ScreenAvailabilityNotSupported should be a browser side setting, which
    // means all urls in PresentationAvailability should report NotSupported.
    // It is not possible to change listening status from Available or
    // Unavailable to NotSupported. No need to update observer.
    auto screen_availability = GetScreenAvailability(listener->urls);
    DCHECK_EQ(screen_availability, ScreenAvailability::UNSUPPORTED);

    for (AvailabilityCallbacksMap::iterator iter(
             &listener->availability_callbacks);
         !iter.IsAtEnd(); iter.Advance()) {
      iter.GetCurrentValue()->onError(blink::WebPresentationError(
          blink::WebPresentationError::ErrorTypeAvailabilityNotSupported,
          not_supported_error));
    }
    listener->availability_callbacks.Clear();

    for (const auto& availability_url : listener->urls)
      MaybeStopListeningToURL(availability_url);

    modified_listeners.insert(listener.get());
  }

  for (auto* listener : modified_listeners)
    TryRemoveAvailabilityListener(listener);
}

void PresentationDispatcher::OnDefaultSessionStarted(
    blink::mojom::PresentationSessionInfoPtr session_info) {
  if (!controller_)
    return;

  if (!session_info.is_null()) {
    presentation_service_->ListenForConnectionMessages(session_info.Clone());
    controller_->didStartDefaultSession(
        mojo::ConvertTo<blink::WebPresentationSessionInfo>(session_info));
  }
}

void PresentationDispatcher::OnSessionCreated(
    std::unique_ptr<blink::WebPresentationConnectionCallback> callback,
    blink::mojom::PresentationSessionInfoPtr session_info,
    blink::mojom::PresentationErrorPtr error) {
  DCHECK(callback);
  if (!error.is_null()) {
    DCHECK(session_info.is_null());
    callback->onError(blink::WebPresentationError(
        GetWebPresentationErrorTypeFromMojo(error->error_type),
        blink::WebString::fromUTF8(error->message)));
    return;
  }

  DCHECK(!session_info.is_null());
  presentation_service_->ListenForConnectionMessages(session_info.Clone());
  callback->onSuccess(
      mojo::ConvertTo<blink::WebPresentationSessionInfo>(session_info));
}

void PresentationDispatcher::OnReceiverConnectionAvailable(
    blink::mojom::PresentationSessionInfoPtr session_info,
    blink::mojom::PresentationConnectionPtr,
    blink::mojom::PresentationConnectionRequest) {
  if (receiver_) {
    receiver_->onReceiverConnectionAvailable(
        mojo::ConvertTo<blink::WebPresentationSessionInfo>(session_info));
  }
}

void PresentationDispatcher::OnConnectionStateChanged(
    blink::mojom::PresentationSessionInfoPtr session_info,
    blink::mojom::PresentationConnectionState state) {
  if (!controller_)
    return;

  controller_->didChangeSessionState(
      mojo::ConvertTo<blink::WebPresentationSessionInfo>(session_info),
      GetWebPresentationConnectionStateFromMojo(state));
}

void PresentationDispatcher::OnConnectionClosed(
    blink::mojom::PresentationSessionInfoPtr session_info,
    blink::mojom::PresentationConnectionCloseReason reason,
    const std::string& message) {
  if (!controller_)
    return;

  controller_->didCloseConnection(
      mojo::ConvertTo<blink::WebPresentationSessionInfo>(session_info),
      GetWebPresentationConnectionCloseReasonFromMojo(reason),
      blink::WebString::fromUTF8(message));
}

void PresentationDispatcher::OnConnectionMessagesReceived(
    blink::mojom::PresentationSessionInfoPtr session_info,
    std::vector<blink::mojom::ConnectionMessagePtr> messages) {
  if (!controller_)
    return;

  for (size_t i = 0; i < messages.size(); ++i) {
    // Note: Passing batches of messages to the Blink layer would be more
    // efficient.
    auto web_session_info =
        mojo::ConvertTo<blink::WebPresentationSessionInfo>(session_info);
    switch (messages[i]->type) {
      case blink::mojom::PresentationMessageType::TEXT: {
        // TODO(mfoltz): Do we need to DCHECK(messages[i]->message)?
        controller_->didReceiveSessionTextMessage(
            web_session_info,
            blink::WebString::fromUTF8(messages[i]->message.value()));
        break;
      }
      case blink::mojom::PresentationMessageType::BINARY: {
        // TODO(mfoltz): Do we need to DCHECK(messages[i]->data)?
        controller_->didReceiveSessionBinaryMessage(
            web_session_info, &(messages[i]->data->front()),
            messages[i]->data->size());
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

  render_frame()->GetRemoteInterfaces()->GetInterface(&presentation_service_);
  presentation_service_->SetClient(binding_.CreateInterfacePtrAndBind());
}

void PresentationDispatcher::StartListeningToURL(const GURL& url) {
  auto* listening_status = GetListeningStatus(url);
  if (!listening_status) {
    listening_status = new ListeningStatus(url);
    listening_status_.insert(
        std::make_pair(url, base::WrapUnique(listening_status)));
  }

  // Already listening.
  if (listening_status->listening_state != ListeningState::INACTIVE)
    return;

  ConnectToPresentationServiceIfNeeded();
  listening_status->listening_state = ListeningState::WAITING;
  presentation_service_->ListenForScreenAvailability(url);
}

void PresentationDispatcher::MaybeStopListeningToURL(const GURL& url) {
  for (const auto& listener : availability_set_) {
    if (!base::ContainsValue(listener->urls, url))
      continue;

    // URL is still observed by some availability object.
    if (!listener->availability_callbacks.IsEmpty() ||
        !listener->availability_observers.empty()) {
      return;
    }
  }

  auto* listening_status = GetListeningStatus(url);
  if (!listening_status) {
    LOG(WARNING) << "Stop listening to unknown url: " << url;
    return;
  }

  if (listening_status->listening_state == ListeningState::INACTIVE)
    return;

  ConnectToPresentationServiceIfNeeded();
  listening_status->listening_state = ListeningState::INACTIVE;
  presentation_service_->StopListeningForScreenAvailability(url);
}

PresentationDispatcher::ListeningStatus*
PresentationDispatcher::GetListeningStatus(const GURL& url) const {
  auto status_it = listening_status_.find(url);
  return status_it == listening_status_.end() ? nullptr
                                              : status_it->second.get();
}

PresentationDispatcher::AvailabilityListener*
PresentationDispatcher::GetAvailabilityListener(
    const std::vector<GURL>& urls) const {
  auto listener_it =
      std::find_if(availability_set_.begin(), availability_set_.end(),
                   [&urls](const std::unique_ptr<AvailabilityListener>& x) {
                     return x->urls == urls;
                   });
  return listener_it == availability_set_.end() ? nullptr : listener_it->get();
}

void PresentationDispatcher::TryRemoveAvailabilityListener(
    AvailabilityListener* listener) {
  // URL is still observed by some availability object.
  if (!listener->availability_callbacks.IsEmpty() ||
      !listener->availability_observers.empty()) {
    return;
  }

  auto listener_it = availability_set_.begin();
  while (listener_it != availability_set_.end()) {
    if (listener_it->get() == listener) {
      availability_set_.erase(listener_it);
      return;
    } else {
      ++listener_it;
    }
  }
}

// Given a screen availability vector and integer value for each availability:
// UNKNOWN = 0, UNAVAILABLE = 1, UNSUPPORTED = 2, and AVAILABLE = 3, the max
// value of the vector is the overall availability.
PresentationDispatcher::ScreenAvailability
PresentationDispatcher::GetScreenAvailability(
    const std::vector<GURL>& urls) const {
  int current_availability = 0;  // UNKNOWN;

  for (const auto& url : urls) {
    auto* status = GetListeningStatus(url);
    auto screen_availability =
        status ? status->last_known_availability : ScreenAvailability::UNKNOWN;
    current_availability =
        std::max(current_availability, static_cast<int>(screen_availability));
  }

  return static_cast<ScreenAvailability>(current_availability);
}

PresentationDispatcher::SendMessageRequest::SendMessageRequest(
    blink::mojom::PresentationSessionInfoPtr session_info,
    blink::mojom::ConnectionMessagePtr message)
    : session_info(std::move(session_info)), message(std::move(message)) {}

PresentationDispatcher::SendMessageRequest::~SendMessageRequest() {}

// static
PresentationDispatcher::SendMessageRequest*
PresentationDispatcher::CreateSendTextMessageRequest(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId,
    const blink::WebString& message) {
  blink::mojom::PresentationSessionInfoPtr session_info =
      blink::mojom::PresentationSessionInfo::New();
  session_info->url = presentationUrl;
  session_info->id = presentationId.utf8();

  blink::mojom::ConnectionMessagePtr session_message =
      blink::mojom::ConnectionMessage::New();
  session_message->type = blink::mojom::PresentationMessageType::TEXT;
  session_message->message = message.utf8();
  return new SendMessageRequest(std::move(session_info),
                                std::move(session_message));
}

// static
PresentationDispatcher::SendMessageRequest*
PresentationDispatcher::CreateSendBinaryMessageRequest(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId,
    blink::mojom::PresentationMessageType type,
    const uint8_t* data,
    size_t length) {
  blink::mojom::PresentationSessionInfoPtr session_info =
      blink::mojom::PresentationSessionInfo::New();
  session_info->url = presentationUrl;
  session_info->id = presentationId.utf8();

  blink::mojom::ConnectionMessagePtr session_message =
      blink::mojom::ConnectionMessage::New();
  session_message->type = type;
  session_message->data = std::vector<uint8_t>(data, data + length);
  return new SendMessageRequest(std::move(session_info),
                                std::move(session_message));
}

PresentationDispatcher::AvailabilityListener::AvailabilityListener(
    const std::vector<GURL>& availability_urls)
    : urls(availability_urls) {}

PresentationDispatcher::AvailabilityListener::~AvailabilityListener() {}

PresentationDispatcher::ListeningStatus::ListeningStatus(
    const GURL& availability_url)
    : url(availability_url),
      last_known_availability(ScreenAvailability::UNKNOWN),
      listening_state(ListeningState::INACTIVE) {}

PresentationDispatcher::ListeningStatus::~ListeningStatus() {}

}  // namespace content
