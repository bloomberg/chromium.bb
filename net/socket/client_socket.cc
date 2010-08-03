// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket.h"

#include "base/histogram.h"

namespace net {

ClientSocket::ClientSocket()
    : omnibox_speculation_(false),
      subresource_speculation_(false),
      was_used_to_transmit_data_(false) {}

ClientSocket::~ClientSocket() {
  EmitPreconnectionHistograms();
}

void ClientSocket::EmitPreconnectionHistograms() const {
  DCHECK(!subresource_speculation_ || !omnibox_speculation_);
  // 0 ==> non-speculative, never connected.
  // 1 ==> non-speculative never used (but connected).
  // 2 ==> non-spculative and used.
  // 3 ==> omnibox_speculative never connected.
  // 4 ==> omnibox_speculative never used (but connected).
  // 5 ==> omnibox_speculative and used.
  // 6 ==> subresource_speculative never connected.
  // 7 ==> subresource_speculative never used (but connected).
  // 8 ==> subresource_speculative and used.
  int result;
  if (was_used_to_transmit_data_)
    result = 2;
  else if (was_ever_connected_)
    result = 1;
  else
    result = 0;  // Never used, and not really connected.

  if (omnibox_speculation_)
    result += 3;
  else if (subresource_speculation_)
    result += 6;
  UMA_HISTOGRAM_ENUMERATION("Net.PreconnectUtilization", result, 9);
}

void ClientSocket::SetSubresourceSpeculation() {
  if (was_used_to_transmit_data_)
    return;
  subresource_speculation_ = true;
}

void ClientSocket::SetOmniboxSpeculation() {
  if (was_used_to_transmit_data_)
    return;
  omnibox_speculation_ = true;
}

void ClientSocket::UpdateConnectivityState(bool is_reused) {
  // Record if this connection has every actually connected successfully.
  // Note that IsConnected() won't be defined at destruction time, so we need
  // to record this data now, while the derived class is present.
  was_ever_connected_ |= IsConnected();
  // A socket is_reused only after it has transmitted some data.
  was_used_to_transmit_data_ |= is_reused;
}

}  // namespace net

