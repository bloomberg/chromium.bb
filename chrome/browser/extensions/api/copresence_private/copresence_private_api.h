// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_PRIVATE_COPRESENCE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_PRIVATE_COPRESENCE_PRIVATE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"

namespace copresence {
class WhispernetClient;
}

namespace extensions {

class CopresencePrivateFunction : public ChromeUIThreadExtensionFunction {
 protected:
  copresence::WhispernetClient* GetWhispernetClient();
  virtual ~CopresencePrivateFunction() {}
};

class CopresencePrivateSendFoundFunction : public CopresencePrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendFound",
                             COPRESENCEPRIVATE_SENDFOUND);

 protected:
  virtual ~CopresencePrivateSendFoundFunction() {}
  virtual ExtensionFunction::ResponseAction Run() OVERRIDE;
};

class CopresencePrivateSendSamplesFunction : public CopresencePrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendSamples",
                             COPRESENCEPRIVATE_SENDSAMPLES);

 protected:
  virtual ~CopresencePrivateSendSamplesFunction() {}
  virtual ExtensionFunction::ResponseAction Run() OVERRIDE;
};

class CopresencePrivateSendDetectFunction : public CopresencePrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendDetect",
                             COPRESENCEPRIVATE_SENDDETECT);

 protected:
  virtual ~CopresencePrivateSendDetectFunction() {}
  virtual ExtensionFunction::ResponseAction Run() OVERRIDE;
};

class CopresencePrivateSendInitializedFunction
    : public CopresencePrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendInitialized",
                             COPRESENCEPRIVATE_SENDINITIALIZED);

 protected:
  virtual ~CopresencePrivateSendInitializedFunction() {}
  virtual ExtensionFunction::ResponseAction Run() OVERRIDE;
};

// This will go away once we check in the code for the CopresenceAPI BCK
// service which lets us inject a whispernet client.
void SetWhispernetClientForTesting(copresence::WhispernetClient* client);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_PRIVATE_COPRESENCE_PRIVATE_API_H_
