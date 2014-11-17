// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_ABSTRACT_PEER_CONNECTION_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_ABSTRACT_PEER_CONNECTION_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace devtools_bridge {

class AbstractDataChannel;

/**
 * WebRTC PeerConnection adapter for DevTools bridge.
 */
class AbstractPeerConnection {
 public:
  /**
   * Delegate is called on signaling thread.
   */
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    virtual void OnIceConnectionChange(bool connected) = 0;
    virtual void OnIceCandidate(
        const std::string& sdp_mid,
        int sdp_mline_index,
        const std::string& sdp) = 0;
    virtual void OnLocalOfferCreatedAndSetSet(
        const std::string& description) = 0;
    virtual void OnLocalAnswerCreatedAndSetSet(
        const std::string& description) = 0;
    virtual void OnRemoteDescriptionSet() = 0;
    virtual void OnFailure(const std::string& description) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  AbstractPeerConnection() {}
  virtual ~AbstractPeerConnection() {}

  virtual void CreateAndSetLocalOffer() = 0;
  virtual void CreateAndSetLocalAnswer() = 0;
  virtual void SetRemoteOffer(const std::string& description) = 0;
  virtual void SetRemoteAnswer(const std::string& description) = 0;
  virtual void AddIceCandidate(
      const std::string& sdp_mid,
      int sdp_mline_index,
      const std::string& sdp) = 0;

  virtual scoped_ptr<AbstractDataChannel> CreateDataChannel(int channelId) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractPeerConnection);
};

}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_ABSTRACT_PEER_CONNECTION_H_
