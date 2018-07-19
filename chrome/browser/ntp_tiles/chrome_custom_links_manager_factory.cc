// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_tiles/chrome_custom_links_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/custom_links_manager_impl.h"

std::unique_ptr<ntp_tiles::CustomLinksManager>
ChromeCustomLinksManagerFactory::NewForProfile(Profile* profile) {
  if (!ntp_tiles::IsCustomLinksEnabled()) {
    return nullptr;
  }
  return std::make_unique<ntp_tiles::CustomLinksManagerImpl>(
      profile->GetPrefs());
}
