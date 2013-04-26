// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CRYPTO_SERVER_CONFIG_PROTOBUF_H_
#define NET_QUIC_CRYPTO_CRYPTO_SERVER_CONFIG_PROTOBUF_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "net/quic/crypto/crypto_protocol.h"

namespace net {

// TODO(rch): sync with server more rationally.
class QuicServerConfigProtobuf {
 public:
  class PrivateKey {
   public:
    CryptoTag tag() const {
      return tag_;
    }
    void set_tag(CryptoTag tag) {
      tag_ = tag;
    }
    std::string private_key() const {
      return private_key_;
    }
    void set_private_key(std::string key) {
      private_key_ = key;
    }

   private:
    CryptoTag tag_;
    std::string private_key_;
  };

  QuicServerConfigProtobuf();
  ~QuicServerConfigProtobuf();

  size_t key_size() const {
    return keys_.size();
  }

  const PrivateKey& key(size_t i) const {
    DCHECK_GT(keys_.size(), i);
    return *keys_[i];
  }

  std::string config() const {
    return config_;
  }

  void set_config(base::StringPiece config) {
    config_ = config.as_string();
  }

  QuicServerConfigProtobuf::PrivateKey* add_key() {
    keys_.push_back(new PrivateKey);
    return keys_.back();
  }

 private:
  std::vector<PrivateKey*> keys_;
  std::string config_;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_SERVER_CONFIG_PROTOBUF_H_
