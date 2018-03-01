// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_version_manager.h"

#include "net/quic/core/quic_versions.h"
#include "net/quic/platform/api/quic_flags.h"

namespace net {

QuicVersionManager::QuicVersionManager(
    ParsedQuicVersionVector supported_versions)
    : enable_version_99_(GetQuicFlag(FLAGS_quic_enable_version_99)),
      enable_version_43_(GetQuicReloadableFlag(quic_enable_version_43)),
      enable_version_42_(GetQuicReloadableFlag(quic_enable_version_42_2)),
      disable_version_41_(GetQuicReloadableFlag(quic_disable_version_41)),
      disable_version_38_(GetQuicReloadableFlag(quic_disable_version_38)),
      disable_version_37_(GetQuicReloadableFlag(quic_disable_version_37)),
      allowed_supported_versions_(std::move(supported_versions)) {
  RefilterSupportedVersions();
}

QuicVersionManager::~QuicVersionManager() {}

const QuicTransportVersionVector&
QuicVersionManager::GetSupportedTransportVersions() {
  MaybeRefilterSupportedVersions();
  return filtered_transport_versions_;
}

const ParsedQuicVersionVector& QuicVersionManager::GetSupportedVersions() {
  MaybeRefilterSupportedVersions();
  return filtered_supported_versions_;
}

void QuicVersionManager::MaybeRefilterSupportedVersions() {
  if (enable_version_99_ != GetQuicFlag(FLAGS_quic_enable_version_99) ||
      enable_version_43_ != GetQuicReloadableFlag(quic_enable_version_43) ||
      enable_version_42_ != GetQuicReloadableFlag(quic_enable_version_42_2) ||
      disable_version_41_ != GetQuicReloadableFlag(quic_disable_version_41) ||
      disable_version_38_ != GetQuicReloadableFlag(quic_disable_version_38) ||
      disable_version_37_ != GetQuicReloadableFlag(quic_disable_version_37)) {
    enable_version_99_ = GetQuicFlag(FLAGS_quic_enable_version_99);
    enable_version_43_ = GetQuicReloadableFlag(quic_enable_version_43);
    enable_version_42_ = GetQuicReloadableFlag(quic_enable_version_42_2);
    disable_version_37_ = GetQuicReloadableFlag(quic_disable_version_37);
    disable_version_38_ = GetQuicReloadableFlag(quic_disable_version_38);
    disable_version_41_ = GetQuicReloadableFlag(quic_disable_version_41);
    RefilterSupportedVersions();
  }
}

void QuicVersionManager::RefilterSupportedVersions() {
  filtered_supported_versions_ =
      FilterSupportedVersions(allowed_supported_versions_);
  filtered_transport_versions_.clear();
  for (ParsedQuicVersion version : filtered_supported_versions_) {
    filtered_transport_versions_.push_back(version.transport_version);
  }
}

}  // namespace net
