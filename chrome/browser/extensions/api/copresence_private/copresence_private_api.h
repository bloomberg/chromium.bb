// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_PRIVATE_COPRESENCE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_PRIVATE_COPRESENCE_PRIVATE_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace audio_modem {
class WhispernetClient;
}

namespace extensions {

namespace copresence_private {

// Register a client to receive events from Whispernet.
const std::string
RegisterWhispernetClient(audio_modem::WhispernetClient* client);

}  // namespace copresence_private

class CopresencePrivateSendFoundFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendFound",
                             COPRESENCEPRIVATE_SENDFOUND);

 protected:
  ~CopresencePrivateSendFoundFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

class CopresencePrivateSendSamplesFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendSamples",
                             COPRESENCEPRIVATE_SENDSAMPLES);

 protected:
  ~CopresencePrivateSendSamplesFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

class CopresencePrivateSendDetectFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendDetect",
                             COPRESENCEPRIVATE_SENDDETECT);

 protected:
  ~CopresencePrivateSendDetectFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

class CopresencePrivateSendInitializedFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresencePrivate.sendInitialized",
                             COPRESENCEPRIVATE_SENDINITIALIZED);

 protected:
  ~CopresencePrivateSendInitializedFunction() override {}
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_PRIVATE_COPRESENCE_PRIVATE_API_H_
