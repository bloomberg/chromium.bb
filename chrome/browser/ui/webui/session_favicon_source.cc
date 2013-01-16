// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/session_favicon_source.h"

#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"

using browser_sync::SessionModelAssociator;

SessionFaviconSource::SessionFaviconSource(Profile* profile)
    : FaviconSource(profile, FaviconSource::FAVICON) {
}

SessionFaviconSource::~SessionFaviconSource() {
}

std::string SessionFaviconSource::GetSource() {
  return chrome::kChromeUISessionFaviconHost;
}

std::string SessionFaviconSource::GetMimeType(const std::string&) const {
  return "image/png";
}

bool SessionFaviconSource::ShouldReplaceExistingSource() const {
  // Leave the existing DataSource in place, otherwise we'll drop any pending
  // requests on the floor.
  return false;
}

bool SessionFaviconSource::AllowCaching() const {
  // Prevent responses from being cached, otherwise session favicons won't
  // update in a timely manner.
  return false;
}

bool SessionFaviconSource::HandleMissingResource(const IconRequest& request) {
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  SessionModelAssociator* associator = sync_service ?
      sync_service->GetSessionModelAssociator() : NULL;

  std::string favicon_data;
  if (associator &&
      associator->GetSyncedFaviconForPageURL(request.request_path,
                                             &favicon_data)) {
    scoped_refptr<base::RefCountedString> response =
        new base::RefCountedString();
    response->data() = favicon_data;
    request.callback.Run(response);
    return true;
  }
  return false;
}
