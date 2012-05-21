// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/session_favicon_source.h"

#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/url_constants.h"

using browser_sync::SessionModelAssociator;

SessionFaviconSource::SessionFaviconSource(Profile* profile)
    : FaviconSource(profile,
                    FaviconSource::FAVICON,
                    chrome::kChromeUISessionFaviconHost) {
}

SessionFaviconSource::~SessionFaviconSource() {
}

void SessionFaviconSource::StartDataRequest(const std::string& path,
                                            bool is_incognito,
                                            int request_id) {
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  SessionModelAssociator* associator = sync_service ?
      sync_service->GetSessionModelAssociator() : NULL;

  std::string favicon_data;
  if (associator &&
      associator->GetSyncedFaviconForPageURL(path, &favicon_data)) {
    scoped_refptr<base::RefCountedString> response =
        new base::RefCountedString();
    response->data() = favicon_data;
    SendResponse(request_id, response);
  } else {
    FaviconSource::StartDataRequest(path, is_incognito, request_id);
  }
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
