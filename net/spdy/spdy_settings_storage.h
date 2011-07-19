// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SETTING_STORAGE_H_
#define NET_SPDY_SPDY_SETTING_STORAGE_H_
#pragma once

#include <map>
#include "base/basictypes.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_api.h"
#include "net/spdy/spdy_framer.h"

namespace net {

// SpdySettingsStorage stores SpdySettings which have been transmitted between
// endpoints for the SPDY SETTINGS frame.
class NET_TEST SpdySettingsStorage {
 public:
  SpdySettingsStorage();
  ~SpdySettingsStorage();

  // Gets a copy of the SpdySettings stored for a host.
  // If no settings are stored, returns an empty set of settings.
  // NOTE: Since settings_map_ may be cleared, don't store the address of the
  // return value.
  const spdy::SpdySettings& Get(const HostPortPair& host_port_pair) const;

  // Saves settings for a host.
  void Set(const HostPortPair& host_port_pair,
           const spdy::SpdySettings& settings);

  // Clears out the settings_map_ object.
  void Clear();

 private:
  typedef std::map<HostPortPair, spdy::SpdySettings> SettingsMap;

  SettingsMap settings_map_;

  DISALLOW_COPY_AND_ASSIGN(SpdySettingsStorage);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SETTING_STORAGE_H_

