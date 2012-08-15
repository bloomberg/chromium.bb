// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_PROVIDER_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "content/browser/device_orientation/device_data.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT Provider : public base::RefCountedThreadSafe<Provider> {
 public:
  class Observer {
   public:
    // Called when device data changes.
    // An Observer must not synchronously call Provider::RemoveObserver
    // or Provider::AddObserver when this is called.
    virtual void OnDeviceDataUpdate(const DeviceData* device_data,
                                    DeviceData::Type device_data_type) = 0;
    DeviceData::Type device_data_type() { return device_data_type_; }

   protected:
    Observer(DeviceData::Type device_data_type)
        : device_data_type_(device_data_type) {
    }
    virtual ~Observer() {}

   private:
    // Each Observer observes exactly one type of DeviceData.
    DeviceData::Type device_data_type_;
  };

  // Returns a pointer to the singleton instance of this class.
  // The caller should store the returned pointer in a scoped_refptr.
  // The Provider instance is lazily constructed when GetInstance() is called,
  // and destructed when the last scoped_refptr referring to it is destructed.
  static Provider* GetInstance();

  // Inject a mock Provider for testing. Only a weak pointer to the injected
  // object will be held by Provider, i.e. it does not itself contribute to the
  // injected object's reference count.
  static void SetInstanceForTests(Provider* provider);

  // Get the current instance. Used for testing.
  static Provider* GetInstanceForTests();

  // Note: AddObserver may call back synchronously to the observer with data.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  Provider();
  virtual ~Provider();

 private:
  friend class base::RefCountedThreadSafe<Provider>;
  static Provider* instance_;

  DISALLOW_COPY_AND_ASSIGN(Provider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_PROVIDER_H_
