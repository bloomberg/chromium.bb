// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUDIO_AUDIO_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUDIO_AUDIO_API_H_

#include "chrome/browser/extensions/api/audio/audio_service.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class AudioService;

class AudioAPI : public ProfileKeyedAPI,
                 public AudioService::Observer {
 public:
  explicit AudioAPI(Profile* profile);
  virtual ~AudioAPI();

  AudioService* GetService() const;

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<AudioAPI>* GetFactoryInstance();

  // AudioService::Observer implementation.
  virtual void OnDeviceChanged() OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<AudioAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "AudioAPI";
  }

  Profile* const profile_;
  AudioService* service_;
};

class AudioGetInfoFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.getInfo",
                             AUDIO_GETINFO);

 protected:
  virtual ~AudioGetInfoFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnGetInfoCompleted(const OutputInfo& output_info,
                          const InputInfo& input_info,
                          bool success);
};

class AudioSetActiveDevicesFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.setActiveDevices",
                             AUDIO_SETACTIVEDEVICES);

 protected:
  virtual ~AudioSetActiveDevicesFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class AudioSetPropertiesFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("audio.setProperties",
                             AUDIO_SETPROPERTIES);

 protected:
  virtual ~AudioSetPropertiesFunction() {}
  virtual bool RunImpl() OVERRIDE;
};


}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUDIO_AUDIO_API_H_
