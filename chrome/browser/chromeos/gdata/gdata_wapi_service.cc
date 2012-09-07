// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_wapi_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/chromeos/gdata/drive_api_operations.h"
#include "chrome/browser/chromeos/gdata/gdata_operations.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/operation_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/url_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

namespace {

const char* GetExportFormatParam(DocumentExportFormat format) {
  switch (format) {
    case PNG:
      return "png";
    case HTML:
      return "html";
    case TXT:
      return "txt";
    case DOC:
      return "doc";
    case ODT:
      return "odt";
    case RTF:
      return "rtf";
    case ZIP:
      return "zip";
    case JPEG:
      return "jpeg";
    case SVG:
      return "svg";
    case PPT:
      return "ppt";
    case XLS:
      return "xls";
    case CSV:
      return "csv";
    case ODS:
      return "ods";
    case TSV:
      return "tsv";
    default:
      return "pdf";
  }
}

// OAuth2 scopes for the documents API.
const char kDocsListScope[] = "https://docs.google.com/feeds/";
const char kSpreadsheetsScope[] = "https://spreadsheets.google.com/feeds/";
const char kUserContentScope[] = "https://docs.googleusercontent.com/";
const char kDriveAppsScope[] = "https://www.googleapis.com/auth/drive.apps";

}  // namespace

GDataWapiService::GDataWapiService()
    : profile_(NULL),
      runner_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataWapiService::~GDataWapiService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (runner_.get())
    runner_->auth_service()->RemoveObserver(this);
}

AuthService* GDataWapiService::auth_service_for_testing() {
  return runner_->auth_service();
}

void GDataWapiService::Initialize(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = profile;

  std::vector<std::string> scopes;
  scopes.push_back(kDocsListScope);
  scopes.push_back(kSpreadsheetsScope);
  scopes.push_back(kUserContentScope);
  // Drive App scope is required for even WAPI v3 apps access.
  scopes.push_back(kDriveAppsScope);
  runner_.reset(new OperationRunner(profile, scopes));
  runner_->Initialize();

  runner_->auth_service()->AddObserver(this);
}

void GDataWapiService::AddObserver(DriveServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void GDataWapiService::RemoveObserver(DriveServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

OperationRegistry* GDataWapiService::operation_registry() const {
  return runner_->operation_registry();
}

bool GDataWapiService::CanStartOperation() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return HasRefreshToken();
}

void GDataWapiService::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  runner_->CancelAll();
}

void GDataWapiService::Authenticate(const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  runner_->Authenticate(callback);
}

void GDataWapiService::GetDocuments(const GURL& url,
                                    int64 start_changestamp,
                                    const std::string& search_query,
                                    const std::string& directory_resource_id,
                                    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Drive V2 API defines changestamp in int64, while DocumentsList API uses
  // int32. This narrowing should not cause any trouble.
  GetDocumentsOperation* operation =
      new GetDocumentsOperation(operation_registry(),
                                url,
                                static_cast<int>(start_changestamp),
                                search_query,
                                directory_resource_id,
                                callback);
  runner_->StartOperationWithRetry(operation);
}

void GDataWapiService::GetDocumentEntry(const std::string& resource_id,
                                        const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetDocumentEntryOperation* operation =
      new GetDocumentEntryOperation(operation_registry(),
                                    resource_id,
                                    callback);
  runner_->StartOperationWithRetry(operation);
}

void GDataWapiService::GetAccountMetadata(const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetAccountMetadataOperation* operation =
      new GetAccountMetadataOperation(operation_registry(), callback);
  runner_->StartOperationWithRetry(operation);
}

void GDataWapiService::GetApplicationInfo(const GetDataCallback& callback) {
  // For WAPI, AccountMetadata includes Drive application information.
  GetAccountMetadata(callback);
}

void GDataWapiService::DownloadDocument(
    const FilePath& virtual_path,
    const FilePath& local_cache_path,
    const GURL& document_url,
    DocumentExportFormat format,
    const DownloadActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadFile(
      virtual_path,
      local_cache_path,
      chrome_common_net::AppendQueryParameter(document_url,
                                               "exportFormat",
                                               GetExportFormatParam(format)),
      callback,
      GetContentCallback());
}

void GDataWapiService::DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& document_url,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new DownloadFileOperation(operation_registry(),
                                download_action_callback,
                                get_content_callback, document_url,
                                virtual_path, local_cache_path));
}

void GDataWapiService::DeleteDocument(const GURL& document_url,
                                      const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new DeleteDocumentOperation(operation_registry(), callback,
                                  document_url));
}

void GDataWapiService::CreateDirectory(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new CreateDirectoryOperation(operation_registry(), callback,
                                   parent_content_url, directory_name));
}

void GDataWapiService::CopyDocument(const std::string& resource_id,
                                    const FilePath::StringType& new_name,
                                    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new CopyDocumentOperation(operation_registry(), callback,
                                resource_id, new_name));
}

void GDataWapiService::RenameResource(const GURL& resource_url,
                                      const FilePath::StringType& new_name,
                                      const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new RenameResourceOperation(operation_registry(), callback,
                                  resource_url, new_name));
}

void GDataWapiService::AddResourceToDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new AddResourceToDirectoryOperation(operation_registry(),
                                          callback,
                                          parent_content_url,
                                          resource_url));
}

void GDataWapiService::RemoveResourceFromDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new RemoveResourceFromDirectoryOperation(operation_registry(),
                                               callback,
                                               parent_content_url,
                                               resource_url,
                                               resource_id));
}

void GDataWapiService::InitiateUpload(const InitiateUploadParams& params,
                                      const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (params.upload_location.is_empty()) {
    if (!callback.is_null())
      callback.Run(HTTP_BAD_REQUEST, GURL());
    return;
  }

  runner_->StartOperationWithRetry(
      new InitiateUploadOperation(operation_registry(), callback, params));
}

void GDataWapiService::ResumeUpload(const ResumeUploadParams& params,
                                    const ResumeUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new ResumeUploadOperation(operation_registry(), callback, params));
}

void GDataWapiService::AuthorizeApp(const GURL& resource_url,
                                    const std::string& app_ids,
                                    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new AuthorizeAppsOperation(operation_registry(), callback,
                                 resource_url, app_ids));
}

bool GDataWapiService::HasAccessToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return runner_->auth_service()->HasAccessToken();
}

bool GDataWapiService::HasRefreshToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return runner_->auth_service()->HasRefreshToken();
}

void GDataWapiService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (CanStartOperation()) {
    FOR_EACH_OBSERVER(
        DriveServiceObserver, observers_, OnReadyToPerformOperations());
  }
}

}  // namespace gdata
