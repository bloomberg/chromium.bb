// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CHROME_MIDI_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_MEDIA_CHROME_MIDI_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class ChromeMIDIPermissionContext;
class Profile;

class ChromeMIDIPermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ChromeMIDIPermissionContext* GetForProfile(Profile* profile);
  static ChromeMIDIPermissionContextFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ChromeMIDIPermissionContextFactory>;

  ChromeMIDIPermissionContextFactory();
  virtual ~ChromeMIDIPermissionContextFactory();

  // BrowserContextKeyedBaseFactory methods:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeMIDIPermissionContextFactory);
};

#endif  // CHROME_BROWSER_MEDIA_CHROME_MIDI_PERMISSION_CONTEXT_FACTORY_H_
