// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/provided_file_systems_ui.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

using content::BrowserThread;

namespace chromeos {

namespace {

const char kKeyId[] = "id";
const char kKeyEventType[] = "eventType";
const char kKeyRequestType[] = "requestType";
const char kKeyTime[] = "time";
const char kKeyHasMore[] = "hasMore";
const char kKeyError[] = "error";
const char kKeyExecutionTime[] = "executionTime";
const char kKeyValueSize[] = "valueSize";

const char kKeyName[] = "name";
const char kKeyExtensionId[] = "extensionId";
const char kKeyMountPath[] = "mountPath";
const char kKeyActiveRequests[] = "activeRequests";

const char kRequestCreated[] = "created";
const char kRequestDestroyed[] = "destroyed";
const char kRequestExecuted[] = "executed";
const char kRequestFulfilled[] = "fulfilled";
const char kRequestRejected[] = "rejected";
const char kRequestTimeouted[] = "timeouted";

const char kFunctionOnRequestEvent[] = "onRequestEvent";
const char kFunctionUpdateFileSystems[] = "updateFileSystems";
const char kFunctionSelectFileSystem[] = "selectFileSystem";

// Creates a dictionary holding common fields for the onRequest* events.
scoped_ptr<base::DictionaryValue> CreateRequestEvent(const std::string& type,
                                                     int request_id) {
  scoped_ptr<base::DictionaryValue> event(new base::DictionaryValue);
  event->SetInteger(kKeyId, request_id);
  event->SetString(kKeyEventType, type);
  event->SetDouble(kKeyTime, base::Time::Now().ToJsTime());
  return event.Pass();
}

// Gets execution time from a RequestValue instance. If the |response| doesn't
// have execution time, then returns 0.
int GetExecutionTime(const file_system_provider::RequestValue& response) {
  {
    const extensions::api::file_system_provider_internal::
        UnmountRequestedSuccess::Params* value =
            response.unmount_success_params();
    if (value)
      return value->execution_time;
  }
  {
    const extensions::api::file_system_provider_internal::
        GetMetadataRequestedSuccess::Params* value =
            response.get_metadata_success_params();
    if (value)
      return value->execution_time;
  }
  {
    const extensions::api::file_system_provider_internal::
        ReadDirectoryRequestedSuccess::Params* value =
            response.read_directory_success_params();
    if (value)
      return value->execution_time;
  }
  {
    const extensions::api::file_system_provider_internal::
        ReadFileRequestedSuccess::Params* value =
            response.read_file_success_params();
    if (value)
      return value->execution_time;
  }
  {
    const extensions::api::file_system_provider_internal::
        OperationRequestedSuccess::Params* value =
            response.operation_success_params();
    if (value)
      return value->execution_time;
  }
  {
    const extensions::api::file_system_provider_internal::
        OperationRequestedError::Params* value =
            response.operation_error_params();
    if (value)
      return value->execution_time;
  }

  return 0;
}

// Gets value size in bytes from a RequestValue instance. If not available,
// then returns 0.
int GetValueSize(const file_system_provider::RequestValue& response) {
  const extensions::api::file_system_provider_internal::
      ReadFileRequestedSuccess::Params* value =
          response.read_file_success_params();
  if (value)
    return value->data.size();

  return 0;
}

// Class to handle messages from chrome://provided-file-systems.
class ProvidedFileSystemsWebUIHandler
    : public content::WebUIMessageHandler,
      public file_system_provider::RequestManager::Observer {
 public:
  ProvidedFileSystemsWebUIHandler() : weak_ptr_factory_(this) {}

  virtual ~ProvidedFileSystemsWebUIHandler();

  // RequestManager::Observer overrides.
  virtual void OnRequestCreated(
      int request_id,
      file_system_provider::RequestType type) OVERRIDE;
  virtual void OnRequestDestroyed(int request_id) OVERRIDE;
  virtual void OnRequestExecuted(int request_id) OVERRIDE;
  virtual void OnRequestFulfilled(
      int request_id,
      const file_system_provider::RequestValue& result,
      bool has_more) OVERRIDE;
  virtual void OnRequestRejected(
      int request_id,
      const file_system_provider::RequestValue& result,
      base::File::Error error) OVERRIDE;
  virtual void OnRequestTimeouted(int request_id) OVERRIDE;

 private:
  // content::WebUIMessageHandler overrides.
  virtual void RegisterMessages() OVERRIDE;

  // Gets a file system provider service for the current profile. If not found,
  // then NULL.
  file_system_provider::Service* GetService();

  // Invoked when updating file system list is requested.
  void UpdateFileSystems(const base::ListValue* args);

  // Invoked when a file system is selected from the list.
  void SelectFileSystem(const base::ListValue* args);

  std::string selected_extension_id;
  std::string selected_file_system_id;
  base::WeakPtrFactory<ProvidedFileSystemsWebUIHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProvidedFileSystemsWebUIHandler);
};

ProvidedFileSystemsWebUIHandler::~ProvidedFileSystemsWebUIHandler() {
  // Stop observing the currently selected file system.
  file_system_provider::Service* const service = GetService();
  if (!service)
    return;

  file_system_provider::ProvidedFileSystemInterface* const file_system =
      service->GetProvidedFileSystem(selected_extension_id,
                                     selected_file_system_id);

  if (file_system) {
    file_system_provider::RequestManager* const request_manager =
        file_system->GetRequestManager();
    DCHECK(request_manager);
    request_manager->RemoveObserver(this);
  }
}

void ProvidedFileSystemsWebUIHandler::OnRequestCreated(
    int request_id,
    file_system_provider::RequestType type) {
  scoped_ptr<base::DictionaryValue> const event =
      CreateRequestEvent(kRequestCreated, request_id);
  event->SetString(kKeyRequestType,
                   file_system_provider::RequestTypeToString(type));
  web_ui()->CallJavascriptFunction(kFunctionOnRequestEvent, *event);
}

void ProvidedFileSystemsWebUIHandler::OnRequestDestroyed(int request_id) {
  scoped_ptr<base::DictionaryValue> const event =
      CreateRequestEvent(kRequestDestroyed, request_id);
  web_ui()->CallJavascriptFunction(kFunctionOnRequestEvent, *event);
}

void ProvidedFileSystemsWebUIHandler::OnRequestExecuted(int request_id) {
  scoped_ptr<base::DictionaryValue> const event =
      CreateRequestEvent(kRequestExecuted, request_id);
  web_ui()->CallJavascriptFunction(kFunctionOnRequestEvent, *event);
}

void ProvidedFileSystemsWebUIHandler::OnRequestFulfilled(
    int request_id,
    const file_system_provider::RequestValue& result,
    bool has_more) {
  scoped_ptr<base::DictionaryValue> const event =
      CreateRequestEvent(kRequestFulfilled, request_id);
  event->SetBoolean(kKeyHasMore, has_more);
  event->SetInteger(kKeyExecutionTime, GetExecutionTime(result));
  event->SetInteger(kKeyValueSize, GetValueSize(result));
  web_ui()->CallJavascriptFunction(kFunctionOnRequestEvent, *event);
}

void ProvidedFileSystemsWebUIHandler::OnRequestRejected(
    int request_id,
    const file_system_provider::RequestValue& result,
    base::File::Error error) {
  scoped_ptr<base::DictionaryValue> const event =
      CreateRequestEvent(kRequestRejected, request_id);
  event->SetString(kKeyError, base::File::ErrorToString(error));
  event->SetInteger(kKeyExecutionTime, GetExecutionTime(result));
  event->SetInteger(kKeyValueSize, GetValueSize(result));
  web_ui()->CallJavascriptFunction(kFunctionOnRequestEvent, *event);
}

void ProvidedFileSystemsWebUIHandler::OnRequestTimeouted(int request_id) {
  scoped_ptr<base::DictionaryValue> const event =
      CreateRequestEvent(kRequestTimeouted, request_id);
  web_ui()->CallJavascriptFunction(kFunctionOnRequestEvent, *event);
}

void ProvidedFileSystemsWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kFunctionUpdateFileSystems,
      base::Bind(&ProvidedFileSystemsWebUIHandler::UpdateFileSystems,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kFunctionSelectFileSystem,
      base::Bind(&ProvidedFileSystemsWebUIHandler::SelectFileSystem,
                 weak_ptr_factory_.GetWeakPtr()));
}

file_system_provider::Service* ProvidedFileSystemsWebUIHandler::GetService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* const profile = Profile::FromWebUI(web_ui());
  return file_system_provider::ServiceFactory::FindExisting(profile);
}

void ProvidedFileSystemsWebUIHandler::UpdateFileSystems(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_system_provider::Service* const service = GetService();
  if (!service)
    return;

  base::ListValue items;

  const std::vector<file_system_provider::ProvidedFileSystemInfo>
      file_system_info_list = service->GetProvidedFileSystemInfoList();

  for (size_t i = 0; i < file_system_info_list.size(); ++i) {
    const file_system_provider::ProvidedFileSystemInfo file_system_info =
        file_system_info_list[i];

    file_system_provider::ProvidedFileSystemInterface* const file_system =
        service->GetProvidedFileSystem(file_system_info.extension_id(),
                                       file_system_info.file_system_id());
    DCHECK(file_system);

    file_system_provider::RequestManager* const request_manager =
        file_system->GetRequestManager();
    DCHECK(request_manager);

    base::DictionaryValue* item = new base::DictionaryValue();
    item->SetString(kKeyId, file_system_info.file_system_id());
    item->SetString(kKeyName, file_system_info.display_name());
    item->SetString(kKeyExtensionId, file_system_info.extension_id());
    item->SetString(kKeyMountPath,
                    file_system_info.mount_path().AsUTF8Unsafe());
    item->SetInteger(kKeyActiveRequests,
                     request_manager->GetActiveRequestsForLogging());

    items.Append(item);
  }

  web_ui()->CallJavascriptFunction(kFunctionUpdateFileSystems, items);
}

void ProvidedFileSystemsWebUIHandler::SelectFileSystem(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  file_system_provider::Service* const service = GetService();
  if (!service)
    return;

  std::string extension_id;
  if (!args->GetString(0, &extension_id))
    return;

  std::string file_system_id;
  if (!args->GetString(1, &file_system_id))
    return;

  // Stop observing the previously selected request manager.
  {
    file_system_provider::ProvidedFileSystemInterface* const file_system =
        service->GetProvidedFileSystem(selected_extension_id,
                                       selected_file_system_id);
    if (file_system) {
      file_system_provider::RequestManager* const request_manager =
          file_system->GetRequestManager();
      DCHECK(request_manager);
      request_manager->RemoveObserver(this);
    }
  }

  // Observe the selected file system.
  file_system_provider::ProvidedFileSystemInterface* const file_system =
      service->GetProvidedFileSystem(extension_id, file_system_id);
  if (!file_system)
    return;

  file_system_provider::RequestManager* const request_manager =
      file_system->GetRequestManager();
  DCHECK(request_manager);

  request_manager->AddObserver(this);
  selected_extension_id = extension_id;
  selected_file_system_id = file_system_id;
}

}  // namespace

ProvidedFileSystemsUI::ProvidedFileSystemsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new ProvidedFileSystemsWebUIHandler());

  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIProvidedFileSystemsHost);
  source->AddResourcePath("provided_file_systems.css",
                          IDR_PROVIDED_FILE_SYSTEMS_CSS);
  source->AddResourcePath("provided_file_systems.js",
                          IDR_PROVIDED_FILE_SYSTEMS_JS);
  source->SetDefaultResource(IDR_PROVIDED_FILE_SYSTEMS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source);
}

}  // namespace chromeos
