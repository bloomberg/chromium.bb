// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class DataReductionProxyClearDataSavingsFunction
    : public UIThreadExtensionFunction {
 private:
  ~DataReductionProxyClearDataSavingsFunction() override {};

  DECLARE_EXTENSION_FUNCTION("dataReductionProxy.clearDataSavings",
                             DATAREDUCTIONPROXY_CLEARDATASAVINGS)

  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_API_H_
