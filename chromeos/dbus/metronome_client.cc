// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/metronome_client.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/chromeos_switches.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

// TODO(benchan): Move these DBus constants to system_api.
namespace metronome {

const char kMetronomeInterface[] = "org.chromium.Metronome";
const char kMetronomeServiceName[] = "org.chromium.Metronome";
const char kMetronomeServicePath[] = "/org/chromium/Metronome";
const char kTimestampUpdatedSignal[] = "TimestampUpdated";

}  // namespace metronome

namespace chromeos {

namespace {

////////////////////////////////////////////////////////////////////////////////

// The MetronomeClient implementation.
class MetronomeClientImpl : public MetronomeClient {
 public:
  MetronomeClientImpl()
      : proxy_(nullptr), signal_connected_(false), weak_ptr_factory_(this) {}

  ~MetronomeClientImpl() override {}

  // MetronomeClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 protected:
  // DBusClient:
  void Init(dbus::Bus* bus) override;

 private:
  // Handles TimestampUpdated signal and notifies |observers_|.
  void OnTimestampUpdated(dbus::Signal* signal);

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded);

  dbus::ObjectProxy* proxy_;

  // True when |proxy_| has been connected.
  bool signal_connected_;

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<MetronomeClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MetronomeClientImpl);
};

void MetronomeClientImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  if (!signal_connected_) {
    signal_connected_ = true;
    proxy_->ConnectToSignal(metronome::kMetronomeInterface,
                            metronome::kTimestampUpdatedSignal,
                            base::Bind(&MetronomeClientImpl::OnTimestampUpdated,
                                       weak_ptr_factory_.GetWeakPtr()),
                            base::Bind(&MetronomeClientImpl::OnSignalConnected,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
  observers_.AddObserver(observer);
}

void MetronomeClientImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void MetronomeClientImpl::Init(dbus::Bus* bus) {
  proxy_ =
      bus->GetObjectProxy(metronome::kMetronomeServiceName,
                          dbus::ObjectPath(metronome::kMetronomeServicePath));
}

void MetronomeClientImpl::OnTimestampUpdated(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  uint64 beacon_timestamp = 0;
  uint64 local_timestamp = 0;
  if (!reader.PopUint64(&beacon_timestamp) ||
      !reader.PopUint64(&local_timestamp)) {
    LOG(ERROR) << "Invalid signal: " << signal->ToString();
    return;
  }
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnTimestampUpdated(beacon_timestamp, local_timestamp));
}

void MetronomeClientImpl::OnSignalConnected(const std::string& interface,
                                            const std::string& signal,
                                            bool succeeded) {
  LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " " << signal
                            << " failed.";
}

////////////////////////////////////////////////////////////////////////////////

// A fake implementation of MetronomeClient. It does not provide true
// synchronization and only exists to exercise the interfaces.
class MetronomeClientFakeImpl : public MetronomeClient {
 public:
  MetronomeClientFakeImpl()
      : random_offset_(base::RandInt(-1000000, 1000000)) {}

  ~MetronomeClientFakeImpl() override { beacon_timer_.Stop(); }

  // MetronomeClient:
  void AddObserver(Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // DBusClient:
  void Init(dbus::Bus* bus) override {
    beacon_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(1000),
                        this,
                        &MetronomeClientFakeImpl::UpdateBeacon);
  }

 private:
  void UpdateBeacon() {
    base::Time now_time = base::Time::Now();
    base::TimeTicks now_ticks = base::TimeTicks::Now();
    uint64 fake_beacon_timestamp = now_time.ToInternalValue() +
                                   base::RandInt(-1000, 1000);
    uint64 fake_local_timestamp = now_ticks.ToInternalValue() + random_offset_ +
                                  base::RandInt(-1000, 1000);
    FOR_EACH_OBSERVER(
        Observer, observers_,
        OnTimestampUpdated(fake_beacon_timestamp, fake_local_timestamp));
  }

  int64 random_offset_;
  base::RepeatingTimer<MetronomeClientFakeImpl> beacon_timer_;

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(MetronomeClientFakeImpl);
};

////////////////////////////////////////////////////////////////////////////////

// A stub implementation of MetronomeClient. It allows unit tests to complete.
class MetronomeClientStubImpl : public MetronomeClient {
 public:
  MetronomeClientStubImpl() {}
  ~MetronomeClientStubImpl() override {}

  // MetronomeClient:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  // DBusClient:
  void Init(dbus::Bus* bus) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MetronomeClientStubImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////

MetronomeClient::MetronomeClient() {
}

MetronomeClient::~MetronomeClient() {
}

// static
MetronomeClient* MetronomeClient::Create(DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new MetronomeClientImpl();
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestMetronomeTimer)) {
    return new MetronomeClientFakeImpl();
  }
  return new MetronomeClientStubImpl();
}

}  // namespace chromeos
