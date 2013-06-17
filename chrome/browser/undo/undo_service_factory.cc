// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/undo/undo_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/undo/undo_service.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

// static
UndoService* UndoServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UndoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
UndoServiceFactory* UndoServiceFactory::GetInstance() {
  return Singleton<UndoServiceFactory>::get();
}

UndoServiceFactory::UndoServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UndoService",
          BrowserContextDependencyManager::GetInstance()) {
}

UndoServiceFactory::~UndoServiceFactory() {
}

BrowserContextKeyedService* UndoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new UndoService(profile);
}
