// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_server_info.h"

#include "base/bind.h"
#include "base/pickle.h"
#include "base/strings/string_piece.h"

namespace net {

QuicServerInfo::State::State() {}

QuicServerInfo::State::~State() {}

void QuicServerInfo::State::Clear() {
  data.clear();
}

// TODO(rtenneti): Flesh out the details.
QuicServerInfo::QuicServerInfo(
    const std::string& hostname)
    : hostname_(hostname),
      weak_factory_(this) {
}

QuicServerInfo::~QuicServerInfo() {
}

const QuicServerInfo::State& QuicServerInfo::state() const {
  return state_;
}

QuicServerInfo::State* QuicServerInfo::mutable_state() {
  return &state_;
}

bool QuicServerInfo::Parse(const std::string& data) {
  State* state = mutable_state();

  state->Clear();

  bool r = ParseInner(data);
  if (!r)
    state->Clear();
  return r;
}

bool QuicServerInfo::ParseInner(const std::string& data) {
  // TODO(rtenneti): restore QuicCryptoClientConfig::CachedState.
  // State* state = mutable_state();

  // Pickle p(data.data(), data.size());
  // PickleIterator iter(p);

  return true;
}

std::string QuicServerInfo::Serialize() const {
  Pickle p(sizeof(Pickle::Header));

  // TODO(rtenneti): serialize QuicCryptoClientConfig::CachedState.
  return std::string(reinterpret_cast<const char *>(p.data()), p.size());
}

QuicServerInfoFactory::~QuicServerInfoFactory() {}

}  // namespace net
