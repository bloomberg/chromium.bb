// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_operation_runner.h"
#include "chrome/browser/chromeos/gdata/gdata_operations.h"
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

}  // namespace

DocumentsService::DocumentsService()
    : profile_(NULL),
      runner_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DocumentsService::~DocumentsService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataAuthService* DocumentsService::auth_service_for_testing() {
  return runner_->auth_service();
}

void DocumentsService::Initialize(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = profile;
  runner_.reset(new GDataOperationRunner(profile));
  runner_->Initialize();
}

GDataOperationRegistry* DocumentsService::operation_registry() const {
  return runner_->operation_registry();
}

void DocumentsService::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  runner_->CancelAll();
}

void DocumentsService::Authenticate(const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  runner_->Authenticate(callback);
}

void DocumentsService::GetDocuments(const GURL& url,
                                    int start_changestamp,
                                    const std::string& search_query,
                                    const std::string& directory_resource_id,
                                    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetDocumentsOperation* operation =
      new GetDocumentsOperation(operation_registry(),
                                profile_,
                                start_changestamp,
                                search_query,
                                directory_resource_id,
                                callback);
  if (!url.is_empty())
    operation->SetUrl(url);
  runner_->StartOperationWithRetry(operation);
}

void DocumentsService::GetDocumentEntry(const std::string& resource_id,
                                        const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetDocumentEntryOperation* operation =
      new GetDocumentEntryOperation(operation_registry(),
                                    profile_,
                                    resource_id,
                                    callback);
  runner_->StartOperationWithRetry(operation);
}

void DocumentsService::GetAccountMetadata(const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetAccountMetadataOperation* operation =
      new GetAccountMetadataOperation(operation_registry(),
                                      profile_,
                                      callback);
  runner_->StartOperationWithRetry(operation);
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
      chrome_common_net::AppendQueryParameter(document_url,
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

  runner_->StartOperationWithRetry(
      new DownloadFileOperation(operation_registry(), profile_,
                                download_action_callback,
                                get_download_data_callback, document_url,
                                virtual_path, local_cache_path));
}

void DocumentsService::DeleteDocument(const GURL& document_url,
                                      const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new DeleteDocumentOperation(operation_registry(), profile_, callback,
                                  document_url));
}

void DocumentsService::CreateDirectory(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new CreateDirectoryOperation(operation_registry(), profile_, callback,
                                   parent_content_url, directory_name));
}

void DocumentsService::CopyDocument(const std::string& resource_id,
                                    const FilePath::StringType& new_name,
                                    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new CopyDocumentOperation(operation_registry(), profile_, callback,
                                resource_id, new_name));
}

void DocumentsService::RenameResource(const GURL& resource_url,
                                      const FilePath::StringType& new_name,
                                      const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new RenameResourceOperation(operation_registry(), profile_, callback,
                                  resource_url, new_name));
}

void DocumentsService::AddResourceToDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new AddResourceToDirectoryOperation(operation_registry(),
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

  runner_->StartOperationWithRetry(
      new RemoveResourceFromDirectoryOperation(operation_registry(),
                                               profile_,
                                               callback,
                                               parent_content_url,
                                               resource_url,
                                               resource_id));
}

void DocumentsService::InitiateUpload(const InitiateUploadParams& params,
                                      const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (params.upload_location.is_empty()) {
    if (!callback.is_null())
      callback.Run(HTTP_BAD_REQUEST, GURL());
    return;
  }

  runner_->StartOperationWithRetry(
      new InitiateUploadOperation(operation_registry(), profile_, callback,
                                  params));
}

void DocumentsService::ResumeUpload(const ResumeUploadParams& params,
                                    const ResumeUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new ResumeUploadOperation(operation_registry(), profile_, callback,
                                params));
}


void DocumentsService::AuthorizeApp(const GURL& resource_url,
                                    const std::string& app_ids,
                                    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  runner_->StartOperationWithRetry(
      new AuthorizeAppsOperation(operation_registry(), profile_, callback,
                                 resource_url, app_ids));
}

}  // namespace gdata
