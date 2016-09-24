// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_connector.h"

#include <stdio.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/cast_remoting_sender.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/route_message.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

using content::BrowserThread;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;

namespace {

// Simple command messages sent from/to the connector to/from the Media Router
// Cast Provider to start/stop media remoting to a Cast device.
//
// Field separator (for tokenizing parts of messages).
constexpr char kMessageFieldSeparator = ':';
// Message sent by CastRemotingConnector to Cast provider to start remoting.
// Example:
//   "START_CAST_REMOTING:session=1f"
constexpr char kStartRemotingMessageFormat[] =
    "START_CAST_REMOTING:session=%x";
// Message sent by CastRemotingConnector to Cast provider to start the remoting
// RTP stream(s). Example:
//   "START_CAST_REMOTING_STREAMS:session=1f:audio=N:video=Y"
constexpr char kStartStreamsMessageFormat[] =
    "START_CAST_REMOTING_STREAMS:session=%x:audio=%c:video=%c";
// Start acknowledgement message sent by Cast provider to CastRemotingConnector
// once remoting RTP streams have been set up. Examples:
//   "STARTED_CAST_REMOTING_STREAMS:session=1f:audio_stream_id=2e:"
//                                                         "video_stream_id=3d"
//   "STARTED_CAST_REMOTING_STREAMS:session=1f:video_stream_id=b33f"
constexpr char kStartedStreamsMessageFormatPartial[] =
    "STARTED_CAST_REMOTING_STREAMS:session=%x";
constexpr char kStartedStreamsMessageAudioIdSpecifier[] = ":audio_stream_id=";
constexpr char kStartedStreamsMessageVideoIdSpecifier[] = ":video_stream_id=";
// Stop message sent by CastRemotingConnector to Cast provider. Example:
//   "STOP_CAST_REMOTING:session=1f"
constexpr char kStopRemotingMessageFormat[] =
    "STOP_CAST_REMOTING:session=%x";
// Stop acknowledgement message sent by Cast provider to CastRemotingConnector
// once remoting is available again after the last session ended. Example:
//   "STOPPED_CAST_REMOTING:session=1f"
constexpr char kStoppedMessageFormat[] =
    "STOPPED_CAST_REMOTING:session=%x";
// Failure message sent by Cast provider to CastRemotingConnector any time there
// was a fatal error (e.g., the Cast provider failed to set up the RTP streams,
// or there was some unexpected external event). Example:
//   "FAILED_CAST_REMOTING:session=1f"
constexpr char kFailedMessageFormat[] = "FAILED_CAST_REMOTING:session=%x";

// Returns true if the given |message| matches the given |format| and the
// session ID in the |message| is equal to the |expected_session_id|.
bool IsMessageForSession(const std::string& message, const char* format,
                         unsigned int expected_session_id) {
  unsigned int session_id;
  if (sscanf(message.c_str(), format, &session_id) == 1)
    return session_id == expected_session_id;
  return false;
}

// Scans |message| for |specifier| and extracts the remoting stream ID that
// follows the specifier. Returns a negative value on error.
int32_t GetStreamIdFromStartedMessage(base::StringPiece message,
                                      base::StringPiece specifier) {
  auto start = message.find(specifier);
  if (start == std::string::npos)
    return -1;
  start += specifier.size();
  if (start + 1 >= message.size())
    return -1; // Must be at least one hex digit following the specifier.
  int parsed_value;
  if (!base::HexStringToInt(
          message.substr(start, message.find(kMessageFieldSeparator, start)),
          &parsed_value) ||
      parsed_value < 0 ||
      parsed_value > std::numeric_limits<int32_t>::max()) {
    return -1; // Non-hex digits, or outside valid range.
  }
  return static_cast<int32_t>(parsed_value);
}

}  // namespace

class CastRemotingConnector::FrameRemoterFactory
    : public media::mojom::RemoterFactory {
 public:
  // |render_frame_host| represents the source render frame.
  explicit FrameRemoterFactory(content::RenderFrameHost* render_frame_host)
      : host_(render_frame_host) {
    DCHECK(host_);
  }
  ~FrameRemoterFactory() final {}

  void Create(media::mojom::RemotingSourcePtr source,
              media::mojom::RemoterRequest request) final {
    CastRemotingConnector::Get(content::WebContents::FromRenderFrameHost(host_))
        ->CreateBridge(std::move(source), std::move(request));
  }

 private:
  content::RenderFrameHost* const host_;

  DISALLOW_COPY_AND_ASSIGN(FrameRemoterFactory);
};

class CastRemotingConnector::RemotingBridge : public media::mojom::Remoter {
 public:
  // Constructs a "bridge" to delegate calls between the given |source| and
  // |connector|. |connector| must outlive this instance.
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
    connector_->DeregisterBridge(this, RemotingStopReason::SOURCE_GONE);
  }

  // The CastRemotingConnector calls these to call back to the RemotingSource.
  void OnSinkAvailable() { source_->OnSinkAvailable(); }
  void OnSinkGone() { source_->OnSinkGone(); }
  void OnStarted() { source_->OnStarted(); }
  void OnStartFailed(RemotingStartFailReason reason) {
    source_->OnStartFailed(reason);
  }
  void OnMessageFromSink(const std::vector<uint8_t>& message) {
    source_->OnMessageFromSink(message);
  }
  void OnStopped(RemotingStopReason reason) { source_->OnStopped(reason); }

  // media::mojom::Remoter implementation. The source calls these to start/stop
  // media remoting and send messages to the sink. These simply delegate to the
  // CastRemotingConnector, which mediates to establish only one remoting
  // session among possibly multiple requests. The connector will respond to
  // this request by calling one of: OnStarted() or OnStartFailed().
  void Start() final {
    connector_->StartRemoting(this);
  }
  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      media::mojom::RemotingDataStreamSenderRequest audio_sender_request,
      media::mojom::RemotingDataStreamSenderRequest video_sender_request)
      final {
    connector_->StartRemotingDataStreams(
        this, std::move(audio_pipe), std::move(video_pipe),
        std::move(audio_sender_request), std::move(video_sender_request));
  }
  void Stop(RemotingStopReason reason) final {
    connector_->StopRemoting(this, reason);
  }
  void SendMessageToSink(const std::vector<uint8_t>& message) final {
    connector_->SendMessageToSink(this, message);
  }

 private:
  media::mojom::RemotingSourcePtr source_;
  CastRemotingConnector* const connector_;

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
void CastRemotingConnector::CreateRemoterFactory(
    content::RenderFrameHost* render_frame_host,
    media::mojom::RemoterFactoryRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<FrameRemoterFactory>(render_frame_host),
      std::move(request));
}

CastRemotingConnector::CastRemotingConnector(
    media_router::MediaRouter* router,
    const media_router::MediaSource::Id& route_source_id)
    : media_router::MediaRoutesObserver(router, route_source_id),
      session_counter_(0),
      active_bridge_(nullptr),
      weak_factory_(this) {}

CastRemotingConnector::~CastRemotingConnector() {
  // Remoting should not be active at this point, and this instance is expected
  // to outlive all bridges. See comment in CreateBridge().
  DCHECK(!active_bridge_);
  DCHECK(bridges_.empty());
}

void CastRemotingConnector::CreateBridge(media::mojom::RemotingSourcePtr source,
                                         media::mojom::RemoterRequest request) {
  // Create a new RemotingBridge, which will become owned by the message pipe
  // associated with |request|. |this| CastRemotingConnector should be valid
  // for the full lifetime of the bridge because it can be deduced that the
  // connector will always outlive the mojo message pipe: A single WebContents
  // will destroy the render frame tree (which destroys all associated mojo
  // message pipes) before CastRemotingConnector. To ensure this assumption is
  // not broken by future design changes in external modules, a DCHECK() has
  // been placed in the CastRemotingConnector destructor as a sanity-check.
  mojo::MakeStrongBinding(
      base::MakeUnique<RemotingBridge>(std::move(source), this),
      std::move(request));
}

void CastRemotingConnector::RegisterBridge(RemotingBridge* bridge) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bridges_.find(bridge) == bridges_.end());

  bridges_.insert(bridge);
  if (message_observer_ && !active_bridge_)
    bridge->OnSinkAvailable();
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
  SendMessageToProvider(
      base::StringPrintf(kStartRemotingMessageFormat, session_counter_));

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
      kStartStreamsMessageFormat, session_counter_,
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

  SendMessageToProvider(
      base::StringPrintf(kStopRemotingMessageFormat, session_counter_));
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
            IsMessageForSession(*message.text,
                                kStartedStreamsMessageFormatPartial,
                                session_counter_)) {
          if (pending_audio_sender_request_.is_pending()) {
            cast::CastRemotingSender::FindAndBind(
                GetStreamIdFromStartedMessage(
                    *message.text, kStartedStreamsMessageAudioIdSpecifier),
                std::move(pending_audio_pipe_),
                std::move(pending_audio_sender_request_),
                base::Bind(&CastRemotingConnector::OnDataSendFailed,
                           weak_factory_.GetWeakPtr()));
          }
          if (pending_video_sender_request_.is_pending()) {
            cast::CastRemotingSender::FindAndBind(
                GetStreamIdFromStartedMessage(
                    *message.text, kStartedStreamsMessageVideoIdSpecifier),
                std::move(pending_video_pipe_),
                std::move(pending_video_sender_request_),
                base::Bind(&CastRemotingConnector::OnDataSendFailed,
                           weak_factory_.GetWeakPtr()));
          }
          break;
        }

        // If this is a failure message, call StopRemoting().
        if (active_bridge_ &&
            IsMessageForSession(*message.text, kFailedMessageFormat,
                                session_counter_)) {
          StopRemoting(active_bridge_, RemotingStopReason::UNEXPECTED_FAILURE);
          break;
        }

        // If this is a stop acknowledgement message, indicating that the last
        // session was stopped, notify all sources that the sink is once again
        // available.
        if (IsMessageForSession(*message.text, kStoppedMessageFormat,
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
            notifyee->OnSinkAvailable();
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
  if (!routes.empty()) {
    const media_router::MediaRoute& route = routes.front();
    message_observer_.reset(new MessageObserver(
        media_router::MediaRoutesObserver::router(), route.media_route_id(),
        this));
    // TODO(miu): In the future, scan the route ID for sink capabilities
    // properties and pass these to the source in the OnSinkAvailable()
    // notification.
    for (RemotingBridge* notifyee : bridges_)
      notifyee->OnSinkAvailable();
  }
}
