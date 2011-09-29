// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_speech_input_manager.h"

#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/common/notification_service.h"

namespace {

// Wrap an ExtensionSpeechInputManager using scoped_refptr to avoid
// assertion failures on destruction because of not using release().
class ExtensionSpeechInputManagerWrapper : public ProfileKeyedService {
 public:
  explicit ExtensionSpeechInputManagerWrapper(
      ExtensionSpeechInputManager* manager)
      : manager_(manager) {}

  virtual ~ExtensionSpeechInputManagerWrapper() {}

  ExtensionSpeechInputManager* manager() const { return manager_.get(); }

 private:
  // Methods from ProfileKeyedService.
  virtual void Shutdown() OVERRIDE {
    manager()->ShutdownOnUIThread();
  }

  scoped_refptr<ExtensionSpeechInputManager> manager_;
};

}

// Factory for ExtensionSpeechInputManagers as profile keyed services.
class ExtensionSpeechInputManager::Factory : public ProfileKeyedServiceFactory {
 public:
  static void Initialize();
  static Factory* GetInstance();

  ExtensionSpeechInputManagerWrapper* GetForProfile(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<Factory>;

  Factory();
  virtual ~Factory();

  // ProfileKeyedServiceFactory methods:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const;
  virtual bool ServiceRedirectedInIncognito() { return false; }
  virtual bool ServiceIsNULLWhileTesting() { return true; }
  virtual bool ServiceIsCreatedWithProfile() { return true; }

  DISALLOW_COPY_AND_ASSIGN(Factory);
};

void ExtensionSpeechInputManager::Factory::Initialize() {
  GetInstance();
}

ExtensionSpeechInputManager::Factory*
    ExtensionSpeechInputManager::Factory::GetInstance() {
  return Singleton<ExtensionSpeechInputManager::Factory>::get();
}

ExtensionSpeechInputManagerWrapper*
    ExtensionSpeechInputManager::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<ExtensionSpeechInputManagerWrapper*>(
      GetServiceForProfile(profile, true));
}

ExtensionSpeechInputManager::Factory::Factory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

ExtensionSpeechInputManager::Factory::~Factory() {
}

ProfileKeyedService*
    ExtensionSpeechInputManager::Factory::BuildServiceInstanceFor(
    Profile* profile) const {
  scoped_refptr<ExtensionSpeechInputManager> manager(
      new ExtensionSpeechInputManager(profile));
  return new ExtensionSpeechInputManagerWrapper(manager);
}

ExtensionSpeechInputManager::ExtensionSpeechInputManager(Profile* profile)
    : profile_(profile),
      extension_(NULL) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 Source<Profile>(profile_));
}

ExtensionSpeechInputManager::~ExtensionSpeechInputManager() {
}

ExtensionSpeechInputManager* ExtensionSpeechInputManager::GetForProfile(
    Profile* profile) {
  ExtensionSpeechInputManagerWrapper *wrapper =
      Factory::GetInstance()->GetForProfile(profile);
  if (!wrapper)
    return NULL;
  return wrapper->manager();
}

void ExtensionSpeechInputManager::InitializeFactory() {
  Factory::Initialize();
}

void ExtensionSpeechInputManager::Observe(int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED)
    ExtensionUnloaded(Details<UnloadedExtensionInfo>(details)->extension->id());
  else
    NOTREACHED();
}

void ExtensionSpeechInputManager::ShutdownOnUIThread() {
  // TODO(leandrogracia): Force stop to speech recognition if active.
  registrar_.RemoveAll();
  profile_ = NULL;
}

void ExtensionSpeechInputManager::ExtensionUnloaded(const std::string& id) {
  // TODO(leandrogracia): Force stop to speech recognition if the extension
  // is currently using the API.
}

void ExtensionSpeechInputManager::Start(const Extension *extension) {
  // TODO(leandrogracia): Start speech recognition.
}

void ExtensionSpeechInputManager::Stop(const Extension *extension) {
  // TODO(leandrogracia): Stop speech recognition.
}

