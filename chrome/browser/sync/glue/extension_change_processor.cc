// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_change_processor.h"

#include <sstream>
#include <string>

#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/extension_sync.h"
#include "chrome/browser/sync/glue/extension_util.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

namespace browser_sync {

ExtensionChangeProcessor::ExtensionChangeProcessor(
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      traits_(GetExtensionSyncTraits()),
      profile_(NULL),
      extension_service_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error_handler);
}

ExtensionChangeProcessor::~ExtensionChangeProcessor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

// TODO(akalin): We need to make sure events we receive from either
// the browser or the syncapi are done in order; this is tricky since
// some events (e.g., extension installation) are done asynchronously.

void ExtensionChangeProcessor::Observe(int type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(running());
  DCHECK(profile_);
  if ((type != chrome::NOTIFICATION_EXTENSION_LOADED) &&
      (type != chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED) &&
      (type != chrome::NOTIFICATION_EXTENSION_UNLOADED)) {
    LOG(DFATAL) << "Received unexpected notification of type "
                << type;
    return;
  }

  // Filter out unhandled extensions first.
  DCHECK_EQ(Source<Profile>(source).ptr(), profile_);
  const Extension& extension =
      (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) ?
      *Details<UnloadedExtensionInfo>(details)->extension :
      *Details<const Extension>(details).ptr();
  if (!traits_.is_valid_and_syncable(extension)) {
    return;
  }

  const std::string& id = extension.id();

  // Then handle extension uninstalls.
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    const UnloadedExtensionInfo& info =
        *Details<UnloadedExtensionInfo>(details).ptr();
    if (info.reason == extension_misc::UNLOAD_REASON_UNINSTALL) {
      VLOG(1) << "Removing server data for uninstalled extension " << id
              << " of type " << info.extension->GetType();
      RemoveServerData(traits_, id, share_handle());
      return;
    }
  }

  VLOG(1) << "Updating server data for extension " << id
          << " (notification type = " << type << ")";
  std::string error;
  if (!UpdateServerData(traits_, extension, *extension_service_,
                        share_handle(), &error)) {
    error_handler()->OnUnrecoverableError(FROM_HERE, error);
  }
}

void ExtensionChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!running()) {
    return;
  }
  for (int i = 0; i < change_count; ++i) {
    const sync_api::SyncManager::ChangeRecord& change = changes[i];
    sync_pb::ExtensionSpecifics specifics;
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
        specifics = (*traits_.extension_specifics_getter)(node);
        break;
      }
      case sync_api::SyncManager::ChangeRecord::ACTION_DELETE: {
        if (!(*traits_.extension_specifics_entity_getter)(
                change.specifics, &specifics)) {
          std::stringstream error;
          error << "Could not get extension specifics from deleted node "
                << change.id;
          error_handler()->OnUnrecoverableError(FROM_HERE, error.str());
          LOG(DFATAL) << error.str();
        }
        break;
      }
    }
    ExtensionSyncData sync_data;
    if (!SpecificsToSyncData(specifics, &sync_data)) {
      // TODO(akalin): Should probably recover or drop.
      std::string error =
          std::string("Invalid server specifics: ") +
          ExtensionSpecificsToString(specifics);
      error_handler()->OnUnrecoverableError(FROM_HERE, error);
      return;
    }
    sync_data.uninstalled =
        (change.action == sync_api::SyncManager::ChangeRecord::ACTION_DELETE);
    StopObserving();
    extension_service_->ProcessSyncData(sync_data,
                                        traits_.is_valid_and_syncable);
    StartObserving();
  }
}

void ExtensionChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = profile;
  extension_service_ = profile_->GetExtensionService();
  DCHECK(profile_);
  DCHECK(extension_service_);
  StartObserving();
}

void ExtensionChangeProcessor::StopImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  StopObserving();
  profile_ = NULL;
  extension_service_ = NULL;
}

void ExtensionChangeProcessor::StartObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);

  notification_registrar_.Add(
      this, chrome::NOTIFICATION_EXTENSION_LOADED,
      Source<Profile>(profile_));
  // Despite the name, this notification is exactly like
  // EXTENSION_LOADED but with an initial state of DISABLED.
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
      Source<Profile>(profile_));

  notification_registrar_.Add(
      this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
      Source<Profile>(profile_));
}

void ExtensionChangeProcessor::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  VLOG(1) << "Unobserving all notifications";
  notification_registrar_.RemoveAll();
}

}  // namespace browser_sync
