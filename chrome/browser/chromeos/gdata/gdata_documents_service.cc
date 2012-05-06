// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_operations.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/browser/profiles/profile.h"
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

}  // namespace

DocumentsService::DocumentsService()
    : profile_(NULL),
      gdata_auth_service_(new GDataAuthService()),
      operation_registry_(new GDataOperationRegistry),
      weak_ptr_factory_(this),
      // The weak pointers is used to post tasks to UI thread.
      weak_ptr_bound_to_ui_thread_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DocumentsService::~DocumentsService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gdata_auth_service_->RemoveObserver(this);
}

void DocumentsService::Initialize(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  profile_ = profile;
  // AddObserver() should be called before Initialize() as it could change
  // the refresh token.
  gdata_auth_service_->AddObserver(this);
  gdata_auth_service_->Initialize(profile);
}

GDataOperationRegistry* DocumentsService::operation_registry() const {
  return operation_registry_.get();
}

void DocumentsService::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  operation_registry_->CancelAll();
}

void DocumentsService::Authenticate(const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gdata_auth_service_->StartAuthentication(operation_registry_.get(),
                                           callback);
}

void DocumentsService::GetDocuments(const GURL& url,
                                    int start_changestamp,
                                    const std::string& search_query,
                                    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetDocumentsOperation* operation =
      new GetDocumentsOperation(operation_registry_.get(),
                                profile_,
                                start_changestamp,
                                search_query,
                                callback);
  if (!url.is_empty())
    operation->SetUrl(url);
  StartOperationWithRetry(operation);
}

void DocumentsService::GetAccountMetadata(const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetAccountMetadataOperation* operation =
      new GetAccountMetadataOperation(operation_registry_.get(),
                                      profile_,
                                      callback);
  StartOperationWithRetry(operation);
}

void DocumentsService::DownloadDocument(
    const FilePath& virtual_path,
    const FilePath& local_cache_path,
    const GURL& document_url,
    DocumentExportFormat format,
    const DownloadActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadFile(
      virtual_path,
      local_cache_path,
      chrome_browser_net::AppendQueryParameter(document_url,
                                               "exportFormat",
                                               GetExportFormatParam(format)),
      callback,
      GetDownloadDataCallback());
}

void DocumentsService::DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& document_url,
      const DownloadActionCallback& download_action_callback,
      const GetDownloadDataCallback& get_download_data_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartOperationWithRetry(
      new DownloadFileOperation(operation_registry_.get(), profile_,
                                download_action_callback,
                                get_download_data_callback, document_url,
                                virtual_path, local_cache_path));
}

void DocumentsService::DeleteDocument(const GURL& document_url,
                                      const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartOperationWithRetry(
      new DeleteDocumentOperation(operation_registry_.get(), profile_, callback,
                                  document_url));
}

void DocumentsService::CreateDirectory(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartOperationWithRetry(
      new CreateDirectoryOperation(operation_registry_.get(), profile_,
                                   callback, parent_content_url,
                                   directory_name));
}

void DocumentsService::CopyDocument(const std::string& resource_id,
                                    const FilePath::StringType& new_name,
                                    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartOperationWithRetry(
      new CopyDocumentOperation(operation_registry_.get(), profile_, callback,
                                resource_id, new_name));
}

void DocumentsService::RenameResource(const GURL& resource_url,
                                      const FilePath::StringType& new_name,
                                      const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartOperationWithRetry(
      new RenameResourceOperation(operation_registry_.get(), profile_, callback,
                                  resource_url, new_name));
}

void DocumentsService::AddResourceToDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartOperationWithRetry(
      new AddResourceToDirectoryOperation(operation_registry_.get(),
                                          profile_,
                                          callback,
                                          parent_content_url,
                                          resource_url));
}

void DocumentsService::RemoveResourceFromDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartOperationWithRetry(
      new RemoveResourceFromDirectoryOperation(operation_registry_.get(),
                                               profile_,
                                               callback,
                                               parent_content_url,
                                               resource_url,
                                               resource_id));
}

void DocumentsService::InitiateUpload(const InitiateUploadParams& params,
                                      const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (params.resumable_create_media_link.is_empty()) {
    if (!callback.is_null()) {
      callback.Run(HTTP_BAD_REQUEST, GURL());
    }
    return;
  }

  StartOperationWithRetry(
      new InitiateUploadOperation(operation_registry_.get(), profile_, callback,
                                  params));
}

void DocumentsService::ResumeUpload(const ResumeUploadParams& params,
                                    const ResumeUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartOperationWithRetry(
      new ResumeUploadOperation(operation_registry_.get(), profile_, callback,
                                params));
}

void DocumentsService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DocumentsService::StartOperationWithRetry(
    GDataOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The re-authenticatation callback will run on UI thread.
  operation->SetReAuthenticateCallback(
      base::Bind(&DocumentsService::RetryOperation,
                 weak_ptr_bound_to_ui_thread_));
  StartOperation(operation);
}

void DocumentsService::StartOperation(GDataOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!gdata_auth_service_->IsFullyAuthenticated()) {
    // Fetch OAuth2 authentication token from the refresh token first.
    gdata_auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&DocumentsService::OnOperationAuthRefresh,
                   weak_ptr_bound_to_ui_thread_,
                   operation));
    return;
  }

  operation->Start(gdata_auth_service_->oauth2_auth_token());
}

void DocumentsService::OnOperationAuthRefresh(
    GDataOperationInterface* operation,
    GDataErrorCode code,
    const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (code == HTTP_SUCCESS) {
    DCHECK(gdata_auth_service_->IsPartiallyAuthenticated());
    StartOperation(operation);
  } else {
    operation->OnAuthFailed(code);
  }
}

void DocumentsService::RetryOperation(GDataOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gdata_auth_service_->ClearOAuth2Token();
  // User authentication might have expired - rerun the request to force
  // auth token refresh.
  StartOperation(operation);
}

}  // namespace gdata
