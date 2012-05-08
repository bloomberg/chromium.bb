// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains mocks for classes in gdata_documents_service.h

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_DOCUMENTS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_DOCUMENTS_SERVICE_H_
#pragma once

#include <string>

#include "base/platform_file.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "testing/gmock/include/gmock/gmock.h"

class FilePath;

namespace gdata {

class MockDocumentsService : public DocumentsServiceInterface {
 public:
  // DocumentsService is usually owned and created by GDataFileSystem.
  MockDocumentsService();
  virtual ~MockDocumentsService();

  // DocumentServiceInterface overrides.
  MOCK_METHOD1(Initialize, void(Profile* profile));
  MOCK_CONST_METHOD0(operation_registry, GDataOperationRegistry*());
  MOCK_METHOD0(CancelAll, void(void));
  MOCK_METHOD1(Authenticate, void(const AuthStatusCallback& callback));
  MOCK_METHOD4(GetDocuments, void(const GURL& feed_url,
                                  int start_changestamp,
                                  const std::string& search_string,
                                  const GetDataCallback& callback));
  MOCK_METHOD1(GetAccountMetadata, void(const GetDataCallback& callback));
  MOCK_METHOD2(DeleteDocument, void(const GURL& document_url,
                                    const EntryActionCallback& callback));
  MOCK_METHOD5(DownloadDocument, void(const FilePath& virtual_path,
                                      const FilePath& local_cache_path,
                                      const GURL& content_url,
                                      DocumentExportFormat format,
                                      const DownloadActionCallback& callback));
  MOCK_METHOD3(CopyDocument, void(const std::string& resource_id,
                                  const FilePath::StringType& new_name,
                                  const GetDataCallback& callback));
  MOCK_METHOD3(RenameResource, void(const GURL& resource_url,
                                    const FilePath::StringType& new_name,
                                    const EntryActionCallback& callback));
  MOCK_METHOD3(AddResourceToDirectory,
               void(const GURL& parent_content_url,
                    const GURL& resource_url,
                    const EntryActionCallback& callback));
  MOCK_METHOD4(RemoveResourceFromDirectory,
               void(const GURL& parent_content_url,
                    const GURL& resource_url,
                    const std::string& resource_id,
                    const EntryActionCallback& callback));
  MOCK_METHOD3(CreateDirectory,
               void(const GURL& parent_content_url,
                    const FilePath::StringType& directory_name,
                    const GetDataCallback& callback));
  MOCK_METHOD5(DownloadFile,
               void(const FilePath& virtual_path,
                    const FilePath& local_cache_path,
                    const GURL& content_url,
                    const DownloadActionCallback& donwload_action_callback,
                    const GetDownloadDataCallback& get_download_data_callback));
  MOCK_METHOD2(InitiateUpload,
               void(const InitiateUploadParams& upload_file_info,
                    const InitiateUploadCallback& callback));
  MOCK_METHOD2(ResumeUpload, void(const ResumeUploadParams& upload_file_info,
                                  const ResumeUploadCallback& callback));

  // Helper stub methods for functions which take callbacks, so that
  // the callbacks get called with testable results.

  // Will call |callback| with HTTP_SUCCESS and the token "test_auth_token"
  // as the token.
  void AuthenticateStub(const AuthStatusCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and a StringValue with the current
  // value of |feed_data_|.
  void GetDocumentsStub(const GURL& feed_url,
                        int start_changestamp,
                        const std::string& search_string,
                        const GetDataCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and a StringValue with the current
  // value of |account_metadata_|.
  void GetAccountMetadataStub(const GetDataCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and the |document_url|.
  void DeleteDocumentStub(const GURL& document_url,
                          const EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS, the given URL, and the host+path
  // portion of the URL as the temporary file path.
  void DownloadDocumentStub(const FilePath& virtual_path,
                            const FilePath& local_tmp_path,
                            const GURL& content_url,
                            DocumentExportFormat format,
                            const DownloadActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and the current value of
  // |document_data_|.
  void CopyDocumentStub(const std::string& resource_id,
                        const FilePath::StringType& new_name,
                        const GetDataCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and the |document_url|.
  void RenameResourceStub(const GURL& document_url,
                          const FilePath::StringType& new_name,
                          const EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and the |resource_url|.
  void AddResourceToDirectoryStub(const GURL& parent_content_url,
                                  const GURL& resource_url,
                                  const EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and the |resource_url|.
  void RemoveResourceFromDirectoryStub(const GURL& parent_content_url,
                                       const GURL& resource_url,
                                       const std::string& resource_id,
                                       const EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and the current value of
  // |directory_data_|.
  void CreateDirectoryStub(const GURL& parent_content_url,
                           const FilePath::StringType& directory_name,
                           const GetDataCallback& callback);

  // Will call |callback| with HTTP_SUCCESS, the given URL, and the host+path
  // portion of the URL as the temporary file path. If |file_data_| is not null,
  // |file_data_| is written to the temporary file.
  void DownloadFileStub(
      const FilePath& virtual_path,
      const FilePath& local_tmp_path,
      const GURL& content_url,
      const DownloadActionCallback& download_action_callback,
      const GetDownloadDataCallback& get_download_data_callback);

  void set_account_metadata(base::Value* account_metadata) {
    feed_data_.reset(account_metadata);
  }

  void set_feed_data(base::Value* feed_data) {
    feed_data_.reset(feed_data);
  }

  void set_directory_data(base::Value* directory_data) {
    directory_data_.reset(directory_data);
  }

  void set_file_data(std::string* file_data) {
    file_data_.reset(file_data);
  }

 private:
  // Account meta data to be returned from GetAccountMetadata.
  scoped_ptr<base::Value> account_metadata_;

  // Feed data to be returned from GetDocuments.
  scoped_ptr<base::Value> feed_data_;

  // Feed data to be returned from CreateDirectory.
  scoped_ptr<base::Value> directory_data_;

  // Feed data to be returned from CopyDocument.
  scoped_ptr<base::Value> document_data_;

  // Feed data to be returned from GetDocuments if the search path is specified.
  // The feed contains subset of the root_feed.
  scoped_ptr<base::Value> search_result_;

  // File data to be written to the local temporary file when
  // DownloadDocumentStub is called.
  scoped_ptr<std::string> file_data_;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_DOCUMENTS_SERVICE_H_
