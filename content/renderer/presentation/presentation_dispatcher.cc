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
#include "content/public/common/presentation_connection_message.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/presentation/presentation_connection_proxy.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnection.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnectionCallbacks.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
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

blink::WebPresentationConnectionState GetWebPresentationConnectionState(
    PresentationConnectionState connectionState) {
  switch (connectionState) {
    case PresentationConnectionState::PRESENTATION_CONNECTION_STATE_CONNECTING:
      return blink::WebPresentationConnectionState::kConnecting;
    case PresentationConnectionState::PRESENTATION_CONNECTION_STATE_CONNECTED:
      return blink::WebPresentationConnectionState::kConnected;
    case PresentationConnectionState::PRESENTATION_CONNECTION_STATE_CLOSED:
      return blink::WebPresentationConnectionState::kClosed;
    case PresentationConnectionState::PRESENTATION_CONNECTION_STATE_TERMINATED:
      return blink::WebPresentationConnectionState::kTerminated;
    default:
      NOTREACHED();
      return blink::WebPresentationConnectionState::kTerminated;
  }
}

blink::WebPresentationConnectionCloseReason
GetWebPresentationConnectionCloseReason(
    PresentationConnectionCloseReason connectionCloseReason) {
  switch (connectionCloseReason) {
    case PresentationConnectionCloseReason::
        PRESENTATION_CONNECTION_CLOSE_REASON_CONNECTION_ERROR:
      return blink::WebPresentationConnectionCloseReason::kError;
    case PresentationConnectionCloseReason::
        PRESENTATION_CONNECTION_CLOSE_REASON_CLOSED:
      return blink::WebPresentationConnectionCloseReason::kClosed;
    case PresentationConnectionCloseReason::
        PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY:
      return blink::WebPresentationConnectionCloseReason::kWentAway;
    default:
      NOTREACHED();
      return blink::WebPresentationConnectionCloseReason::kError;
  }
}
}  // namespace

// TODO(mfoltz): Reorder definitions to match presentation_dispatcher.h.

PresentationDispatcher::PresentationDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      controller_(nullptr),
      receiver_(nullptr),
      binding_(this) {}

PresentationDispatcher::~PresentationDispatcher() {
  // Controller should be destroyed before the dispatcher when frame is
  // destroyed.
  DCHECK(!controller_);
}

void PresentationDispatcher::SetController(
    blink::WebPresentationController* controller) {
  // There shouldn't be any swapping from one non-null controller to another.
  DCHECK(controller != controller_ && (!controller || !controller_));
  controller_ = controller;
  // The controller is set to null when the frame is about to be detached.
  // Nothing is listening for screen availability anymore but the Mojo service
  // will know about the frame being detached anyway.
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
      urls, base::Bind(&PresentationDispatcher::OnConnectionCreated,
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
      base::Bind(&PresentationDispatcher::OnConnectionCreated,
                 base::Unretained(this), base::Passed(&callback)));
}

void PresentationDispatcher::SendString(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId,
    const blink::WebString& message,
    const blink::WebPresentationConnectionProxy* connection_proxy) {
  if (message.Utf8().size() > kMaxPresentationConnectionMessageSize) {
    // TODO(crbug.com/459008): Limit the size of individual messages to 64k
    // for now. Consider throwing DOMException or splitting bigger messages
    // into smaller chunks later.
    LOG(WARNING) << "message size exceeded limit!";
    return;
  }

  message_request_queue_.push_back(CreateSendTextMessageRequest(
      presentationUrl, presentationId, message, connection_proxy));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::TerminatePresentation(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId) {
  if (receiver_) {
    receiver_->TerminateConnection();
    return;
  }

  ConnectToPresentationServiceIfNeeded();
  presentation_service_->Terminate(presentationUrl, presentationId.Utf8());
}

void PresentationDispatcher::SendArrayBuffer(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId,
    const uint8_t* data,
    size_t length,
    const blink::WebPresentationConnectionProxy* connection_proxy) {
  DCHECK(data);
  if (length > kMaxPresentationConnectionMessageSize) {
    // TODO(crbug.com/459008): Same as in sendString().
    LOG(WARNING) << "data size exceeded limit!";
    return;
  }

  message_request_queue_.push_back(CreateSendBinaryMessageRequest(
      presentationUrl, presentationId, data, length, connection_proxy));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::SendBlobData(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId,
    const uint8_t* data,
    size_t length,
    const blink::WebPresentationConnectionProxy* connection_proxy) {
  DCHECK(data);
  if (length > kMaxPresentationConnectionMessageSize) {
    // TODO(crbug.com/459008): Same as in sendString().
    LOG(WARNING) << "data size exceeded limit!";
    return;
  }

  message_request_queue_.push_back(CreateSendBinaryMessageRequest(
      presentationUrl, presentationId, data, length, connection_proxy));
  // Start processing request if only one in the queue.
  if (message_request_queue_.size() == 1)
    DoSendMessage(message_request_queue_.front().get());
}

void PresentationDispatcher::CloseConnection(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId,
    const blink::WebPresentationConnectionProxy* connection_proxy) {
  message_request_queue_.erase(
      std::remove_if(message_request_queue_.begin(),
                     message_request_queue_.end(),
                     [&connection_proxy](
                         const std::unique_ptr<SendMessageRequest>& request) {
                       return request->connection_proxy == connection_proxy;
                     }),
      message_request_queue_.end());

  connection_proxy->Close();

  ConnectToPresentationServiceIfNeeded();
  presentation_service_->CloseConnection(presentationUrl,
                                         presentationId.Utf8());
}

void PresentationDispatcher::DoSendMessage(SendMessageRequest* request) {
  DCHECK(request->connection_proxy);
  // TODO(crbug.com/684116): Remove static_cast after moving message queue logic
  // from PresentationDispatcher to PresentationConnectionProxy.
  static_cast<const PresentationConnectionProxy*>(request->connection_proxy)
      ->SendConnectionMessage(
          std::move(request->message),
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

  message_request_queue_.pop_front();
  if (!message_request_queue_.empty()) {
    DoSendMessage(message_request_queue_.front().get());
  }
}

void PresentationDispatcher::SetControllerConnection(
    const PresentationInfo& presentation_info,
    blink::WebPresentationConnection* connection) {
  DCHECK(connection);

  auto* controller_connection_proxy = new ControllerConnectionProxy(connection);
  connection->BindProxy(base::WrapUnique(controller_connection_proxy));

  ConnectToPresentationServiceIfNeeded();
  presentation_service_->SetPresentationConnection(
      presentation_info, controller_connection_proxy->Bind(),
      controller_connection_proxy->MakeRemoteRequest());
}

void PresentationDispatcher::GetAvailability(
    const blink::WebVector<blink::WebURL>& availabilityUrls,
    std::unique_ptr<blink::WebPresentationAvailabilityCallbacks> callback) {
  DCHECK(!availabilityUrls.IsEmpty());

  std::vector<GURL> urls;
  for (const auto& availability_url : availabilityUrls)
    urls.push_back(availability_url);

  auto screen_availability = GetScreenAvailability(urls);
  // Reject Promise if screen availability is unsupported for all URLs.
  if (screen_availability == blink::mojom::ScreenAvailability::DISABLED) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            &blink::WebPresentationAvailabilityCallbacks::OnError,
            base::Passed(&callback),
            blink::WebPresentationError(
                blink::WebPresentationError::kErrorTypeAvailabilityNotSupported,
                "Screen availability monitoring not supported")));
    // Do not listen to urls if we reject the promise.
    return;
  }

  auto* listener = GetAvailabilityListener(urls);
  if (!listener) {
    listener = new AvailabilityListener(urls);
    availability_set_.insert(base::WrapUnique(listener));
  }

  if (screen_availability != blink::mojom::ScreenAvailability::UNKNOWN) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&blink::WebPresentationAvailabilityCallbacks::OnSuccess,
                   base::Passed(&callback),
                   screen_availability ==
                       blink::mojom::ScreenAvailability::AVAILABLE));
  } else {
    listener->availability_callbacks.Add(std::move(callback));
  }

  for (const auto& availabilityUrl : urls)
    StartListeningToURL(availabilityUrl);
}

void PresentationDispatcher::StartListening(
    blink::WebPresentationAvailabilityObserver* observer) {
  std::vector<GURL> urls;
  for (const auto& url : observer->Urls())
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

void PresentationDispatcher::StopListening(
    blink::WebPresentationAvailabilityObserver* observer) {
  std::vector<GURL> urls;
  for (const auto& url : observer->Urls())
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

void PresentationDispatcher::SetDefaultPresentationUrls(
    const blink::WebVector<blink::WebURL>& presentationUrls) {
  ConnectToPresentationServiceIfNeeded();

  std::vector<GURL> urls;
  for (const auto& url : presentationUrls)
    urls.push_back(url);

  presentation_service_->SetDefaultPresentationUrls(urls);
}

void PresentationDispatcher::SetReceiver(
    blink::WebPresentationReceiver* receiver) {
  receiver_ = receiver;

  // Create receiver PSImpl after loading document.
  if (render_frame() && render_frame()->GetWebFrame() &&
      !render_frame()->GetWebFrame()->IsLoading()) {
    ConnectToPresentationServiceIfNeeded();
  }
}

void PresentationDispatcher::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_document_navigation) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  // If not top-level navigation.
  if (frame->Parent() || is_same_document_navigation)
    return;

  // Remove all pending send message requests.
  MessageRequestQueue empty;
  std::swap(message_request_queue_, empty);
}

void PresentationDispatcher::DidFinishDocumentLoad() {
  if (receiver_)
    ConnectToPresentationServiceIfNeeded();
}

void PresentationDispatcher::OnDestruct() {
  delete this;
}

void PresentationDispatcher::WidgetWillClose() {
  if (!receiver_)
    return;

  receiver_->DidChangeConnectionState(
      blink::WebPresentationConnectionState::kTerminated);
}

void PresentationDispatcher::OnScreenAvailabilityUpdated(
    const GURL& url,
    blink::mojom::ScreenAvailability availability) {
  auto* listening_status = GetListeningStatus(url);
  if (!listening_status)
    return;

  if (listening_status->listening_state == ListeningState::WAITING)
    listening_status->listening_state = ListeningState::ACTIVE;

  if (listening_status->last_known_availability == availability)
    return;

  listening_status->last_known_availability = availability;

  static const blink::WebString& not_supported_error =
      blink::WebString::FromUTF8(
          "getAvailability() isn't supported at the moment. It can be due to "
          "a permanent or temporary system limitation. It is recommended to "
          "try to blindly start a presentation in that case.");

  std::set<AvailabilityListener*> modified_listeners;
  for (auto& listener : availability_set_) {
    if (!base::ContainsValue(listener->urls, url))
      continue;

    auto screen_availability = GetScreenAvailability(listener->urls);
    DCHECK(screen_availability != blink::mojom::ScreenAvailability::UNKNOWN);
    for (auto* observer : listener->availability_observers)
      observer->AvailabilityChanged(screen_availability);

    for (AvailabilityCallbacksMap::iterator iter(
             &listener->availability_callbacks);
         !iter.IsAtEnd(); iter.Advance()) {
      if (screen_availability == blink::mojom::ScreenAvailability::DISABLED) {
        iter.GetCurrentValue()->OnError(blink::WebPresentationError(
            blink::WebPresentationError::kErrorTypeAvailabilityNotSupported,
            not_supported_error));
      } else {
        iter.GetCurrentValue()->OnSuccess(
            screen_availability == blink::mojom::ScreenAvailability::AVAILABLE);
      }
    }
    listener->availability_callbacks.Clear();

    for (const auto& availabilityUrl : listener->urls)
      MaybeStopListeningToURL(availabilityUrl);

    modified_listeners.insert(listener.get());
  }

  for (auto* listener : modified_listeners)
    TryRemoveAvailabilityListener(listener);
}

void PresentationDispatcher::OnDefaultPresentationStarted(
    const PresentationInfo& presentation_info) {
  if (!controller_)
    return;

  auto* connection =
      controller_->DidStartDefaultPresentation(blink::WebPresentationInfo(
          presentation_info.presentation_url,
          blink::WebString::FromUTF8(presentation_info.presentation_id)));

  if (connection) {
    SetControllerConnection(presentation_info, connection);
    // Change blink connection state to 'connected' before listening to
    // connection message. Remove ListenForConnectionMessage() after
    // TODO(crbug.com/687011): use BrowserPresentationConnectionProxy to send
    // message from route to blink connection.
    presentation_service_->ListenForConnectionMessages(presentation_info);
  }
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
  // Change blink connection state to 'connected' before listening to
  // connection message. Remove ListenForConnectionMessage() after
  // TODO(crbug.com/687011): use BrowserPresentationConnectionProxy to send
  // message from route to blink connection.
  SetControllerConnection(presentation_info.value(), callback->GetConnection());
  presentation_service_->ListenForConnectionMessages(presentation_info.value());
}

void PresentationDispatcher::OnReceiverConnectionAvailable(
    const PresentationInfo& presentation_info,
    blink::mojom::PresentationConnectionPtr controller_connection_ptr,
    blink::mojom::PresentationConnectionRequest receiver_connection_request) {
  DCHECK(receiver_);

  // Bind receiver_connection_proxy with PresentationConnection in receiver
  // page.
  auto* connection =
      receiver_->OnReceiverConnectionAvailable(blink::WebPresentationInfo(
          presentation_info.presentation_url,
          blink::WebString::FromUTF8(presentation_info.presentation_id)));
  auto* receiver_connection_proxy =
      new ReceiverConnectionProxy(connection, receiver_);
  connection->BindProxy(base::WrapUnique(receiver_connection_proxy));

  receiver_connection_proxy->Bind(std::move(receiver_connection_request));
  receiver_connection_proxy->BindControllerConnection(
      std::move(controller_connection_ptr));
}

void PresentationDispatcher::OnConnectionStateChanged(
    const PresentationInfo& presentation_info,
    PresentationConnectionState state) {
  if (!controller_)
    return;

  controller_->DidChangeConnectionState(
      blink::WebPresentationInfo(
          presentation_info.presentation_url,
          blink::WebString::FromUTF8(presentation_info.presentation_id)),
      GetWebPresentationConnectionState(state));
}

void PresentationDispatcher::OnConnectionClosed(
    const PresentationInfo& presentation_info,
    PresentationConnectionCloseReason reason,
    const std::string& message) {
  if (!controller_)
    return;

  controller_->DidCloseConnection(
      blink::WebPresentationInfo(
          presentation_info.presentation_url,
          blink::WebString::FromUTF8(presentation_info.presentation_id)),
      GetWebPresentationConnectionCloseReason(reason),
      blink::WebString::FromUTF8(message));
}

void PresentationDispatcher::OnConnectionMessagesReceived(
    const PresentationInfo& presentation_info,
    std::vector<PresentationConnectionMessage> messages) {
  if (!controller_)
    return;

  for (size_t i = 0; i < messages.size(); ++i) {
    // Note: Passing batches of messages to the Blink layer would be more
    // efficient.
    auto web_presentation_info = blink::WebPresentationInfo(
        presentation_info.presentation_url,
        blink::WebString::FromUTF8(presentation_info.presentation_id));

    if (messages[i].is_binary()) {
      controller_->DidReceiveConnectionBinaryMessage(
          web_presentation_info, &(messages[i].data->front()),
          messages[i].data->size());
    } else {
      DCHECK(messages[i].message);
      controller_->DidReceiveConnectionTextMessage(
          web_presentation_info,
          blink::WebString::FromUTF8(*messages[i].message));
    }
  }
}

void PresentationDispatcher::ConnectToPresentationServiceIfNeeded() {
  if (presentation_service_.get())
    return;

  render_frame()->GetRemoteInterfaces()->GetInterface(&presentation_service_);
  blink::mojom::PresentationServiceClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  presentation_service_->SetClient(std::move(client));
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

blink::mojom::ScreenAvailability PresentationDispatcher::GetScreenAvailability(
    const std::vector<GURL>& urls) const {
  bool has_disabled = false;
  bool has_source_not_supported = false;
  bool has_unavailable = false;

  for (const auto& url : urls) {
    auto* status = GetListeningStatus(url);
    auto screen_availability = status
                                   ? status->last_known_availability
                                   : blink::mojom::ScreenAvailability::UNKNOWN;
    if (screen_availability == blink::mojom::ScreenAvailability::AVAILABLE) {
      return blink::mojom::ScreenAvailability::AVAILABLE;
    } else if (screen_availability ==
               blink::mojom::ScreenAvailability::DISABLED) {
      has_disabled = true;
    } else if (screen_availability ==
               blink::mojom::ScreenAvailability::SOURCE_NOT_SUPPORTED) {
      has_source_not_supported = true;
    } else if (screen_availability ==
               blink::mojom::ScreenAvailability::UNAVAILABLE) {
      has_unavailable = true;
    }
  }

  if (has_disabled)
    return blink::mojom::ScreenAvailability::DISABLED;
  if (has_source_not_supported)
    return blink::mojom::ScreenAvailability::SOURCE_NOT_SUPPORTED;
  if (has_unavailable)
    return blink::mojom::ScreenAvailability::UNAVAILABLE;
  return blink::mojom::ScreenAvailability::UNKNOWN;
}

PresentationDispatcher::SendMessageRequest::SendMessageRequest(
    const PresentationInfo& presentation_info,
    PresentationConnectionMessage connection_message,
    const blink::WebPresentationConnectionProxy* connection_proxy)
    : presentation_info(presentation_info),
      message(std::move(connection_message)),
      connection_proxy(connection_proxy) {}

PresentationDispatcher::SendMessageRequest::~SendMessageRequest() {}

// static
std::unique_ptr<PresentationDispatcher::SendMessageRequest>
PresentationDispatcher::CreateSendTextMessageRequest(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId,
    const blink::WebString& message,
    const blink::WebPresentationConnectionProxy* connection_proxy) {
  PresentationInfo presentation_info(GURL(presentationUrl),
                                     presentationId.Utf8());

  return base::MakeUnique<SendMessageRequest>(
      presentation_info, PresentationConnectionMessage(message.Utf8()),
      connection_proxy);
}

// static
std::unique_ptr<PresentationDispatcher::SendMessageRequest>
PresentationDispatcher::CreateSendBinaryMessageRequest(
    const blink::WebURL& presentationUrl,
    const blink::WebString& presentationId,
    const uint8_t* data,
    size_t length,
    const blink::WebPresentationConnectionProxy* connection_proxy) {
  PresentationInfo presentation_info(GURL(presentationUrl),
                                     presentationId.Utf8());
  return base::MakeUnique<SendMessageRequest>(
      presentation_info,
      PresentationConnectionMessage(std::vector<uint8_t>(data, data + length)),
      connection_proxy);
}

PresentationDispatcher::AvailabilityListener::AvailabilityListener(
    const std::vector<GURL>& availability_urls)
    : urls(availability_urls) {}

PresentationDispatcher::AvailabilityListener::~AvailabilityListener() {}

PresentationDispatcher::ListeningStatus::ListeningStatus(
    const GURL& availability_url)
    : url(availability_url),
      last_known_availability(blink::mojom::ScreenAvailability::UNKNOWN),
      listening_state(ListeningState::INACTIVE) {}

PresentationDispatcher::ListeningStatus::~ListeningStatus() {}

}  // namespace content
