// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_operation.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_operation_manager.h"
#include "chrome/browser/extensions/api/recovery_private/write_from_file_operation.h"
#include "chrome/browser/extensions/api/recovery_private/write_from_url_operation.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

namespace recovery_private = extensions::api::recovery_private;

namespace recovery {

namespace {

recovery_api::Stage nextStage(recovery_api::Stage stage) {
  if (stage == recovery_api::STAGE_CONFIRMATION) {
    return recovery_api::STAGE_DOWNLOAD;
  } else if (stage == recovery_api::STAGE_DOWNLOAD) {
    return recovery_api::STAGE_VERIFYDOWNLOAD;
  } else if (stage == recovery_api::STAGE_VERIFYDOWNLOAD) {
    return recovery_api::STAGE_WRITE;
  } else if (stage == recovery_api::STAGE_WRITE) {
    return recovery_api::STAGE_VERIFYWRITE;
  } else {
    return recovery_api::STAGE_NONE;
  }
}

} // namespace


RecoveryOperationManager::RecoveryOperationManager(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::Source<Profile>(profile_));
}

RecoveryOperationManager::~RecoveryOperationManager() {
}

void RecoveryOperationManager::Shutdown() {
  OperationMap::iterator iter;

  for (iter = operations_.begin();
       iter != operations_.end();
       ++iter) {
    iter->second->Abort();
  }
}

void RecoveryOperationManager::StartWriteFromUrl(
    const ExtensionId& extension_id,
    const GURL& url,
    scoped_ptr<std::string> hash,
    bool saveImageAsDownload,
    const std::string& storage_unit_id,
    const StartWriteCallback& callback) {
  OperationMap::iterator existing_operation = operations_.find(extension_id);

  if (existing_operation != operations_.end())
    return callback.Run(false);

  linked_ptr<RecoveryOperation> operation(
      new WriteFromUrlOperation(this, extension_id, url, hash.Pass(),
                                saveImageAsDownload, storage_unit_id));

  operations_.insert(make_pair(extension_id, operation));

  operation->Start(callback);
}

void RecoveryOperationManager::StartWriteFromFile(
    const ExtensionId& extension_id,
    const std::string& storage_unit_id,
    const StartWriteCallback& callback) {
  // Currently unimplemented.
  callback.Run(false);
}

void RecoveryOperationManager::CancelWrite(
    const ExtensionId& extension_id,
    const CancelWriteCallback& callback) {
  RecoveryOperation* existing_operation = GetOperation(extension_id);

  if (existing_operation == NULL) {
    callback.Run(false);
  } else {
    existing_operation->Cancel(callback);
    DeleteOperation(extension_id);
  }
}

void RecoveryOperationManager::OnProgress(const ExtensionId& extension_id,
                                          recovery_api::Stage stage,
                                          int progress) {
  recovery_api::ProgressInfo info;

  info.stage = stage;
  info.percent_complete = progress;

  scoped_ptr<base::ListValue> args(recovery_api::OnWriteProgress::Create(info));
  scoped_ptr<Event> event(new Event(
      recovery_private::OnWriteProgress::kEventName, args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
}

void RecoveryOperationManager::OnComplete(const ExtensionId& extension_id) {

  scoped_ptr<base::ListValue> args(recovery_api::OnWriteComplete::Create());
  scoped_ptr<Event> event(new Event(
      recovery_private::OnWriteComplete::kEventName, args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());

  DeleteOperation(extension_id);
}

void RecoveryOperationManager::OnError(const ExtensionId& extension_id,
                                       recovery_api::Stage stage,
                                       int progress) {
  recovery_api::ProgressInfo info;

  info.stage = stage;
  info.percent_complete = progress;

  scoped_ptr<base::ListValue> args(recovery_api::OnWriteError::Create(info));
  scoped_ptr<Event> event(new Event(recovery_private::OnWriteError::kEventName,
                                    args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());

  DeleteOperation(extension_id);
}

RecoveryOperation* RecoveryOperationManager::GetOperation(
    const ExtensionId& extension_id) {
  OperationMap::iterator existing_operation = operations_.find(extension_id);

  if (existing_operation == operations_.end())
    return NULL;

  return existing_operation->second.get();
}

void RecoveryOperationManager::DeleteOperation(
    const ExtensionId& extension_id) {
  OperationMap::iterator existing_operation = operations_.find(extension_id);
  if (existing_operation != operations_.end()) {
    operations_.erase(existing_operation);
  }
}

void RecoveryOperationManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      DeleteOperation(content::Details<const Extension>(details).ptr()->id());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      DeleteOperation(content::Details<const Extension>(details).ptr()->id());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED: {
      DeleteOperation(content::Details<const Extension>(details).ptr()->id());
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

RecoveryOperationManager* RecoveryOperationManager::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<RecoveryOperationManager>::
      GetForProfile(profile);
}

static base::LazyInstance<ProfileKeyedAPIFactory<RecoveryOperationManager> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

ProfileKeyedAPIFactory<RecoveryOperationManager>*
    RecoveryOperationManager::GetFactoryInstance() {
  return &g_factory.Get();
}


} // namespace recovery
} // namespace extensions
