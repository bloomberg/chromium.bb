// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HID_HID_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_HID_HID_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/hid/hid_connection_resource.h"
#include "chrome/browser/extensions/api/hid/hid_device_manager.h"
#include "chrome/common/extensions/api/hid.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/async_api_function.h"

namespace net {

class IOBufferWithSize;

}  // namespace net

namespace extensions {

class HidAsyncApiFunction : public AsyncApiFunction {
 public:
  HidAsyncApiFunction();

  virtual bool PrePrepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 protected:
  virtual ~HidAsyncApiFunction();

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
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

  virtual ~HidGetDevicesFunction();

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::GetDevices::Params> parameters_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HidGetDevicesFunction);
};

class HidConnectFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.connect", HID_CONNECT);

  HidConnectFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidConnectFunction();

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::Connect::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectFunction);
};

class HidDisconnectFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.disconnect", HID_DISCONNECT);

  HidDisconnectFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidDisconnectFunction();

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::Disconnect::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidDisconnectFunction);
};

class HidReceiveFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.receive", HID_RECEIVE);

  HidReceiveFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidReceiveFunction();

  void OnFinished(bool success, size_t bytes);

  scoped_refptr<net::IOBufferWithSize> buffer_;
  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::Receive::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidReceiveFunction);
};

class HidSendFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.send", HID_SEND);

  HidSendFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidSendFunction();

  void OnFinished(bool success, size_t bytes);

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::Send::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidSendFunction);
};

class HidReceiveFeatureReportFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.receiveFeatureReport",
                             HID_RECEIVEFEATUREREPORT);

  HidReceiveFeatureReportFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidReceiveFeatureReportFunction();

  void OnFinished(bool success, size_t bytes);

  scoped_refptr<net::IOBufferWithSize> buffer_;
  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::ReceiveFeatureReport::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidReceiveFeatureReportFunction);
};

class HidSendFeatureReportFunction : public HidAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.sendFeatureReport", HID_SENDFEATUREREPORT);

  HidSendFeatureReportFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual ~HidSendFeatureReportFunction();

  void OnFinished(bool success, size_t bytes);

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::hid::SendFeatureReport::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidSendFeatureReportFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HID_HID_API_H_
