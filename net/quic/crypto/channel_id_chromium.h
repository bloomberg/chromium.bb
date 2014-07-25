// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CHANNEL_ID_CHROMIUM_H_
#define NET_QUIC_CRYPTO_CHANNEL_ID_CHROMIUM_H_

#include <set>

#include "net/quic/crypto/channel_id.h"

namespace crypto {
class ECPrivateKey;
}  // namespace crypto

namespace net {

class ChannelIDService;

class NET_EXPORT_PRIVATE ChannelIDKeyChromium: public ChannelIDKey {
 public:
  explicit ChannelIDKeyChromium(crypto::ECPrivateKey* ec_private_key);
  virtual ~ChannelIDKeyChromium();

  // ChannelIDKey interface
  virtual bool Sign(base::StringPiece signed_data,
                    std::string* out_signature) const OVERRIDE;
  virtual std::string SerializeKey() const OVERRIDE;

 private:
  scoped_ptr<crypto::ECPrivateKey> ec_private_key_;
};

// ChannelIDSourceChromium implements the QUIC ChannelIDSource interface.
class ChannelIDSourceChromium : public ChannelIDSource {
 public:
  explicit ChannelIDSourceChromium(
      ChannelIDService* channel_id_service);
  virtual ~ChannelIDSourceChromium();

  // ChannelIDSource interface
  virtual QuicAsyncStatus GetChannelIDKey(
      const std::string& hostname,
      scoped_ptr<ChannelIDKey>* channel_id_key,
      ChannelIDSourceCallback* callback) OVERRIDE;

 private:
  class Job;
  typedef std::set<Job*> JobSet;

  void OnJobComplete(Job* job);

  // Set owning pointers to active jobs.
  JobSet active_jobs_;

  // The service for retrieving Channel ID keys.
  ChannelIDService* const channel_id_service_;

  DISALLOW_COPY_AND_ASSIGN(ChannelIDSourceChromium);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CHANNEL_ID_CHROMIUM_H_
