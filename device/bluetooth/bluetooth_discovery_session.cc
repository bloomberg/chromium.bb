// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_discovery_session.h"

#include "base/bind.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

BluetoothDiscoveryFilter::BluetoothDiscoveryFilter(TransportMask transport) {
  SetTransport(transport);
}

BluetoothDiscoveryFilter::~BluetoothDiscoveryFilter() {
}

bool BluetoothDiscoveryFilter::GetRSSI(int16_t* out_rssi) const {
  DCHECK(out_rssi);
  if (!rssi_.get())
    return false;

  *out_rssi = *rssi_;
  return true;
}

void BluetoothDiscoveryFilter::SetRSSI(int16_t rssi) {
  if (!rssi_.get())
    rssi_.reset(new int16_t());

  *rssi_ = rssi;
}

bool BluetoothDiscoveryFilter::GetPathloss(uint16_t* out_pathloss) const {
  DCHECK(out_pathloss);
  if (!pathloss_.get())
    return false;

  *out_pathloss = *pathloss_;
  return true;
}

void BluetoothDiscoveryFilter::SetPathloss(uint16_t pathloss) {
  if (!pathloss_.get())
    pathloss_.reset(new uint16_t());

  *pathloss_ = pathloss;
}

BluetoothDiscoveryFilter::TransportMask BluetoothDiscoveryFilter::GetTransport()
    const {
  return transport_;
}

void BluetoothDiscoveryFilter::SetTransport(TransportMask transport) {
  DCHECK(transport > 0 && transport < 4);
  transport_ = transport;
}

void BluetoothDiscoveryFilter::GetUUIDs(
    std::set<device::BluetoothUUID>& out_uuids) const {
  out_uuids.clear();

  for (auto& uuid : uuids_)
    out_uuids.insert(*uuid);
}

void BluetoothDiscoveryFilter::AddUUID(const device::BluetoothUUID& uuid) {
  DCHECK(uuid.IsValid());
  for (auto& uuid_it : uuids_) {
    if (*uuid_it == uuid)
      return;
  }

  uuids_.push_back(new device::BluetoothUUID(uuid));
}

void BluetoothDiscoveryFilter::CopyFrom(
    const BluetoothDiscoveryFilter& filter) {
  transport_ = filter.transport_;

  if (filter.uuids_.size()) {
    for (auto& uuid : filter.uuids_)
      AddUUID(*uuid);
  } else
    uuids_.clear();

  if (filter.rssi_.get()) {
    SetRSSI(*filter.rssi_);
  } else
    rssi_.reset();

  if (filter.pathloss_.get()) {
    SetPathloss(*filter.pathloss_);
  } else
    pathloss_.reset();
}

scoped_ptr<device::BluetoothDiscoveryFilter> BluetoothDiscoveryFilter::Merge(
    const device::BluetoothDiscoveryFilter* filter_a,
    const device::BluetoothDiscoveryFilter* filter_b) {
  scoped_ptr<BluetoothDiscoveryFilter> result;

  if (!filter_a && !filter_b) {
    return result;
  }

  result.reset(new BluetoothDiscoveryFilter(Transport::TRANSPORT_DUAL));

  if (!filter_a || !filter_b || filter_a->IsDefault() ||
      filter_b->IsDefault()) {
    return result;
  }

  // both filters are not empty, so they must have transport set.
  result->SetTransport(filter_a->transport_ | filter_b->transport_);

  // if both filters have uuids, them merge them. Otherwise uuids filter should
  // be left empty
  if (filter_a->uuids_.size() && filter_b->uuids_.size()) {
    std::set<device::BluetoothUUID> uuids;
    filter_a->GetUUIDs(uuids);
    for (auto& uuid : uuids)
      result->AddUUID(uuid);

    filter_b->GetUUIDs(uuids);
    for (auto& uuid : uuids)
      result->AddUUID(uuid);
  }

  if ((filter_a->rssi_.get() && filter_b->pathloss_.get()) ||
      (filter_a->pathloss_.get() && filter_b->rssi_.get())) {
    // if both rssi and pathloss filtering is enabled in two different
    // filters, we can't tell which filter is more generic, and we don't set
    // proximity filtering on merged filter.
    return result;
  }

  if (filter_a->rssi_.get() && filter_b->rssi_.get()) {
    result->SetRSSI(std::min(*filter_a->rssi_, *filter_b->rssi_));
  } else if (filter_a->pathloss_.get() && filter_b->pathloss_.get()) {
    result->SetPathloss(std::max(*filter_a->pathloss_, *filter_b->pathloss_));
  }

  return result;
}

bool BluetoothDiscoveryFilter::Equals(
    const BluetoothDiscoveryFilter& other) const {
  if (((!!rssi_.get()) != (!!other.rssi_.get())) ||
      (rssi_.get() && other.rssi_.get() && *rssi_ != *other.rssi_)) {
    return false;
  }

  if (((!!pathloss_.get()) != (!!other.pathloss_.get())) ||
      (pathloss_.get() && other.pathloss_.get() &&
       *pathloss_ != *other.pathloss_)) {
    return false;
  }

  if (transport_ != other.transport_)
    return false;

  std::set<device::BluetoothUUID> uuids_a, uuids_b;
  GetUUIDs(uuids_a);
  other.GetUUIDs(uuids_b);
  if (uuids_a != uuids_b)
    return false;

  return true;
}

bool BluetoothDiscoveryFilter::IsDefault() const {
  return !(rssi_.get() || pathloss_.get() || uuids_.size() ||
           transport_ != Transport::TRANSPORT_DUAL);
}

BluetoothDiscoverySession::BluetoothDiscoverySession(
    scoped_refptr<BluetoothAdapter> adapter,
    scoped_ptr<BluetoothDiscoveryFilter> discovery_filter)
    : active_(true),
      adapter_(adapter),
      discovery_filter_(discovery_filter.release()),
      weak_ptr_factory_(this) {
  DCHECK(adapter_.get());
}

BluetoothDiscoverySession::~BluetoothDiscoverySession() {
  if (active_) {
    Stop(base::Bind(&base::DoNothing), base::Bind(&base::DoNothing));
    MarkAsInactive();
  }
}

bool BluetoothDiscoverySession::IsActive() const {
  return active_;
}

void BluetoothDiscoverySession::Stop(const base::Closure& success_callback,
                                     const ErrorCallback& error_callback) {
  if (!active_) {
    LOG(WARNING) << "Discovery session not active. Cannot stop.";
    error_callback.Run();
    return;
  }
  VLOG(1) << "Stopping device discovery session.";
  base::Closure deactive_discovery_session =
      base::Bind(&BluetoothDiscoverySession::DeactivateDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr());

  // Create a callback that runs
  // BluetoothDiscoverySession::DeactivateDiscoverySession if the session still
  // exists, but always runs success_callback.
  base::Closure discovery_session_removed_callback =
      base::Bind(&BluetoothDiscoverySession::OnDiscoverySessionRemoved,
                 deactive_discovery_session, success_callback);
  adapter_->RemoveDiscoverySession(discovery_filter_.get(),
                                   discovery_session_removed_callback,
                                   error_callback);
}

// static
void BluetoothDiscoverySession::OnDiscoverySessionRemoved(
    const base::Closure& deactivate_discovery_session,
    const base::Closure& success_callback) {
  deactivate_discovery_session.Run();
  success_callback.Run();
}

void BluetoothDiscoverySession::DeactivateDiscoverySession() {
  MarkAsInactive();
  discovery_filter_.reset();
}

void BluetoothDiscoverySession::MarkAsInactive() {
  if (!active_)
    return;
  active_ = false;
  adapter_->DiscoverySessionBecameInactive(this);
}

void BluetoothDiscoverySession::SetDiscoveryFilter(
    scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  discovery_filter_.reset(discovery_filter.release());
  adapter_->SetDiscoveryFilter(adapter_->GetMergedDiscoveryFilter().Pass(),
                               callback, error_callback);
}

const BluetoothDiscoveryFilter* BluetoothDiscoverySession::GetDiscoveryFilter()
    const {
  return discovery_filter_.get();
}

}  // namespace device
