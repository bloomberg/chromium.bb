// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_P2P_TRANSPORT_IMPL_H_
#define CONTENT_RENDERER_P2P_P2P_TRANSPORT_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "net/base/completion_callback.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "webkit/glue/p2p_transport.h"

namespace cricket {
class Candidate;
class PortAllocator;
class P2PTransportChannel;
class TransportChannel;
class TransportChannelImpl;
}  // namespace cricket

namespace jingle_glue {
class TransportChannelSocketAdapter;
class PseudoTcpAdapter;
}  // namespace jingle_glue

namespace talk_base {
class NetworkManager;
class PacketSocketFactory;
}  // namespace talk_base

namespace content {

class P2PPortAllocator;
class P2PSocketDispatcher;

class CONTENT_EXPORT P2PTransportImpl
    : NON_EXPORTED_BASE(public webkit_glue::P2PTransport),
      public sigslot::has_slots<> {
 public:
  // Creates P2PTransportImpl using specified NetworkManager and
  // PacketSocketFactory. Takes ownership of |network_manager| and
  // |socket_factory|. Provided to be used for tests only.
  P2PTransportImpl(talk_base::NetworkManager* network_manager,
                   talk_base::PacketSocketFactory* socket_factory);

  // Creates P2PTransportImpl using specified
  // P2PSocketDispatcher. This constructor creates IpcNetworkManager
  // and IpcPacketSocketFactory, and keeps ownership of these objects.
  explicit P2PTransportImpl(P2PSocketDispatcher* socket_dispatcher);

  virtual ~P2PTransportImpl();

  // webkit_glue::P2PTransport interface.
  virtual bool Init(WebKit::WebFrame* web_frame,
                    const std::string& name,
                    Protocol protocol,
                    const Config& config,
                    EventHandler* event_handler) OVERRIDE;
  virtual bool AddRemoteCandidate(const std::string& address) OVERRIDE;
  virtual net::Socket* GetChannel() OVERRIDE;

 private:
  class ChannelAdapter;

  void OnRequestSignaling();
  void OnCandidateReady(cricket::TransportChannelImpl* channel,
                        const cricket::Candidate& candidate);
  void OnReadableState(cricket::TransportChannel* channel);
  void OnWriteableState(cricket::TransportChannel* channel);

  void OnTcpConnected(int result);

  P2PSocketDispatcher* socket_dispatcher_;
  std::string name_;
  EventHandler* event_handler_;
  State state_;

  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;

  scoped_ptr<cricket::PortAllocator> allocator_;
  scoped_ptr<cricket::P2PTransportChannel> channel_;

  scoped_ptr<jingle_glue::TransportChannelSocketAdapter> channel_adapter_;
  scoped_ptr<jingle_glue::PseudoTcpAdapter> pseudo_tcp_adapter_;

  net::OldCompletionCallbackImpl<P2PTransportImpl> connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(P2PTransportImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_P2P_TRANSPORT_IMPL_H_
