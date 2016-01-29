// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_DEVICE_CAPABILITIES_IMPL_H_
#define CHROMECAST_BASE_DEVICE_CAPABILITIES_IMPL_H_

#include <string>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "chromecast/base/device_capabilities.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {

class DeviceCapabilitiesImpl : public DeviceCapabilities {
 public:
  ~DeviceCapabilitiesImpl() override;

  // DeviceCapabilities implementation:
  void Register(const std::string& key, Validator* validator) override;
  void Unregister(const std::string& key, const Validator* validator) override;
  Validator* GetValidator(const std::string& key) const override;
  bool BluetoothSupported() const override;
  bool DisplaySupported() const override;
  bool HiResAudioSupported() const override;
  scoped_ptr<base::Value> GetCapability(const std::string& path) const override;
  scoped_refptr<Data> GetData() const override;
  void SetCapability(const std::string& path,
                     scoped_ptr<base::Value> proposed_value) override;
  void MergeDictionary(const base::DictionaryValue& dict_value) override;
  void AddCapabilitiesObserver(Observer* observer) override;
  void RemoveCapabilitiesObserver(Observer* observer) override;

 private:
  class ValidatorInfo : public base::SupportsWeakPtr<ValidatorInfo> {
   public:
    explicit ValidatorInfo(Validator* validator);
    ~ValidatorInfo();

    Validator* validator() const { return validator_; }

    scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
      return task_runner_;
    }

    void Validate(const std::string& path,
                  scoped_ptr<base::Value> proposed_value) const;

   private:
    Validator* const validator_;
    // TaskRunner of thread that validator_ was registered on
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(ValidatorInfo);
  };

  // For DeviceCapabilitiesImpl()
  friend class DeviceCapabilities;
  // For SetValidatedValueInternal()
  friend class DeviceCapabilities::Validator;

  // Map from capability key to corresponding ValidatorInfo. Gets updated
  // in Register()/Unregister().
  typedef base::ScopedPtrHashMap<std::string, scoped_ptr<ValidatorInfo>>
      ValidatorMap;

  // Internal constructor used by static DeviceCapabilities::Create*() methods.
  DeviceCapabilitiesImpl();

  void SetValidatedValue(const std::string& path,
                         scoped_ptr<base::Value> new_value) override;

  // Lock for reading/writing data_ pointer
  mutable base::Lock data_lock_;
  // Lock for reading/writing validator_map_
  mutable base::Lock validation_lock_;

  scoped_refptr<Data> data_;
  // TaskRunner for capability writes. All internal writes to data_ must occur
  // on task_runner_for_writes_'s thread.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_writes_;

  ValidatorMap validator_map_;
  const scoped_refptr<base::ObserverListThreadSafe<Observer>> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCapabilitiesImpl);
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_DEVICE_CAPABILITIES_IMPL_H_
