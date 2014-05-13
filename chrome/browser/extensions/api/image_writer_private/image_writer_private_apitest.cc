// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/image_writer_private.h"

namespace extensions {

using api::image_writer_private::RemovableStorageDevice;

class ImageWriterPrivateApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    scoped_refptr<StorageDeviceList> device_list(new StorageDeviceList);

    RemovableStorageDevice* expected1 = new RemovableStorageDevice();
    expected1->vendor = "Vendor 1";
    expected1->model = "Model 1";
    expected1->capacity = 1 << 20;
    expected1->storage_unit_id = "/test/id/1";

    RemovableStorageDevice* expected2 = new RemovableStorageDevice();
    expected2->vendor = "Vendor 2";
    expected2->model = "Model 2";
    expected2->capacity = 1 << 22;
    expected2->storage_unit_id = "/test/id/2";

    linked_ptr<RemovableStorageDevice> device1(expected1);
    device_list->data.push_back(device1);
    linked_ptr<RemovableStorageDevice> device2(expected2);
    device_list->data.push_back(device2);

    RemovableStorageProvider::SetDeviceListForTesting(device_list);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    RemovableStorageProvider::ClearDeviceListForTesting();
  }
};

IN_PROC_BROWSER_TEST_F(ImageWriterPrivateApiTest, TestListDevices) {
  ASSERT_TRUE(RunExtensionTest("image_writer_private/list_devices"))
      << message_;
}

}  // namespace extensions
