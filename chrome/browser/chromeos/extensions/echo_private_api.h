// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"

class EchoPrivateGetRegistrationCodeFunction : public SyncExtensionFunction {
 public:
  EchoPrivateGetRegistrationCodeFunction();

 protected:
  virtual ~EchoPrivateGetRegistrationCodeFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  void GetRegistrationCode(const std::string& type);
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getRegistrationCode",
                             ECHOPRIVATE_GETREGISTRATIONCODE)
};

class EchoPrivateGetOobeTimestampFunction : public AsyncExtensionFunction {
 public:
  EchoPrivateGetOobeTimestampFunction();

 protected:
  virtual ~EchoPrivateGetOobeTimestampFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  bool GetOobeTimestampOnFileThread();
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getOobeTimestamp",
                             ECHOPRIVATE_GETOOBETIMESTAMP)
};

class EchoPrivateCheckAllowRedeemOffersFunction
    : public AsyncExtensionFunction {
 public:
  EchoPrivateCheckAllowRedeemOffersFunction();

 protected:
  virtual ~EchoPrivateCheckAllowRedeemOffersFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  void CheckAllowRedeemOffers();
  DECLARE_EXTENSION_FUNCTION("echoPrivate.checkAllowRedeemOffers",
                             ECHOPRIVATE_CHECKALLOWREDEEMOFFERS)
};
#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_
