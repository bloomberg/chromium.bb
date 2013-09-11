// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/recovery_private/error_messages.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_operation.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_operation_manager.h"
#include "chrome/browser/extensions/api/recovery_private/write_from_file_operation.h"
#include "chrome/browser/extensions/api/recovery_private/write_from_url_operation.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

namespace recovery_api = extensions::api::recovery_private;

namespace extensions {
namespace recovery {

using content::BrowserThread;

RecoveryOperationManager::RecoveryOperationManager(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::Source<Profile>(profile_));
}

RecoveryOperationManager::~RecoveryOperationManager() {
}

void RecoveryOperationManager::Shutdown() {
  for (OperationMap::iterator iter = operations_.begin();
       iter != operations_.end();
       iter++) {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&RecoveryOperation::Abort,
                                       iter->second));
  }
}

void RecoveryOperationManager::StartWriteFromUrl(
    const ExtensionId& extension_id,
    GURL url,
    content::RenderViewHost* rvh,
    const std::string& hash,
    bool saveImageAsDownload,
    const std::string& storage_unit_id,
    const RecoveryOperation::StartWriteCallback& callback) {

  OperationMap::iterator existing_operation = operations_.find(extension_id);

  if (existing_operation != operations_.end()) {
    return callback.Run(false,
                        error::kOperationAlreadyInProgress);
  }

  scoped_refptr<RecoveryOperation> operation(
      new WriteFromUrlOperation(this, extension_id, rvh, url,
                                hash, saveImageAsDownload,
                                storage_unit_id));

  operations_[extension_id] = operation;

  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&RecoveryOperation::Start,
                                     operation.get()));

  callback.Run(true, "");
}

void RecoveryOperationManager::StartWriteFromFile(
    const ExtensionId& extension_id,
    const std::string& storage_unit_id,
    const RecoveryOperation::StartWriteCallback& callback) {
  // Currently unimplemented.
  callback.Run(false, error::kFileOperationsNotImplemented);
}

void RecoveryOperationManager::CancelWrite(
    const ExtensionId& extension_id,
    const RecoveryOperation::CancelWriteCallback& callback) {
  RecoveryOperation* existing_operation = GetOperation(extension_id);

  if (existing_operation == NULL) {
    callback.Run(false, error::kNoOperationInProgress);
  } else {
    DeleteOperation(extension_id);
    callback.Run(true, "");
  }
}

void RecoveryOperationManager::OnProgress(const ExtensionId& extension_id,
                                          recovery_api::Stage stage,
                                          int progress) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  recovery_api::ProgressInfo info;
  DVLOG(2) << "progress - " << stage << " at " << progress << "%";

  info.stage = stage;
  info.percent_complete = progress;

  scoped_ptr<base::ListValue> args(
      recovery_api::OnWriteProgress::Create(info));
  scoped_ptr<Event> event(new Event(
      recovery_api::OnWriteProgress::kEventName, args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
}

void RecoveryOperationManager::OnComplete(const ExtensionId& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<base::ListValue> args(recovery_api::OnWriteComplete::Create());
  scoped_ptr<Event> event(new Event(
      recovery_api::OnWriteComplete::kEventName, args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());

  DeleteOperation(extension_id);
}

void RecoveryOperationManager::OnError(const ExtensionId& extension_id,
                                       recovery_api::Stage stage,
                                       int progress,
                                       const std::string& error_message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  recovery_api::ProgressInfo info;

  DLOG(ERROR) << "Recovery error: " << error_message;

  // TODO(haven): Set up error messages. http://crbug.com/284880

  info.stage = stage;
  info.percent_complete = progress;

  scoped_ptr<base::ListValue> args(
      recovery_api::OnWriteError::Create(info, error_message));
  scoped_ptr<Event> event(new Event(
      recovery_api::OnWriteError::kEventName, args.Pass()));

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
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&RecoveryOperation::Cancel,
                                       existing_operation->second));
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
    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
      DeleteOperation(
        content::Details<ExtensionHost>(details)->extension()->id());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      DeleteOperation(
        content::Details<ExtensionHost>(details)->extension()->id());
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
