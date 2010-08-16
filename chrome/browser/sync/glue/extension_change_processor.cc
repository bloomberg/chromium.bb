// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_change_processor.h"

#include <sstream>
#include <string>

#include "base/logging.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/sync/glue/extension_sync.h"
#include "chrome/browser/sync/glue/extension_util.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"

namespace browser_sync {

ExtensionChangeProcessor::ExtensionChangeProcessor(
    const ExtensionSyncTraits& traits,
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      traits_(traits),
      profile_(NULL) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(error_handler);
}

ExtensionChangeProcessor::~ExtensionChangeProcessor() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

// TODO(akalin): We need to make sure events we receive from either
// the browser or the syncapi are done in order; this is tricky since
// some events (e.g., extension installation) are done asynchronously.

void ExtensionChangeProcessor::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(running());
  DCHECK(profile_);
  if ((type != NotificationType::EXTENSION_LOADED) &&
      (type != NotificationType::EXTENSION_UPDATE_DISABLED) &&
      (type != NotificationType::EXTENSION_UNLOADED) &&
      (type != NotificationType::EXTENSION_UNLOADED_DISABLED)) {
    LOG(DFATAL) << "Received unexpected notification of type "
                << type.value;
    return;
  }
  DCHECK_EQ(Source<Profile>(source).ptr(), profile_);
  const Extension* extension = Details<Extension>(details).ptr();
  CHECK(extension);
  // Ignore non-syncable extensions.
  if (!IsExtensionValidAndSyncable(
          *extension, traits_.allowed_extension_types)) {
    return;
  }
  const std::string& id = extension->id();
  LOG(INFO) << "Got notification of type " << type.value
            << " for extension " << id;
  ExtensionsService* extensions_service =
      GetExtensionsServiceFromProfile(profile_);
  // Whether an extension is loaded or not isn't an indicator of
  // whether it's installed or not; some extension actions unload and
  // then reload an extension to force listeners to update.
  bool extension_installed =
      (extensions_service->GetExtensionById(id, true) != NULL);
  if (extension_installed) {
    LOG(INFO) << "Extension " << id
              << " is installed; updating server data";
    std::string error;
    if (!UpdateServerData(traits_, *extension,
                          profile_->GetProfileSyncService(), &error)) {
      error_handler()->OnUnrecoverableError(FROM_HERE, error);
    }
  } else {
    LOG(INFO) << "Extension " << id
              << " is not installed; removing server data";
    RemoveServerData(traits_, *extension,
                     profile_->GetProfileSyncService());
  }
}

void ExtensionChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!running()) {
    return;
  }
  ExtensionsService* extensions_service =
      GetExtensionsServiceFromProfile(profile_);
  for (int i = 0; i < change_count; ++i) {
    const sync_api::SyncManager::ChangeRecord& change = changes[i];
    switch (change.action) {
      case sync_api::SyncManager::ChangeRecord::ACTION_ADD:
      case sync_api::SyncManager::ChangeRecord::ACTION_UPDATE: {
        sync_api::ReadNode node(trans);
        if (!node.InitByIdLookup(change.id)) {
          std::stringstream error;
          error << "Extension node lookup failed for change " << change.id
                << " of action type " << change.action;
          error_handler()->OnUnrecoverableError(FROM_HERE, error.str());
          return;
        }
        DCHECK_EQ(node.GetModelType(), traits_.model_type);
        const sync_pb::ExtensionSpecifics& specifics =
            (*traits_.extension_specifics_getter)(node);
        if (!IsExtensionSpecificsValid(specifics)) {
          std::string error =
              std::string("Invalid server specifics: ") +
              ExtensionSpecificsToString(specifics);
          error_handler()->OnUnrecoverableError(FROM_HERE, error);
          return;
        }
        StopObserving();
        UpdateClient(traits_, specifics, extensions_service);
        StartObserving();
        break;
      }
      case sync_api::SyncManager::ChangeRecord::ACTION_DELETE: {
        sync_pb::ExtensionSpecifics specifics;
        if ((*traits_.extension_specifics_entity_getter)(
                change.specifics, &specifics)) {
          StopObserving();
          RemoveFromClient(traits_, specifics.id(), extensions_service);
          StartObserving();
        } else {
          std::stringstream error;
          error << "Could not get extension ID for deleted node "
                << change.id;
          error_handler()->OnUnrecoverableError(FROM_HERE, error.str());
          LOG(DFATAL) << error.str();
        }
        break;
      }
    }
  }
}

void ExtensionChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(profile);
  profile_ = profile;
  StartObserving();
}

void ExtensionChangeProcessor::StopImpl() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  StopObserving();
  profile_ = NULL;
}

void ExtensionChangeProcessor::StartObserving() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(profile_);
  LOG(INFO) << "Observing EXTENSION_LOADED, EXTENSION_UPDATE_DISABLED, "
            << "EXTENSION_UNLOADED, and EXTENSION_UNLOADED_DISABLED";
  notification_registrar_.Add(
      this, NotificationType::EXTENSION_LOADED,
      Source<Profile>(profile_));
  // Despite the name, this notification is exactly like
  // EXTENSION_LOADED but with an initial state of DISABLED.
  //
  // TODO(akalin): See if the themes change processor needs to listen
  // to any of these, too.
  notification_registrar_.Add(
      this, NotificationType::EXTENSION_UPDATE_DISABLED,
      Source<Profile>(profile_));
  notification_registrar_.Add(
      this, NotificationType::EXTENSION_UNLOADED,
      Source<Profile>(profile_));
  notification_registrar_.Add(
      this, NotificationType::EXTENSION_UNLOADED_DISABLED,
      Source<Profile>(profile_));
}

void ExtensionChangeProcessor::StopObserving() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(profile_);
  LOG(INFO) << "Unobserving all notifications";
  notification_registrar_.RemoveAll();
}

}  // namespace browser_sync
