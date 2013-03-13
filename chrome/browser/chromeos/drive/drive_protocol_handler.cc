// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_protocol_handler.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/drive/drive_url_request_job.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

namespace drive {

namespace {

// Helper function to get DriveFileSystemInterface from Profile.
DriveFileSystemInterface* GetDriveFileSystem(void* profile_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // |profile_id| needs to be checked with ProfileManager::IsValidProfile
  // before using it.
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return NULL;

  DriveSystemService* system_service =
      DriveSystemServiceFactory::FindForProfile(profile);
  return system_service ? system_service->file_system() : NULL;
}

}  // namespace

DriveProtocolHandler::DriveProtocolHandler(void* profile_id)
  : profile_id_(profile_id) {
}

DriveProtocolHandler::~DriveProtocolHandler() {
}

net::URLRequestJob* DriveProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  DVLOG(1) << "Handling url: " << request->url().spec();
  return new DriveURLRequestJob(
      base::Bind(&GetDriveFileSystem, profile_id_), request, network_delegate);
}

}  // namespace drive
