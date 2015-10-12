// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/device_capabilities_impl.h"

#include "base/logging.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"

namespace chromecast {

namespace {

const char kPathSeparator = '.';

// Determines if a key passed to Register() is valid. No path separators can
// be present in the key and it must not be empty.
bool IsValidRegisterKey(const std::string& key) {
  return !key.empty() && key.find(kPathSeparator) == std::string::npos;
}

// Determines if a path is valid. This is true if there are no empty keys
// anywhere in the path (ex: .foo, foo., foo..bar are all invalid).
bool IsValidPath(const std::string& path) {
  return !path.empty() && *path.begin() != kPathSeparator &&
         *path.rbegin() != kPathSeparator &&
         path.find("..") == std::string::npos;
}

// Given a path, gets the first key present in the path (ex: for path "foo.bar"
// return "foo").
std::string GetFirstKey(const std::string& path) {
  std::size_t length_to_first_separator = path.find(kPathSeparator);
  return (length_to_first_separator == std::string::npos)
             ? path
             : path.substr(0, length_to_first_separator);
}

}  // namespace

// static Default Capability Keys
const char DeviceCapabilities::kKeyBluetoothSupported[] = "bluetooth_supported";
const char DeviceCapabilities::kKeyDisplaySupported[] = "display_supported";

// static
scoped_ptr<DeviceCapabilities> DeviceCapabilities::Create() {
  return make_scoped_ptr(new DeviceCapabilitiesImpl);
}

// static
scoped_ptr<DeviceCapabilities> DeviceCapabilities::CreateForTesting() {
  DeviceCapabilities* capabilities = new DeviceCapabilitiesImpl;
  capabilities->SetCapability(
      kKeyBluetoothSupported,
      make_scoped_ptr(new base::FundamentalValue(false)));
  capabilities->SetCapability(
      kKeyDisplaySupported, make_scoped_ptr(new base::FundamentalValue(true)));
  return make_scoped_ptr(capabilities);
}

DeviceCapabilities::Validator::Validator(DeviceCapabilities* capabilities)
    : capabilities_(capabilities) {
  DCHECK(capabilities);
}

void DeviceCapabilities::Validator::SetValidatedValue(
    const std::string& path,
    scoped_ptr<base::Value> new_value) const {
  capabilities_->SetValidatedValueInternal(path, new_value.Pass());
}

DeviceCapabilitiesImpl::DeviceCapabilitiesImpl()
    : capabilities_(new base::DictionaryValue),
      capabilities_str_(SerializeToJson(*capabilities_)) {
  DCHECK(capabilities_str_.get());
}

DeviceCapabilitiesImpl::~DeviceCapabilitiesImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Make sure that any registered Validators have unregistered at this point
  DCHECK(validator_map_.empty());
}

void DeviceCapabilitiesImpl::Register(const std::string& key,
                                      scoped_ptr<base::Value> init_value,
                                      Validator* validator) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsValidRegisterKey(key));
  DCHECK(init_value.get());
  DCHECK(validator);

  AddValidator(key, validator);

  capabilities_->Set(key, init_value.Pass());
  UpdateStrAndNotifyChanged(key);
}

void DeviceCapabilitiesImpl::Unregister(const std::string& key,
                                        const Validator* validator) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto validator_it = validator_map_.find(key);
  DCHECK(validator_it != validator_map_.end());
  // Check that validator being unregistered matches the original for |key|.
  // This prevents managers from accidentally unregistering incorrect
  // validators.
  DCHECK_EQ(validator, validator_it->second);
  validator_map_.erase(validator_it);

  bool removed = capabilities_->Remove(key, nullptr);
  DCHECK(removed);
  UpdateStrAndNotifyChanged(key);
}

bool DeviceCapabilitiesImpl::BluetoothSupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool bluetooth_supported = false;
  bool found_key =
      capabilities_->GetBoolean(kKeyBluetoothSupported, &bluetooth_supported);
  DCHECK(found_key);
  return bluetooth_supported;
}

bool DeviceCapabilitiesImpl::DisplaySupported() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool display_supported = false;
  bool found_key =
      capabilities_->GetBoolean(kKeyDisplaySupported, &display_supported);
  DCHECK(found_key);
  return display_supported;
}

bool DeviceCapabilitiesImpl::GetCapability(
    const std::string& path,
    const base::Value** out_value) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return capabilities_ && capabilities_->Get(path, out_value);
}

const std::string& DeviceCapabilitiesImpl::GetCapabilitiesString() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return *capabilities_str_;
}

const base::DictionaryValue* DeviceCapabilitiesImpl::GetCapabilities() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return capabilities_.get();
}

void DeviceCapabilitiesImpl::SetCapability(
    const std::string& path,
    scoped_ptr<base::Value> proposed_value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(proposed_value.get());
  if (!IsValidPath(path)) {
    LOG(DFATAL) << "Invalid capability path encountered for SetCapability()";
    return;
  }

  // Check for Validator registered under first key per the Register()
  // interface.
  auto validator_it = validator_map_.find(GetFirstKey(path));
  if (validator_it == validator_map_.end()) {
    SetValidatedValueInternal(path, proposed_value.Pass());
    return;
  }

  validator_it->second->Validate(path, proposed_value.Pass());
}

void DeviceCapabilitiesImpl::MergeDictionary(
    const base::DictionaryValue& dict_value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (base::DictionaryValue::Iterator it(dict_value); !it.IsAtEnd();
       it.Advance()) {
    SetCapability(it.key(), it.value().CreateDeepCopy());
  }
}

void DeviceCapabilitiesImpl::AddCapabilitiesObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void DeviceCapabilitiesImpl::RemoveCapabilitiesObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void DeviceCapabilitiesImpl::SetValidatedValueInternal(
    const std::string& path,
    scoped_ptr<base::Value> new_value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsValidPath(path));
  DCHECK(new_value.get());

  const base::Value* cur_value = nullptr;
  bool capability_unchaged =
      GetCapability(path, &cur_value) && cur_value->Equals(new_value.get());
  if (capability_unchaged) {
    VLOG(1) << "Ignoring unchanged capability: " << path;
    return;
  }

  capabilities_->Set(path, new_value.Pass());
  UpdateStrAndNotifyChanged(path);
}

void DeviceCapabilitiesImpl::AddValidator(const std::string& key,
                                          Validator* validator) {
  DCHECK(validator);
  bool added = validator_map_.insert(std::make_pair(key, validator)).second;
  // Check that a validator has not already been registered for this key
  DCHECK(added);
}

void DeviceCapabilitiesImpl::UpdateStrAndNotifyChanged(
    const std::string& path) {
  // Update capabilities string here since all updates to capabilities must
  // ultimately call this method no matter where the update originated from.
  capabilities_str_ = SerializeToJson(*capabilities_);
  DCHECK(capabilities_str_.get());
  FOR_EACH_OBSERVER(Observer, observer_list_, OnCapabilitiesChanged(path));
}

}  // namespace chromecast
