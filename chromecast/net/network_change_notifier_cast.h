// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_CAST_H_
#define CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_CAST_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/network_change_notifier.h"

namespace chromecast {

// TODO(lcwu): http://crbug.com/391064. This is a place holder for
// cast-specific NetworkChangeNotifier implementation. The actual
// implementation of this class will come in later CLs.
class NetworkChangeNotifierCast : public net::NetworkChangeNotifier {
 public:
  NetworkChangeNotifierCast();
  virtual ~NetworkChangeNotifierCast();

  // net::NetworkChangeNotifier implementation:
  virtual net::NetworkChangeNotifier::ConnectionType
      GetCurrentConnectionType() const OVERRIDE;

 private:
  friend class NetworkChangeNotifierCastTest;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierCast);
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_CAST_H_
