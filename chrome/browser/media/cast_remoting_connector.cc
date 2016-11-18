// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_connector.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/cast_remoting_connector_messaging.h"
#include "chrome/browser/media/cast_remoting_sender.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/route_message.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

using content::BrowserThread;
using media::mojom::RemotingSinkCapabilities;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;

using Messaging = CastRemotingConnectorMessaging;

class CastRemotingConnector::RemotingBridge : public media::mojom::Remoter {
 public:
  // Constructs a "bridge" to delegate calls between the given |source| and
  // |connector|. |connector| must be valid at the time of construction, but is
  // otherwise a weak pointer that can become invalid during the lifetime of a
  // RemotingBridge.
  RemotingBridge(media::mojom::RemotingSourcePtr source,
                 CastRemotingConnector* connector)
      : source_(std::move(source)), connector_(connector) {
    DCHECK(connector_);
    source_.set_connection_error_handler(base::Bind(
        &RemotingBridge::Stop, base::Unretained(this),
        RemotingStopReason::SOURCE_GONE));
    connector_->RegisterBridge(this);
  }

  ~RemotingBridge() final {
    if (connector_)
      connector_->DeregisterBridge(this, RemotingStopReason::SOURCE_GONE);
  }

  // The CastRemotingConnector calls these to call back to the RemotingSource.
  void OnSinkAvailable(RemotingSinkCapabilities capabilities) {
    source_->OnSinkAvailable(capabilities);
  }
  void OnSinkGone() { source_->OnSinkGone(); }
  void OnStarted() { source_->OnStarted(); }
  void OnStartFailed(RemotingStartFailReason reason) {
    source_->OnStartFailed(reason);
  }
  void OnMessageFromSink(const std::vector<uint8_t>& message) {
    source_->OnMessageFromSink(message);
  }
  void OnStopped(RemotingStopReason reason) { source_->OnStopped(reason); }

  // The CastRemotingConnector calls this when it is no longer valid.
  void OnCastRemotingConnectorDestroyed() {
    connector_ = nullptr;
  }

  // media::mojom::Remoter implementation. The source calls these to start/stop
  // media remoting and send messages to the sink. These simply delegate to the
  // CastRemotingConnector, which mediates to establish only one remoting
  // session among possibly multiple requests. The connector will respond to
  // this request by calling one of: OnStarted() or OnStartFailed().
  void Start() final {
    if (connector_)
      connector_->StartRemoting(this);
  }
  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      media::mojom::RemotingDataStreamSenderRequest audio_sender_request,
      media::mojom::RemotingDataStreamSenderRequest video_sender_request)
      final {
    if (connector_) {
      connector_->StartRemotingDataStreams(
          this, std::move(audio_pipe), std::move(video_pipe),
          std::move(audio_sender_request), std::move(video_sender_request));
    }
  }
  void Stop(RemotingStopReason reason) final {
    if (connector_)
      connector_->StopRemoting(this, reason);
  }
  void SendMessageToSink(const std::vector<uint8_t>& message) final {
    if (connector_)
      connector_->SendMessageToSink(this, message);
  }

 private:
  media::mojom::RemotingSourcePtr source_;

  // Weak pointer. Will be set to nullptr if the CastRemotingConnector is
  // destroyed before this RemotingBridge.
  CastRemotingConnector* connector_;

  DISALLOW_COPY_AND_ASSIGN(RemotingBridge);
};

class CastRemotingConnector::MessageObserver
    : public media_router::RouteMessageObserver {
 public:
  MessageObserver(media_router::MediaRouter* router,
                  const media_router::MediaRoute::Id& route_id,
                  CastRemotingConnector* connector)
      : RouteMessageObserver(router, route_id), connector_(connector) {}
  ~MessageObserver() final {}

 private:
  void OnMessagesReceived(
      const std::vector<media_router::RouteMessage>& messages) final {
    connector_->ProcessMessagesFromRoute(messages);
  }

  CastRemotingConnector* const connector_;
};

// static
const void* const CastRemotingConnector::kUserDataKey = &kUserDataKey;

// static
CastRemotingConnector* CastRemotingConnector::Get(
    content::WebContents* contents) {
  DCHECK(contents);
  CastRemotingConnector* connector =
      static_cast<CastRemotingConnector*>(contents->GetUserData(kUserDataKey));
  if (!connector) {
    connector = new CastRemotingConnector(
        media_router::MediaRouterFactory::GetApiForBrowserContext(
            contents->GetBrowserContext()),
        media_router::MediaSourceForTabContentRemoting(contents).id());
    contents->SetUserData(kUserDataKey, connector);
  }
  return connector;
}

// static
void CastRemotingConnector::CreateMediaRemoter(
    content::RenderFrameHost* host,
    media::mojom::RemotingSourcePtr source,
    media::mojom::RemoterRequest request) {
  DCHECK(host);
  auto* const contents = content::WebContents::FromRenderFrameHost(host);
  if (!contents)
    return;
  CastRemotingConnector::Get(contents)->CreateBridge(std::move(source),
                                                     std::move(request));
}

namespace {
RemotingSinkCapabilities GetFeatureEnabledCapabilities() {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (base::FeatureList::IsEnabled(features::kMediaRemoting)) {
    if (base::FeatureList::IsEnabled(features::kMediaRemotingEncrypted))
      return RemotingSinkCapabilities::CONTENT_DECRYPTION_AND_RENDERING;
    return RemotingSinkCapabilities::RENDERING_ONLY;
  }
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)
  return RemotingSinkCapabilities::NONE;
}
}  // namespace

CastRemotingConnector::CastRemotingConnector(
    media_router::MediaRouter* router,
    const media_router::MediaSource::Id& media_source_id)
    : media_router::MediaRoutesObserver(router),
      media_source_id_(media_source_id),
      enabled_features_(GetFeatureEnabledCapabilities()),
      session_counter_(0),
      active_bridge_(nullptr),
      weak_factory_(this) {}

CastRemotingConnector::~CastRemotingConnector() {
  // Assume nothing about destruction/shutdown sequence of a tab. For example,
  // it's possible the owning WebContents will be destroyed before the Mojo
  // message pipes to the RemotingBridges have been closed.
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::ROUTE_TERMINATED);
  for (RemotingBridge* notifyee : bridges_) {
    notifyee->OnSinkGone();
    notifyee->OnCastRemotingConnectorDestroyed();
  }
}

void CastRemotingConnector::CreateBridge(media::mojom::RemotingSourcePtr source,
                                         media::mojom::RemoterRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<RemotingBridge>(std::move(source), this),
      std::move(request));
}

void CastRemotingConnector::RegisterBridge(RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bridges_.find(bridge) == bridges_.end());

  bridges_.insert(bridge);
  if (message_observer_ && !active_bridge_)
    bridge->OnSinkAvailable(enabled_features_);
}

void CastRemotingConnector::DeregisterBridge(RemotingBridge* bridge,
                                             RemotingStopReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bridges_.find(bridge) != bridges_.end());

  if (bridge == active_bridge_)
    StopRemoting(bridge, reason);
  bridges_.erase(bridge);
}

void CastRemotingConnector::StartRemoting(RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bridges_.find(bridge) != bridges_.end());

  // Refuse to start if there is no remoting route available, or if remoting is
  // already active.
  if (!message_observer_) {
    bridge->OnStartFailed(RemotingStartFailReason::ROUTE_TERMINATED);
    return;
  }
  if (active_bridge_) {
    bridge->OnStartFailed(RemotingStartFailReason::CANNOT_START_MULTIPLE);
    return;
  }

  // Notify all other sources that the sink is no longer available for remoting.
  // A race condition is possible, where one of the other sources will try to
  // start remoting before receiving this notification; but that attempt will
  // just fail later on.
  for (RemotingBridge* notifyee : bridges_) {
    if (notifyee == bridge)
      continue;
    notifyee->OnSinkGone();
  }

  active_bridge_ = bridge;

  // Send a start message to the Cast Provider.
  ++session_counter_;  // New remoting session ID.
  SendMessageToProvider(base::StringPrintf(
      Messaging::kStartRemotingMessageFormat, session_counter_));

  bridge->OnStarted();
}

void CastRemotingConnector::StartRemotingDataStreams(
    RemotingBridge* bridge,
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    media::mojom::RemotingDataStreamSenderRequest audio_sender_request,
    media::mojom::RemotingDataStreamSenderRequest video_sender_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Refuse to start if there is no remoting route available, or if remoting is
  // not active for this |bridge|.
  if (!message_observer_ || active_bridge_ != bridge)
    return;
  // Also, if neither audio nor video pipe was provided, or if a request for a
  // RemotingDataStreamSender was not provided for a data pipe, error-out early.
  if ((!audio_pipe.is_valid() && !video_pipe.is_valid()) ||
      (audio_pipe.is_valid() && !audio_sender_request.is_pending()) ||
      (video_pipe.is_valid() && !video_sender_request.is_pending())) {
    StopRemoting(active_bridge_, RemotingStopReason::DATA_SEND_FAILED);
    return;
  }

  // Hold on to the data pipe handles and interface requests until one/both
  // CastRemotingSenders are created and ready for use.
  pending_audio_pipe_ = std::move(audio_pipe);
  pending_video_pipe_ = std::move(video_pipe);
  pending_audio_sender_request_ = std::move(audio_sender_request);
  pending_video_sender_request_ = std::move(video_sender_request);

  // Send a "start streams" message to the Cast Provider. The provider is
  // responsible for creating and setting up a remoting Cast Streaming session
  // that will result in new CastRemotingSender instances being created here in
  // the browser process.
  SendMessageToProvider(base::StringPrintf(
      Messaging::kStartStreamsMessageFormat, session_counter_,
      pending_audio_sender_request_.is_pending() ? 'Y' : 'N',
      pending_video_sender_request_.is_pending() ? 'Y' : 'N'));
}

void CastRemotingConnector::StopRemoting(RemotingBridge* bridge,
                                         RemotingStopReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (active_bridge_ != bridge)
    return;

  active_bridge_ = nullptr;

  // Explicitly close the data pipes (and related requests) just in case the
  // "start streams" operation was interrupted.
  pending_audio_pipe_.reset();
  pending_video_pipe_.reset();
  pending_audio_sender_request_ = nullptr;
  pending_video_sender_request_ = nullptr;

  // Cancel all outstanding callbacks related to the remoting session.
  weak_factory_.InvalidateWeakPtrs();

  // Prevent the source from trying to start again until the Cast Provider has
  // indicated the stop operation has completed.
  bridge->OnSinkGone();
  // Note: At this point, all sources should think the sink is gone.

  SendMessageToProvider(base::StringPrintf(
      Messaging::kStopRemotingMessageFormat, session_counter_));
  // Note: Once the Cast Provider sends back an acknowledgement message, all
  // sources will be notified that the remoting sink is available again.

  bridge->OnStopped(reason);
}

void CastRemotingConnector::SendMessageToSink(
    RemotingBridge* bridge, const std::vector<uint8_t>& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // During an active remoting session, simply pass all binary messages through
  // to the sink.
  if (!message_observer_ || active_bridge_ != bridge)
    return;
  media_router::MediaRoutesObserver::router()->SendRouteBinaryMessage(
      message_observer_->route_id(),
      base::MakeUnique<std::vector<uint8_t>>(message),
      base::Bind(&CastRemotingConnector::HandleSendMessageResult,
                 weak_factory_.GetWeakPtr()));
}

void CastRemotingConnector::SendMessageToProvider(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!message_observer_)
    return;

  if (active_bridge_) {
    media_router::MediaRoutesObserver::router()->SendRouteMessage(
        message_observer_->route_id(), message,
        base::Bind(&CastRemotingConnector::HandleSendMessageResult,
                   weak_factory_.GetWeakPtr()));
  } else {
    struct Helper {
      static void IgnoreSendMessageResult(bool ignored) {}
    };
    media_router::MediaRoutesObserver::router()->SendRouteMessage(
        message_observer_->route_id(), message,
        base::Bind(&Helper::IgnoreSendMessageResult));
  }
}

void CastRemotingConnector::ProcessMessagesFromRoute(
    const std::vector<media_router::RouteMessage>& messages) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Note: If any calls to message parsing functions are added/changed here,
  // please update cast_remoting_connector_fuzzertest.cc as well!

  for (const media_router::RouteMessage& message : messages) {
    switch (message.type) {
      case media_router::RouteMessage::TEXT:
        // This is a notification message from the Cast Provider, about the
        // execution state of the media remoting session between Chrome and the
        // remote device.
        DCHECK(message.text);

        // If this is a "start streams" acknowledgement message, the
        // CastRemotingSenders should now be available to begin consuming from
        // the data pipes.
        if (active_bridge_ &&
            Messaging::IsMessageForSession(
                *message.text,
                Messaging::kStartedStreamsMessageFormatPartial,
                session_counter_)) {
          if (pending_audio_sender_request_.is_pending()) {
            cast::CastRemotingSender::FindAndBind(
                Messaging::GetStreamIdFromStartedMessage(
                    *message.text,
                    Messaging::kStartedStreamsMessageAudioIdSpecifier),
                std::move(pending_audio_pipe_),
                std::move(pending_audio_sender_request_),
                base::Bind(&CastRemotingConnector::OnDataSendFailed,
                           weak_factory_.GetWeakPtr()));
          }
          if (pending_video_sender_request_.is_pending()) {
            cast::CastRemotingSender::FindAndBind(
                Messaging::GetStreamIdFromStartedMessage(
                    *message.text,
                    Messaging::kStartedStreamsMessageVideoIdSpecifier),
                std::move(pending_video_pipe_),
                std::move(pending_video_sender_request_),
                base::Bind(&CastRemotingConnector::OnDataSendFailed,
                           weak_factory_.GetWeakPtr()));
          }
          break;
        }

        // If this is a failure message, call StopRemoting().
        if (active_bridge_ &&
            Messaging::IsMessageForSession(*message.text,
                                           Messaging::kFailedMessageFormat,
                                           session_counter_)) {
          StopRemoting(active_bridge_, RemotingStopReason::UNEXPECTED_FAILURE);
          break;
        }

        // If this is a stop acknowledgement message, indicating that the last
        // session was stopped, notify all sources that the sink is once again
        // available.
        if (Messaging::IsMessageForSession(*message.text,
                                           Messaging::kStoppedMessageFormat,
                                           session_counter_)) {
          if (active_bridge_) {
            // Hmm...The Cast Provider was in a state that disagrees with this
            // connector. Attempt to resolve this by shutting everything down to
            // effectively reset to a known state.
            LOG(WARNING) << "BUG: Cast Provider sent 'stopped' message during "
                            "an active remoting session.";
            StopRemoting(active_bridge_,
                         RemotingStopReason::UNEXPECTED_FAILURE);
          }
          for (RemotingBridge* notifyee : bridges_)
            notifyee->OnSinkAvailable(enabled_features_);
          break;
        }

        LOG(WARNING) << "BUG: Unexpected message from Cast Provider: "
                     << *message.text;
        break;

      case media_router::RouteMessage::BINARY:  // This is for the source.
        DCHECK(message.binary);

        // All binary messages are passed through to the source during an active
        // remoting session.
        if (active_bridge_)
          active_bridge_->OnMessageFromSink(*message.binary);
        break;
    }
  }
}

void CastRemotingConnector::HandleSendMessageResult(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // A single message send failure is treated as fatal to an active remoting
  // session.
  if (!success && active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::MESSAGE_SEND_FAILED);
}

void CastRemotingConnector::OnDataSendFailed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // A single data send failure is treated as fatal to an active remoting
  // session.
  if (active_bridge_)
    StopRemoting(active_bridge_, RemotingStopReason::DATA_SEND_FAILED);
}

void CastRemotingConnector::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes,
    const std::vector<media_router::MediaRoute::Id>& joinable_route_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If a remoting route has already been identified, check that it still
  // exists. Otherwise, shut down messaging and any active remoting, and notify
  // the sources that remoting is no longer available.
  if (message_observer_) {
    for (const media_router::MediaRoute& route : routes) {
      if (message_observer_->route_id() == route.media_route_id())
        return;  // Remoting route still exists. Take no further action.
    }
    message_observer_.reset();
    if (active_bridge_)
      StopRemoting(active_bridge_, RemotingStopReason::ROUTE_TERMINATED);
    for (RemotingBridge* notifyee : bridges_)
      notifyee->OnSinkGone();
  }

  // There shouldn't be an active RemotingBridge at this point, since there is
  // currently no known remoting route.
  DCHECK(!active_bridge_);

  // Scan |routes| for a new remoting route. If one is found, begin processing
  // messages on the route, and notify the sources that remoting is now
  // available.
  for (const media_router::MediaRoute& route : routes) {
    if (route.media_source().id() != media_source_id_)
      continue;
    message_observer_.reset(new MessageObserver(
        media_router::MediaRoutesObserver::router(), route.media_route_id(),
        this));
    // TODO(miu): In the future, scan the route ID for sink capabilities
    // properties and pass these to the source in the OnSinkAvailable()
    // notification.
    for (RemotingBridge* notifyee : bridges_)
      notifyee->OnSinkAvailable(enabled_features_);
    break;
  }
}
