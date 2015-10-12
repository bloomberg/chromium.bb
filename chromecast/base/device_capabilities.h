// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_DEVICE_CAPABILITIES_H_
#define CHROMECAST_BASE_DEVICE_CAPABILITIES_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromecast {

// Device capabilities are a set of features used to determine what operations
// are available on the device. They are identified by a key (string) and a
// value (base::Value). The class serves 2 main purposes:
//
// 1) Provide an interface for updating default capabilities and querying their
// current value. Default capabilities are known to the system beforehand
// and used by modules throughout Chromecast to control behavior of operations.
//
// 2) Store dynamic capabilities. Dynamic capabilities are not known to the
// system beforehand and are introduced by external parties. These capabilites
// are stored and then forwarded to app servers that use them to determine how
// to interact with the device.

// TODO(esum):
// 1) A pure virtual interface with implementation files is no longer needed
//    now that this file resides in chromecast/base. Change it such that there
//    is just device_capabilities.h/cc.
// 2) Make class thread-safe with locking.
// 3) Add WifiSupported, HotspotSupported, and MultizoneSupported capabilities.
// 4) It's not ideal to have the accessors (BluetoothSupported(), etc.) not
//    be valid initially until the capability gets registered. We might want
//    to use some kind of builder class to solve this.
class DeviceCapabilities {
 public:
  class Observer {
   public:
    // Called when DeviceCapabilities gets written to in any way. |path|
    // is full path to capability that has been updated.
    virtual void OnCapabilitiesChanged(const std::string& path) = 0;

   protected:
    virtual ~Observer() {}
  };

  // When another module attempts to update the value for a capability,
  // a manager may want to validate the change or even modify the new value.
  // Managers that wish to perform this validation should inherit from the
  // Validator class and implement its interface.
  class Validator {
   public:
    // |path| is full path to capability, which could include paths expanded on
    // the capability key that gets registered through the Register() method.
    // For example, if a key of "foo" is registered for a Validator, |path|
    // could be "foo", "foo.bar", "foo.bar.what", etc. |proposed_value| is new
    // value being proposed for |path|. Determines if |proposed_value| is valid
    // change for |path|. This method may be asynchronous, but multiple calls
    // to it must be handled serially. Returns response through
    // SetValidatedValue().
    virtual void Validate(const std::string& path,
                          scoped_ptr<base::Value> proposed_value) = 0;

   protected:
    explicit Validator(DeviceCapabilities* capabilities);
    virtual ~Validator() {}

    DeviceCapabilities* capabilities() const { return capabilities_; }

    // Meant to be called when Validate() has finished. |path| is full path to
    // capability. |new_value| is new validated value to be used in
    // DeviceCapabilities. This method passes these parameters to
    // DeviceCapabilities, where |path| is updated internally to |new_value|.
    void SetValidatedValue(const std::string& path,
                           scoped_ptr<base::Value> new_value) const;

   private:
    DeviceCapabilities* const capabilities_;

    DISALLOW_COPY_AND_ASSIGN(Validator);
  };

  // Default Capability keys
  static const char kKeyBluetoothSupported[];
  static const char kKeyDisplaySupported[];

  virtual ~DeviceCapabilities() {}

  // Create empty instance with no capabilities. Although the class is not
  // singleton, there is meant to be a single instance owned by another module.
  // The instance should be created early enough for all managers to register
  // themselves, and then live long enough for all managers to unregister.
  static scoped_ptr<DeviceCapabilities> Create();
  // Creates an instance where all the default capabilities are initialized
  // to a predefined default value, and no Validators are registered. For use
  // only in unit tests.
  static scoped_ptr<DeviceCapabilities> CreateForTesting();

  // Registers a Validator for a capability. A given key must only be
  // registered once, and must be unregistered before calling Register() again.
  // The capability also gets added to the class with an initial value passed
  // in. If the capability has a value of Dictionary type, |key| must be just
  // the capability's top-level key and not include path expansions to levels
  // farther down. For example, "foo" is a valid value for |key|, but "foo.bar"
  // is not. Note that if "foo.bar" is updated in SetCapability(), the
  // Validate() method for "foo"'s Validator will be called, with a |path| of
  // "foo.bar". Both Register() and Unregister() must be called on cast
  // receiver main thread; the Validator provided will also be called on cast
  // receiver main thread.
  virtual void Register(const std::string& key,
                        scoped_ptr<base::Value> init_value,
                        Validator* validator) = 0;
  // Unregisters Validator for |key| and removes capability. |validator|
  // argument must match |validator| argument that was passed in to Register()
  // for |key|.
  virtual void Unregister(const std::string& key,
                          const Validator* validator) = 0;

  // Accessors for default capabilities. Note that the capability must be added
  // through Register() or SetCapability() before accessors are called.
  virtual bool BluetoothSupported() const = 0;
  virtual bool DisplaySupported() const = 0;

  // Gets the value of |path|. If capability at |path| does not exist,
  // |out_value| remains untouched. Returns true if the capability has been
  // successfully retrieved. Note that this does NOT perform a deep copy, and
  // DeviceCapabilities still owns the memory returned through |out_value|.
  virtual bool GetCapability(const std::string& path,
                             const base::Value** out_value) const = 0;

  // Returns the complete capability string (JSON format).
  virtual const std::string& GetCapabilitiesString() const = 0;

  // Returns the capabilities dictionary.
  virtual const base::DictionaryValue* GetCapabilities() const = 0;

  // Updates the value at |path| to |proposed_value| if |path| already exists
  // and adds new capability if |path| doesn't. Note that if a key has been
  // registered that is at the beginning of |path|, then the Validator will be
  // used to determine if |proposed_value| is accepted.
  // Ex: If "foo" has a Validator registered, a |path| of "foo.bar"
  // will cause |proposed_value| to go through the Validator's Validate()
  // method. Client code may use the Observer interface to determine the
  // ultimate value used.
  virtual void SetCapability(const std::string& path,
                             scoped_ptr<base::Value> proposed_value) = 0;
  // Iterates through entries in |dict_value| and calls SetCapability() for
  // each one.
  virtual void MergeDictionary(const base::DictionaryValue& dict_value) = 0;

  // Adds/removes an observer. It doesn't take the ownership of |observer|.
  virtual void AddCapabilitiesObserver(Observer* observer) = 0;
  virtual void RemoveCapabilitiesObserver(Observer* observer) = 0;

 protected:
  DeviceCapabilities() {}

 private:
  // Does actual internal update of |path| to |new_value|.
  virtual void SetValidatedValueInternal(const std::string& path,
                                         scoped_ptr<base::Value> new_value) = 0;

  DISALLOW_COPY_AND_ASSIGN(DeviceCapabilities);
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_DEVICE_CAPABILITIES_H_
