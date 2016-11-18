// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_REMOTING_CONNECTOR_H_
#define CHROME_BROWSER_MEDIA_CAST_REMOTING_CONNECTOR_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/route_message.h"
#include "media/mojo/interfaces/remoting.mojom.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

// CastRemotingConnector connects a single source (a media element in a render
// frame) with a single sink (a media player in a remote device). There is one
// instance of a CastRemotingConnector per source WebContents (representing a
// collection of render frames), and it is created on-demand. The source in the
// render process represents itself by providing a media::mojom::RemotingSource
// service instance. The sink is represented by the Media Router Cast Provider.
//
// Whenever a candidate media source is created in a render frame,
// ChromeContentBrowserClient will call CreateMediaRemoter() to instantiate a
// media::mojom::Remoter associated with the connector. A corresponding
// media::mojom::RemotingSource is provided by the caller for communications
// back to the media source in the render frame: The connector uses this to
// notify when a sink becomes available for remoting, and to pass binary
// messages from the sink back to the source.
//
// At any time before or after the CastRemotingConnector is created, the
// Media Router Cast Provider may create a tab remoting media route. This
// indicates that the provider has found a sink that is capable of remoting and
// is available for use. At this point, CastRemotingConnector notifies all
// RemotingSources that a sink is available, and some time later a
// RemotingSource can request the start of a remoting session. Once the sink is
// no longer available, the provider terminates the route and
// CastRemotingConnector notifies all RemotingSources that the sink is gone.
//
// Note that only one RemotingSource can remote media at a time. Therefore,
// CastRemotingConnector must mediate among simultaneous requests for media
// remoting, and only allow one at a time. Currently, the policy is "first come,
// first served."
//
// When starting a remoting session, the CastRemotingConnector sends control
// messages to the Cast Provider. These control messages direct the provider to
// start/stop remoting with the remote device (e.g., launching apps or services
// on the remote device). Among its other duties, the provider will also set up
// a Cast Streaming session to provide a bitstream transport for the media. Once
// this is done, the provider sends notification messages back to
// CastRemotingConnector to acknowledge this. Then, CastRemotingConnector knows
// it can look-up and pass the mojo data pipe handles to CastRemotingSenders,
// and the remoting session will be fully active. The CastRemotingConnector is
// responsible for passing small binary messages between the source and sink,
// while the CastRemotingSender handles the high-volume media data transfer.
//
// Please see the unit tests in cast_remoting_connector_unittest.cc as a
// reference for how CastRemotingConnector and a Cast Provider interact to
// start/execute/stop remoting sessions.
class CastRemotingConnector
    : public base::SupportsUserData::Data,
      public media_router::MediaRoutesObserver {
 public:
  // Returns the instance of the CastRemotingConnector associated with
  // |source_contents|, creating a new instance if needed.
  static CastRemotingConnector* Get(content::WebContents* source_contents);

  // Used by ChromeContentBrowserClient to request a binding to a new
  // Remoter for each new source in a render frame.
  static void CreateMediaRemoter(content::RenderFrameHost* render_frame_host,
                                 media::mojom::RemotingSourcePtr source,
                                 media::mojom::RemoterRequest request);

 private:
  // Allow unit tests access to the private constructor and CreateBridge()
  // method, since unit tests don't have a complete browser (i.e., with a
  // WebContents and RenderFrameHost) to work with.
  friend class CastRemotingConnectorTest;

  // Implementation of the media::mojom::Remoter service for a single source in
  // a render frame. This is just a "lightweight bridge" that delegates calls
  // back-and-forth between a CastRemotingConnector and a
  // media::mojom::RemotingSource. An instance of this class is owned by its
  // mojo message pipe.
  class RemotingBridge;

  // A RouteMessageObserver for the remoting route that passes messages from the
  // Cast Provider back to this connector. An instance of this class only exists
  // while a remoting route is available, and is owned by CastRemotingConnector.
  class MessageObserver;

  // Main constructor. |media_source_id| refers to any remoted content managed
  // by this instance (i.e., any remoted content from one tab/WebContents).
  CastRemotingConnector(media_router::MediaRouter* router,
                        const media_router::MediaSource::Id& media_source_id);

  ~CastRemotingConnector() final;

  // Creates a RemotingBridge that implements the requested Remoter service, and
  // binds it to the interface |request|.
  void CreateBridge(media::mojom::RemotingSourcePtr source,
                    media::mojom::RemoterRequest request);

  // Called by the RemotingBridge constructor/destructor to register/deregister
  // an instance. This allows this connector to broadcast notifications to all
  // active sources.
  void RegisterBridge(RemotingBridge* bridge);
  void DeregisterBridge(RemotingBridge* bridge,
                        media::mojom::RemotingStopReason reason);

  // These methods are called by RemotingBridge to forward media::mojom::Remoter
  // calls from a source through to this connector. They ensure that only one
  // source is allowed to be in a remoting session at a time, and that no source
  // may interfere with any other.
  void StartRemoting(RemotingBridge* bridge);
  void StartRemotingDataStreams(
      RemotingBridge* bridge,
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      media::mojom::RemotingDataStreamSenderRequest audio_sender_request,
      media::mojom::RemotingDataStreamSenderRequest video_sender_request);
  void StopRemoting(RemotingBridge* bridge,
                    media::mojom::RemotingStopReason reason);
  void SendMessageToSink(RemotingBridge* bridge,
                         const std::vector<uint8_t>& message);

  // Send a control message to the Cast Provider. This may or may not succeed,
  // with the success status reported later via HandleSendMessageResult().
  void SendMessageToProvider(const std::string& message);

  // Called by the current MessageObserver to process messages observed on the
  // remoting route. There are two types of messages: 1) TEXT notification
  // messages from the Cast Provider, to report on the current state of a
  // remoting session between Chrome and the remote device, and 2) BINARY
  // messages, to be passed directly to the active remoting source during a
  // remoting session.
  void ProcessMessagesFromRoute(
      const std::vector<media_router::RouteMessage>& messages);

  // Error handlers for message/data sending during an active remoting
  // session. When a failure occurs, these immediately force-stop remoting.
  void HandleSendMessageResult(bool success);
  void OnDataSendFailed();

  // MediaRoutesObserver implementation: Scans |routes| to check whether the
  // existing remoting route has gone away and/or there is a new remoting route
  // established, and take the necessary actions to notify sources and/or
  // shutdown an active remoting session.
  void OnRoutesUpdated(
      const std::vector<media_router::MediaRoute>& routes,
      const std::vector<media_router::MediaRoute::Id>& ignored) final;

  // The MediaSource ID referring to any remoted content managed by this
  // CastRemotingConnector.
  const media_router::MediaSource::Id media_source_id_;

  // Describes the sink's capabilities according to what has been enabled via
  // |features::kMediaRemoting|. These are controlled manually via
  // chrome://flags or the command line; or in-the-wild via feature experiments.
  const media::mojom::RemotingSinkCapabilities enabled_features_;

  // Set of registered RemotingBridges, maintained by RegisterBridge() and
  // DeregisterBridge(). These pointers are always valid while they are in this
  // set.
  std::set<RemotingBridge*> bridges_;

  // Created when the Media Router Cast Provider has created a media remoting
  // route to a sink that supports remoting and is available for use. This
  // observer simply dispatches messages from the Cast Provider and sink back to
  // this connector. Once the route is gone, this is reset to null.
  std::unique_ptr<MessageObserver> message_observer_;

  // Incremented each time StartRemoting() is called, and used as a "current
  // session ID" to ensure that control messaging between this connector and the
  // Cast Provider are referring to the same remoting session. This allows both
  // this connector and the provider to ignore stale messages.
  unsigned int session_counter_;

  // When non-null, an active remoting session is taking place, with this
  // pointing to the RemotingBridge being used to communicate with the source.
  RemotingBridge* active_bridge_;

  // These temporarily hold the mojo data pipe handles and interface requests
  // until hand-off to the CastRemotingSenders.
  mojo::ScopedDataPipeConsumerHandle pending_audio_pipe_;
  mojo::ScopedDataPipeConsumerHandle pending_video_pipe_;
  media::mojom::RemotingDataStreamSenderRequest pending_audio_sender_request_;
  media::mojom::RemotingDataStreamSenderRequest pending_video_sender_request_;

  // Produces weak pointers that are only valid for the current remoting
  // session. This is used to cancel any outstanding callbacks when a remoting
  // session is stopped.
  base::WeakPtrFactory<CastRemotingConnector> weak_factory_;

  // Key used with the base::SupportsUserData interface to search for an
  // instance of CastRemotingConnector owned by a WebContents.
  static const void* const kUserDataKey;

  DISALLOW_COPY_AND_ASSIGN(CastRemotingConnector);
};

#endif  // CHROME_BROWSER_MEDIA_CAST_REMOTING_CONNECTOR_H_
