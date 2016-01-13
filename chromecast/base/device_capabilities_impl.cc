// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/device_capabilities_impl.h"

#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
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
const char DeviceCapabilities::kKeyHiResAudioSupported[] =
    "hi_res_audio_supported";

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
  capabilities->SetCapability(
      kKeyHiResAudioSupported,
      make_scoped_ptr(new base::FundamentalValue(false)));
  return make_scoped_ptr(capabilities);
}

scoped_refptr<DeviceCapabilities::Data> DeviceCapabilities::CreateData() {
  return make_scoped_refptr(new Data);
}

scoped_refptr<DeviceCapabilities::Data> DeviceCapabilities::CreateData(
    scoped_ptr<const base::DictionaryValue> dictionary) {
  DCHECK(dictionary.get());
  return make_scoped_refptr(new Data(std::move(dictionary)));
}

DeviceCapabilities::Validator::Validator(DeviceCapabilities* capabilities)
    : capabilities_(capabilities) {
  DCHECK(capabilities);
}

void DeviceCapabilities::Validator::SetValidatedValue(
    const std::string& path,
    scoped_ptr<base::Value> new_value) const {
  capabilities_->SetValidatedValue(path, std::move(new_value));
}

DeviceCapabilities::Data::Data()
    : dictionary_(new base::DictionaryValue),
      json_string_(SerializeToJson(*dictionary_)) {
  DCHECK(json_string_.get());
}

DeviceCapabilities::Data::Data(
    scoped_ptr<const base::DictionaryValue> dictionary)
    : dictionary_(std::move(dictionary)),
      json_string_(SerializeToJson(*dictionary_)) {
  DCHECK(dictionary_.get());
  DCHECK(json_string_.get());
}

DeviceCapabilitiesImpl::Data::~Data() {}

DeviceCapabilitiesImpl::ValidatorInfo::ValidatorInfo(Validator* validator)
    : validator_(validator), task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(validator_);
  DCHECK(task_runner_.get());
}

DeviceCapabilitiesImpl::ValidatorInfo::~ValidatorInfo() {
  // Check that ValidatorInfo is being destroyed on the same thread that it was
  // constructed on.
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void DeviceCapabilitiesImpl::ValidatorInfo::Validate(
    const std::string& path,
    scoped_ptr<base::Value> proposed_value) const {
  // Check that we are running Validate on the same thread that ValidatorInfo
  // was constructed on.
  DCHECK(task_runner_->BelongsToCurrentThread());
  validator_->Validate(path, std::move(proposed_value));
}

DeviceCapabilitiesImpl::DeviceCapabilitiesImpl()
    : data_(CreateData()),
      task_runner_for_writes_(base::ThreadTaskRunnerHandle::Get()),
      observer_list_(new base::ObserverListThreadSafe<Observer>) {
  DCHECK(task_runner_for_writes_.get());
}

DeviceCapabilitiesImpl::~DeviceCapabilitiesImpl() {
  // Make sure that any registered Validators have unregistered at this point
  DCHECK(validator_map_.empty());
  // Make sure that all observers have been removed at this point
  observer_list_->AssertEmpty();
}

void DeviceCapabilitiesImpl::Register(const std::string& key,
                                      Validator* validator) {
  DCHECK(IsValidRegisterKey(key));
  DCHECK(validator);

  base::AutoLock auto_lock(validation_lock_);
  bool added =
      validator_map_.add(key, make_scoped_ptr(new ValidatorInfo(validator)))
          .second;
  // Check that a validator has not already been registered for this key
  DCHECK(added);
}

void DeviceCapabilitiesImpl::Unregister(const std::string& key,
                                        const Validator* validator) {
  base::AutoLock auto_lock(validation_lock_);
  auto validator_it = validator_map_.find(key);
  DCHECK(validator_it != validator_map_.end());
  // Check that validator being unregistered matches the original for |key|.
  // This prevents managers from accidentally unregistering incorrect
  // validators.
  DCHECK_EQ(validator, validator_it->second->validator());
  // Check that validator is unregistering on same thread that it was
  // registered on
  DCHECK(validator_it->second->task_runner()->BelongsToCurrentThread());
  validator_map_.erase(validator_it);
}

DeviceCapabilities::Validator* DeviceCapabilitiesImpl::GetValidator(
    const std::string& key) const {
  base::AutoLock auto_lock(validation_lock_);
  auto validator_it = validator_map_.find(key);
  return validator_it == validator_map_.end()
             ? nullptr
             : validator_it->second->validator();
}

bool DeviceCapabilitiesImpl::BluetoothSupported() const {
  scoped_refptr<Data> data_ref = GetData();
  bool bluetooth_supported = false;
  bool found_key = data_ref->dictionary().GetBoolean(kKeyBluetoothSupported,
                                                     &bluetooth_supported);
  DCHECK(found_key);
  return bluetooth_supported;
}

bool DeviceCapabilitiesImpl::DisplaySupported() const {
  scoped_refptr<Data> data_ref = GetData();
  bool display_supported = false;
  bool found_key = data_ref->dictionary().GetBoolean(kKeyDisplaySupported,
                                                     &display_supported);
  DCHECK(found_key);
  return display_supported;
}

bool DeviceCapabilitiesImpl::HiResAudioSupported() const {
  scoped_refptr<Data> data_ref = GetData();
  bool hi_res_audio_supported = false;
  bool found_key = data_ref->dictionary().GetBoolean(kKeyHiResAudioSupported,
                                                     &hi_res_audio_supported);
  DCHECK(found_key);
  return hi_res_audio_supported;
}

scoped_ptr<base::Value> DeviceCapabilitiesImpl::GetCapability(
    const std::string& path) const {
  scoped_refptr<Data> data_ref = GetData();
  const base::Value* value = nullptr;
  bool found_path = data_ref->dictionary().Get(path, &value);
  return found_path ? value->CreateDeepCopy() : scoped_ptr<base::Value>();
}

scoped_refptr<DeviceCapabilities::Data>
DeviceCapabilitiesImpl::GetData() const {
  // Need to acquire lock here when copy constructing data_ otherwise we could
  // be concurrently be writing to scoped_refptr in SetValidatedValue(), which
  // could cause a bad scoped_refptr read.
  base::AutoLock auto_lock(data_lock_);
  return data_;
}

void DeviceCapabilitiesImpl::SetCapability(
    const std::string& path,
    scoped_ptr<base::Value> proposed_value) {
  DCHECK(proposed_value.get());
  if (!IsValidPath(path)) {
    LOG(DFATAL) << "Invalid capability path encountered for SetCapability()";
    return;
  }

  {
    base::AutoLock auto_lock(validation_lock_);
    // Check for Validator registered under first key per the Register()
    // interface.
    auto validator_it = validator_map_.find(GetFirstKey(path));
    if (validator_it != validator_map_.end()) {
      // We do not want to post a task directly for the Validator's Validate()
      // method here because if another thread is in the middle of unregistering
      // that Validator, there will be an outstanding call to Validate() that
      // occurs after it has unregistered. Since ValidatorInfo gets destroyed
      // in Unregister() on same thread that validation should run on, we can
      // post a task to the Validator's thread with weak_ptr. This way, if the
      // Validator gets unregistered, the call to Validate will get skipped.
      validator_it->second->task_runner()->PostTask(
          FROM_HERE, base::Bind(&ValidatorInfo::Validate,
                                validator_it->second->AsWeakPtr(), path,
                                base::Passed(&proposed_value)));
      return;
    }
  }
  // Since we are done checking for a registered Validator at this point, we
  // can release the lock. All further member access will be for capabilities.
  SetValidatedValue(path, std::move(proposed_value));
}

void DeviceCapabilitiesImpl::MergeDictionary(
    const base::DictionaryValue& dict_value) {
  for (base::DictionaryValue::Iterator it(dict_value); !it.IsAtEnd();
       it.Advance()) {
    SetCapability(it.key(), it.value().CreateDeepCopy());
  }
}

void DeviceCapabilitiesImpl::AddCapabilitiesObserver(Observer* observer) {
  DCHECK(observer);
  observer_list_->AddObserver(observer);
}

void DeviceCapabilitiesImpl::RemoveCapabilitiesObserver(Observer* observer) {
  DCHECK(observer);
  observer_list_->RemoveObserver(observer);
}

void DeviceCapabilitiesImpl::SetValidatedValue(
    const std::string& path,
    scoped_ptr<base::Value> new_value) {
  // All internal writes/modifications of capabilities must occur on same
  // thread to avoid race conditions.
  if (!task_runner_for_writes_->BelongsToCurrentThread()) {
    task_runner_for_writes_->PostTask(
        FROM_HERE,
        base::Bind(&DeviceCapabilitiesImpl::SetValidatedValue,
                   base::Unretained(this), path, base::Passed(&new_value)));
    return;
  }

  DCHECK(IsValidPath(path));
  DCHECK(new_value.get());

  // We don't need to acquire lock here when reading data_ because we know that
  // all writes to data_ must occur serially on thread that we're on.
  const base::Value* cur_value = nullptr;
  bool capability_unchanged = data_->dictionary().Get(path, &cur_value) &&
                              cur_value->Equals(new_value.get());
  if (capability_unchanged) {
    VLOG(1) << "Ignoring unchanged capability: " << path;
    return;
  }

  // In this sequence, we create a deep copy, modify the deep copy, and then
  // do a pointer swap. We do this to have minimal time spent in the
  // data_lock_. If we were to lock and modify the capabilities
  // dictionary directly, there may be expensive writes that block other
  // threads.
  scoped_ptr<base::DictionaryValue> dictionary_deep_copy(
      data_->dictionary().CreateDeepCopy());
  dictionary_deep_copy->Set(path, std::move(new_value));
  scoped_refptr<Data> new_data(CreateData(std::move(dictionary_deep_copy)));

  {
    base::AutoLock auto_lock(data_lock_);
    // Using swap instead of assignment operator here because it's a little
    // faster. Avoids an extra call to AddRef()/Release().
    data_.swap(new_data);
  }

  // Even though ObserverListThreadSafe notifications are always asynchronous
  // (posts task even if to same thread), no locks should be held at this point
  // in the code. This is just to be safe that no deadlocks occur if Observers
  // call DeviceCapabilities methods in OnCapabilitiesChanged().
  observer_list_->Notify(FROM_HERE, &Observer::OnCapabilitiesChanged, path);
}

}  // namespace chromecast
