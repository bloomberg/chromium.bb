// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_url_operation.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

namespace image_writer_api = extensions::api::image_writer_private;

namespace extensions {
namespace image_writer {

using content::BrowserThread;

OperationManager::OperationManager(Profile* profile)
    : profile_(profile),
      weak_factory_(this) {
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

OperationManager::~OperationManager() {
}

void OperationManager::Shutdown() {
  for (OperationMap::iterator iter = operations_.begin();
       iter != operations_.end();
       iter++) {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&Operation::Abort,
                                       iter->second));
  }
}

void OperationManager::StartWriteFromUrl(
    const ExtensionId& extension_id,
    GURL url,
    content::RenderViewHost* rvh,
    const std::string& hash,
    bool saveImageAsDownload,
    const std::string& storage_unit_id,
    const Operation::StartWriteCallback& callback) {

  OperationMap::iterator existing_operation = operations_.find(extension_id);

  if (existing_operation != operations_.end()) {
    return callback.Run(false,
                        error::kOperationAlreadyInProgress);
  }

  scoped_refptr<Operation> operation(
      new WriteFromUrlOperation(weak_factory_.GetWeakPtr(),
                                extension_id,
                                rvh,
                                url,
                                hash,
                                saveImageAsDownload,
                                storage_unit_id));

  operations_[extension_id] = operation;

  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&Operation::Start,
                                     operation.get()));

  callback.Run(true, "");
}

void OperationManager::StartWriteFromFile(
    const ExtensionId& extension_id,
    const std::string& storage_unit_id,
    const Operation::StartWriteCallback& callback) {
  // Currently unimplemented.
  callback.Run(false, error::kFileOperationsNotImplemented);
}

void OperationManager::CancelWrite(
    const ExtensionId& extension_id,
    const Operation::CancelWriteCallback& callback) {
  Operation* existing_operation = GetOperation(extension_id);

  if (existing_operation == NULL) {
    callback.Run(false, error::kNoOperationInProgress);
  } else {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&Operation::Cancel, existing_operation));
    DeleteOperation(extension_id);
    callback.Run(true, "");
  }
}

void OperationManager::OnProgress(const ExtensionId& extension_id,
                                  image_writer_api::Stage stage,
                                  int progress) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(2) << "progress - " << stage << " at " << progress << "%";

  image_writer_api::ProgressInfo info;
  info.stage = stage;
  info.percent_complete = progress;

  scoped_ptr<base::ListValue> args(
      image_writer_api::OnWriteProgress::Create(info));
  scoped_ptr<Event> event(new Event(
      image_writer_api::OnWriteProgress::kEventName, args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
}

void OperationManager::OnComplete(const ExtensionId& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<base::ListValue> args(image_writer_api::OnWriteComplete::Create());
  scoped_ptr<Event> event(new Event(
      image_writer_api::OnWriteComplete::kEventName, args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());

  DeleteOperation(extension_id);
}

void OperationManager::OnError(const ExtensionId& extension_id,
                               image_writer_api::Stage stage,
                               int progress,
                               const std::string& error_message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  image_writer_api::ProgressInfo info;

  DLOG(ERROR) << "ImageWriter error: " << error_message;

  // TODO(haven): Set up error messages. http://crbug.com/284880
  info.stage = stage;
  info.percent_complete = progress;

  scoped_ptr<base::ListValue> args(
      image_writer_api::OnWriteError::Create(info, error_message));
  scoped_ptr<Event> event(new Event(
      image_writer_api::OnWriteError::kEventName, args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());

  DeleteOperation(extension_id);
}

Operation* OperationManager::GetOperation(const ExtensionId& extension_id) {
  OperationMap::iterator existing_operation = operations_.find(extension_id);

  if (existing_operation == operations_.end())
    return NULL;
  return existing_operation->second.get();
}

void OperationManager::DeleteOperation(const ExtensionId& extension_id) {
  OperationMap::iterator existing_operation = operations_.find(extension_id);
  if (existing_operation != operations_.end()) {
    operations_.erase(existing_operation);
  }
}

void OperationManager::Observe(int type,
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

OperationManager* OperationManager::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<OperationManager>::
      GetForProfile(profile);
}

static base::LazyInstance<ProfileKeyedAPIFactory<OperationManager> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

ProfileKeyedAPIFactory<OperationManager>*
    OperationManager::GetFactoryInstance() {
  return &g_factory.Get();
}


}  // namespace image_writer
}  // namespace extensions
