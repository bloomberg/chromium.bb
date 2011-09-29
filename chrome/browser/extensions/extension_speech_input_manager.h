// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SPEECH_INPUT_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SPEECH_INPUT_MANAGER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include <string>

class Extension;
class NotificationDetails;
class NotificationSource;
class Profile;

// Manages the speech input requests and responses from the extensions
// associated to the given profile.
class ExtensionSpeechInputManager : public NotificationObserver,
    public base::RefCountedThreadSafe<ExtensionSpeechInputManager> {
 public:
  // Should not be used directly. Managed by a ProfileKeyedServiceFactory.
  explicit ExtensionSpeechInputManager(Profile* profile);

  // Returns the corresponding manager for the given profile, creating
  // a new one if required.
  static ExtensionSpeechInputManager* GetForProfile(Profile* profile);

  // Initialize the ProfileKeyedServiceFactory.
  static void InitializeFactory();

  // Methods from NotificationObserver.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Request to start speech recognition for the provided extension.
  void Start(const Extension *extension);

  // Request to stop an ongoing speech recognition.
  void Stop(const Extension *extension);

  // Called by internal ProfileKeyedService class.
  void ShutdownOnUIThread();

 private:
  void ExtensionUnloaded(const std::string& id);

  friend class base::RefCountedThreadSafe<ExtensionSpeechInputManager>;
  class Factory;

  virtual ~ExtensionSpeechInputManager();

  Profile* profile_;
  scoped_refptr<Extension> extension_;
  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SPEECH_INPUT_MANAGER_H_
