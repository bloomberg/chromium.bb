// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUDIO_AUDIO_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUDIO_AUDIO_API_H_

#include "chrome/browser/extensions/api/audio/audio_service.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace extensions {

class AudioService;

class AudioAPI : public BrowserContextKeyedAPI, public AudioService::Observer {
 public:
  explicit AudioAPI(content::BrowserContext* context);
  virtual ~AudioAPI();

  AudioService* GetService() const;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<AudioAPI>* GetFactoryInstance();

  // AudioService::Observer implementation.
  virtual void OnDeviceChanged() OVERRIDE;

 private:
  friend class BrowserContextKeyedAPIFactory<AudioAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "AudioAPI";
  }

  content::BrowserContext* const browser_context_;
  AudioService* service_;
};

class AudioGetInfoFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.getInfo",
                             AUDIO_GETINFO);

 protected:
  virtual ~AudioGetInfoFunction() {}
  virtual bool RunAsync() OVERRIDE;

 private:
  void OnGetInfoCompleted(const OutputInfo& output_info,
                          const InputInfo& input_info,
                          bool success);
};

class AudioSetActiveDevicesFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.setActiveDevices",
                             AUDIO_SETACTIVEDEVICES);

 protected:
  virtual ~AudioSetActiveDevicesFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class AudioSetPropertiesFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.setProperties",
                             AUDIO_SETPROPERTIES);

 protected:
  virtual ~AudioSetPropertiesFunction() {}
  virtual bool RunSync() OVERRIDE;
};


}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUDIO_AUDIO_API_H_
