// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_HID_HID_API_H_
#define EXTENSIONS_BROWSER_API_HID_HID_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/api/hid/hid_connection_resource.h"
#include "extensions/browser/api/hid/hid_device_manager.h"
#include "extensions/common/api/hid.h"

namespace device {
class HidService;
}  // namespace device

namespace net {
class IOBuffer;
}  // namespace net

namespace extensions {

class HidAsyncApiFunction : public AsyncApiFunction {
 public:
  HidAsyncApiFunction();

  bool PrePrepare() override;
  bool Respond() override;

 protected:
  ~HidAsyncApiFunction() override;

  HidConnectionResource* GetHidConnectionResource(int api_resource_id);
  void RemoveHidConnectionResource(int api_resource_id);

  void CompleteWithError(const std::string& error);

  HidDeviceManager* device_manager_;
  ApiResourceManager<HidConnectionResource>* connection_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HidAsyncApiFunction);
};

class HidGetDevicesFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.getDevices", HID_GETDEVICES);

  HidGetDevicesFunction();

 protected:
  bool Prepare() override;
  void AsyncWorkStart() override;

  ~HidGetDevicesFunction() override;

  scoped_ptr<core_api::hid::GetDevices::Params> parameters_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HidGetDevicesFunction);
};

class HidConnectFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.connect", HID_CONNECT);

  HidConnectFunction();

 protected:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  ~HidConnectFunction() override;

  void OnConnectComplete(scoped_refptr<device::HidConnection> connection);

  scoped_ptr<core_api::hid::Connect::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectFunction);
};

class HidDisconnectFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.disconnect", HID_DISCONNECT);

  HidDisconnectFunction();

 protected:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  ~HidDisconnectFunction() override;

  scoped_ptr<core_api::hid::Disconnect::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidDisconnectFunction);
};

class HidReceiveFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.receive", HID_RECEIVE);

  HidReceiveFunction();

 protected:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  ~HidReceiveFunction() override;

  void OnFinished(bool success,
                  scoped_refptr<net::IOBuffer> buffer,
                  size_t size);

  scoped_ptr<core_api::hid::Receive::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidReceiveFunction);
};

class HidSendFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.send", HID_SEND);

  HidSendFunction();

 protected:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  ~HidSendFunction() override;

  void OnFinished(bool success);

  scoped_ptr<core_api::hid::Send::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidSendFunction);
};

class HidReceiveFeatureReportFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.receiveFeatureReport",
                             HID_RECEIVEFEATUREREPORT);

  HidReceiveFeatureReportFunction();

 protected:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  ~HidReceiveFeatureReportFunction() override;

  void OnFinished(bool success,
                  scoped_refptr<net::IOBuffer> buffer,
                  size_t size);

  scoped_ptr<core_api::hid::ReceiveFeatureReport::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidReceiveFeatureReportFunction);
};

class HidSendFeatureReportFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.sendFeatureReport", HID_SENDFEATUREREPORT);

  HidSendFeatureReportFunction();

 protected:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  ~HidSendFeatureReportFunction() override;

  void OnFinished(bool success);

  scoped_ptr<core_api::hid::SendFeatureReport::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidSendFeatureReportFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_HID_HID_API_H_
