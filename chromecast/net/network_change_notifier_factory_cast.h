// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_FACTORY_CAST_H_
#define CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_FACTORY_CAST_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/network_change_notifier_factory.h"

namespace chromecast {

class NetworkChangeNotifierCast;

class NetworkChangeNotifierFactoryCast
    : public net::NetworkChangeNotifierFactory {
 public:
  NetworkChangeNotifierFactoryCast() {}
  virtual ~NetworkChangeNotifierFactoryCast();

  // net::NetworkChangeNotifierFactory implementation:
  virtual net::NetworkChangeNotifier* CreateInstance() OVERRIDE;

  static NetworkChangeNotifierCast* GetInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierFactoryCast);
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_NETWORK_CHANGE_NOTIFIER_FACTORY_CAST_H_
