// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_MIC_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_MIC_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/permissions/permission_context_factory_base.h"
#include "components/content_settings/core/common/content_settings_types.h"

class MediaStreamDevicePermissionContext;
class Profile;

class MediaStreamMicPermissionContextFactory
    : public PermissionContextFactoryBase {
 public:
  static MediaStreamDevicePermissionContext* GetForProfile(Profile* profile);
  static MediaStreamMicPermissionContextFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      MediaStreamMicPermissionContextFactory>;

  MediaStreamMicPermissionContextFactory();
  ~MediaStreamMicPermissionContextFactory() override;

  // BrowserContextKeyedBaseFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamMicPermissionContextFactory);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_MIC_PERMISSION_CONTEXT_FACTORY_H_
