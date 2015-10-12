// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_DEVICE_CAPABILITIES_IMPL_H_
#define CHROMECAST_BASE_DEVICE_CAPABILITIES_IMPL_H_

#include "base/containers/hash_tables.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chromecast/base/device_capabilities.h"

namespace chromecast {

class DeviceCapabilitiesImpl : public DeviceCapabilities {
 public:
  ~DeviceCapabilitiesImpl() override;

  // DeviceCapabilities implementation:
  void Register(const std::string& key,
                scoped_ptr<base::Value> init_value,
                Validator* validator) override;
  void Unregister(const std::string& key, const Validator* validator) override;
  bool BluetoothSupported() const override;
  bool DisplaySupported() const override;
  bool GetCapability(const std::string& path,
                     const base::Value** out_value) const override;
  const std::string& GetCapabilitiesString() const override;
  const base::DictionaryValue* GetCapabilities() const override;
  void SetCapability(const std::string& path,
                     scoped_ptr<base::Value> proposed_value) override;
  void MergeDictionary(const base::DictionaryValue& dict_value) override;
  void AddCapabilitiesObserver(Observer* observer) override;
  void RemoveCapabilitiesObserver(Observer* observer) override;

 private:
  // For DeviceCapabilitiesImpl()
  friend class DeviceCapabilities;
  // For SetValidatedValueInternal()
  friend class DeviceCapabilities::Validator;

  // Map from capability key to corresponding Validator. Gets updated
  // in Register()/Unregister().
  typedef base::hash_map<std::string, Validator*> ValidatorMap;

  // Internal constructor used by static DeviceCapabilities::Create*() methods.
  DeviceCapabilitiesImpl();

  void SetValidatedValueInternal(const std::string& path,
                                 scoped_ptr<base::Value> new_value) override;

  void AddValidator(const std::string& key, Validator* validator);
  void UpdateStrAndNotifyChanged(const std::string& path);

  scoped_ptr<base::DictionaryValue> capabilities_;
  scoped_ptr<const std::string> capabilities_str_;

  ValidatorMap validator_map_;
  base::ObserverList<Observer> observer_list_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCapabilitiesImpl);
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_DEVICE_CAPABILITIES_IMPL_H_
