// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/usb/usb_device_resource.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "chrome/common/extensions/api/usb.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

static base::LazyInstance<ProfileKeyedAPIFactory<
        ApiResourceManager<UsbDeviceResource> > >
            g_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
ProfileKeyedAPIFactory<ApiResourceManager<UsbDeviceResource> >*
ApiResourceManager<UsbDeviceResource>::GetFactoryInstance() {
  return g_factory.Pointer();
}

UsbDeviceResource::UsbDeviceResource(const std::string& owner_extension_id,
                                     scoped_refptr<UsbDeviceHandle> device)
    : ApiResource(owner_extension_id), device_(device) {}

UsbDeviceResource::~UsbDeviceResource() {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&UsbDeviceHandle::Close, device_));
}

}  // namespace extensions
