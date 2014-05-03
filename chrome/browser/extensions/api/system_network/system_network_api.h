// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_NETWORK_SYSTEM_NETWORK_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_NETWORK_SYSTEM_NETWORK_API_H_

#include "chrome/common/extensions/api/system_network.h"
#include "extensions/browser/extension_function.h"
#include "net/base/net_util.h"

namespace extensions {
namespace api {

class SystemNetworkGetNetworkInterfacesFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.network.getNetworkInterfaces",
                             SYSTEM_NETWORK_GETNETWORKINTERFACES)

  SystemNetworkGetNetworkInterfacesFunction();

 protected:
  virtual ~SystemNetworkGetNetworkInterfacesFunction();

  // AsyncApiFunction:
  virtual bool RunAsync() OVERRIDE;

 private:
  void GetListOnFileThread();
  void HandleGetListError();
  void SendResponseOnUIThread(const net::NetworkInterfaceList& interface_list);
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_NETWORK_SYSTEM_NETWORK_API_H_
