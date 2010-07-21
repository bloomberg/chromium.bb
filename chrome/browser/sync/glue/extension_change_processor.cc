// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_change_processor.h"

#include <sstream>
#include <string>

#include "base/logging.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/extension_model_associator.h"
#include "chrome/browser/sync/glue/extension_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"

namespace browser_sync {

ExtensionChangeProcessor::ExtensionChangeProcessor(
    UnrecoverableErrorHandler* error_handler,
    ExtensionModelAssociator* extension_model_associator)
    : ChangeProcessor(error_handler),
      extension_model_associator_(extension_model_associator),
      profile_(NULL) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(error_handler);
  DCHECK(extension_model_associator_);
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
  switch (type.value) {
    case NotificationType::EXTENSION_LOADED:
    case NotificationType::EXTENSION_UPDATE_DISABLED:
    case NotificationType::EXTENSION_UNLOADED:
    case NotificationType::EXTENSION_UNLOADED_DISABLED: {
      DCHECK_EQ(Source<Profile>(source).ptr(), profile_);
      Extension* extension = Details<Extension>(details).ptr();
      CHECK(extension);
      // Ignore non-syncable extensions.
      if (!IsExtensionSyncable(*extension)) {
        return;
      }
      const std::string& id = extension->id();
      LOG(INFO) << "Got change notification of type " << type.value
                << " for extension " << id;
      if (!extension_model_associator_->OnClientUpdate(id)) {
        std::string error = std::string("Client update failed for ") + id;
        error_handler()->OnUnrecoverableError(FROM_HERE, error);
        return;
      }
      break;
    }
    default:
      LOG(DFATAL) << "Received unexpected notification of type "
                  << type.value;
      break;
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
        DCHECK_EQ(node.GetModelType(), syncable::EXTENSIONS);
        const sync_pb::ExtensionSpecifics& specifics =
            node.GetExtensionSpecifics();
        if (!IsExtensionSpecificsValid(specifics)) {
          std::string error =
              std::string("Invalid server specifics: ") +
              ExtensionSpecificsToString(specifics);
          error_handler()->OnUnrecoverableError(FROM_HERE, error);
          return;
        }
        StopObserving();
        extension_model_associator_->OnServerUpdate(specifics);
        StartObserving();
        break;
      }
      case sync_api::SyncManager::ChangeRecord::ACTION_DELETE: {
        StopObserving();
        if (change.specifics.HasExtension(sync_pb::extension)) {
          extension_model_associator_->OnServerRemove(
              change.specifics.GetExtension(sync_pb::extension).id());
        } else {
          std::stringstream error;
          error << "Could not get extension ID for deleted node "
                << change.id;
          error_handler()->OnUnrecoverableError(FROM_HERE, error.str());
          LOG(DFATAL) << error.str();
        }
        StartObserving();
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
