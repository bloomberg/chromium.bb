// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CHROME_MIDI_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_MEDIA_CHROME_MIDI_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class ChromeMidiPermissionContext;
class Profile;

class ChromeMidiPermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ChromeMidiPermissionContext* GetForProfile(Profile* profile);
  static ChromeMidiPermissionContextFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ChromeMidiPermissionContextFactory>;

  ChromeMidiPermissionContextFactory();
  virtual ~ChromeMidiPermissionContextFactory();

  // BrowserContextKeyedBaseFactory methods:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeMidiPermissionContextFactory);
};

#endif  // CHROME_BROWSER_MEDIA_CHROME_MIDI_PERMISSION_CONTEXT_FACTORY_H_
