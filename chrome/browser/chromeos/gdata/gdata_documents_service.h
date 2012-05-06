// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DOCUMENTS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DOCUMENTS_SERVICE_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/gdata/gdata_auth_service.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"

class FilePath;
class GURL;
class Profile;

namespace gdata {

class GDataOperationInterface;
class GDataOperationRegistry;

// Document export format.
enum DocumentExportFormat {
  PDF,     // Portable Document Format. (all documents)
  PNG,     // Portable Networks Graphic Image Format (all documents)
  HTML,    // HTML Format (text documents and spreadsheets).
  TXT,     // Text file (text documents and presentations).
  DOC,     // Word (text documents only).
  ODT,     // Open Document Format (text documents only).
  RTF,     // Rich Text Format (text documents only).
  ZIP,     // ZIP archive (text documents only). Contains the images (if any)
           // used in the document as well as a .html file containing the
           // document's text.
  JPEG,    // JPEG (drawings only).
  SVG,     // Scalable Vector Graphics Image Format (drawings only).
  PPT,     // Powerpoint (presentations only).
  XLS,     // Excel (spreadsheets only).
  CSV,     // Excel (spreadsheets only).
  ODS,     // Open Document Spreadsheet (spreadsheets only).
  TSV,     // Tab Separated Value (spreadsheets only). Only the first worksheet
           // is returned in TSV by default.
};

// This defines an interface for sharing by DocumentService and
// MockDocumentService so that we can do testing of clients of DocumentService.
//
// All functions must be called on UI thread. DocumentService is built on top
// of URLFetcher that runs on UI thread.
//
// TODO(zel,benchan): Make the terminology/naming convention (e.g. file vs
// document vs resource, directory vs collection) more consistent and precise.
class DocumentsServiceInterface {
 public:
  virtual ~DocumentsServiceInterface() {}

  // Initializes the documents service tied with |profile|.
  virtual void Initialize(Profile* profile) = 0;

  // Retrieves the operation registry.
  virtual GDataOperationRegistry* operation_registry() const = 0;

  // Cancels all in-flight operations.
  virtual void CancelAll() = 0;

  // Authenticates the user by fetching the auth token as
  // needed. |callback| will be run with the error code and the auth
  // token, on the thread this function is run.
  virtual void Authenticate(const AuthStatusCallback& callback) = 0;

  // Fetches the document feed from |feed_url| with |start_changestamp|. If this
  // URL is empty, the call will fetch the default root or change document feed.
  // |start_changestamp| specifies the starting point from change feeds only.
  // Value different than 0, it would trigger delta feed fetching.
  // |search_query| specifies search query to be sent to the server. It will be
  // used only if |start_changestamp| is 0. If empty string is passed,
  // |search_query| is ignored.
  // Upon completion, invokes |callback| with results on the calling thread.
  virtual void GetDocuments(const GURL& feed_url,
                            int start_changestamp,
                            const std::string& search_query,
                            const GetDataCallback& callback) = 0;

  // Gets the account metadata from the server using the default account
  // metadata URL. Upon completion, invokes |callback| with results on the
  // calling thread.
  virtual void GetAccountMetadata(const GetDataCallback& callback) = 0;

  // Deletes a document identified by its 'self' |url| and |etag|.
  // Upon completion, invokes |callback| with results on the calling thread.
  virtual void DeleteDocument(const GURL& document_url,
                              const EntryActionCallback& callback) = 0;

  // Downloads a document identified by its |content_url| in a given |format|.
  // Upon completion, invokes |callback| with results on the calling thread.
  virtual void DownloadDocument(const FilePath& virtual_path,
                                const FilePath& local_cache_path,
                                const GURL& content_url,
                                DocumentExportFormat format,
                                const DownloadActionCallback& callback) = 0;

  // Makes a copy of a document identified by its |resource_id|.
  // The copy is named as the UTF-8 encoded |new_name| and is not added to any
  // collection. Use AddResourceToDirectory() to add the copy to a collection
  // when needed. Upon completion, invokes |callback| with results on the
  // calling thread.
  virtual void CopyDocument(const std::string& resource_id,
                            const FilePath::StringType& new_name,
                            const GetDataCallback& callback) = 0;

  // Renames a document or collection identified by its 'self' link
  // |document_url| to the UTF-8 encoded |new_name|. Upon completion,
  // invokes |callback| with results on the calling thread.
  virtual void RenameResource(const GURL& resource_url,
                              const FilePath::StringType& new_name,
                              const EntryActionCallback& callback) = 0;

  // Adds a resource (document, file, or collection) identified by its
  // 'self' link |resource_url| to a collection with a content link
  // |parent_content_url|. Upon completion, invokes |callback| with
  // results on the calling thread.
  virtual void AddResourceToDirectory(const GURL& parent_content_url,
                                      const GURL& resource_url,
                                      const EntryActionCallback& callback) = 0;

  // Removes a resource (document, file, collection) identified by its
  // 'self' link |resource_url| from a collection with a content link
  // |parent_content_url|. Upon completion, invokes |callback| with
  // results on the calling thread.
  virtual void RemoveResourceFromDirectory(
      const GURL& parent_content_url,
      const GURL& resource_url,
      const std::string& resource_id,
      const EntryActionCallback& callback) = 0;

  // Creates new collection with |directory_name| under parent directory
  // identified with |parent_content_url|. If |parent_content_url| is empty,
  // the new collection will be created in the root. Upon completion,
  // invokes |callback| and passes newly created entry on the calling thread.
  virtual void CreateDirectory(const GURL& parent_content_url,
                               const FilePath::StringType& directory_name,
                               const GetDataCallback& callback) = 0;

  // Downloads a file identified by its |content_url|. The downloaded file will
  // be stored at |local_cache_path| location. Upon completion, invokes
  // |download_action_callback| with results on the calling thread.
  // If |get_download_data_callback| is not empty,
  // URLFetcherDelegate::OnURLFetchDownloadData will be called, which will in
  // turn invoke |get_download_data_callback| on the calling thread.
  virtual void DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const DownloadActionCallback& download_action_callback,
      const GetDownloadDataCallback& get_download_data_callback) = 0;

  // Initiates uploading of a document/file.
  virtual void InitiateUpload(const InitiateUploadParams& params,
                              const InitiateUploadCallback& callback) = 0;

  // Resumes uploading of a document/file on the calling thread.
  virtual void ResumeUpload(const ResumeUploadParams& params,
                            const ResumeUploadCallback& callback) = 0;
};

// This class provides documents feed service calls.
class DocumentsService
    : public DocumentsServiceInterface,
      public GDataAuthService::Observer {
 public:
  // DocumentsService is usually owned and created by GDataFileSystem.
  DocumentsService();
  virtual ~DocumentsService();

  // DocumentsServiceInterface Overrides
  virtual void Initialize(Profile* profile) OVERRIDE;
  virtual GDataOperationRegistry* operation_registry() const OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual void Authenticate(const AuthStatusCallback& callback) OVERRIDE;
  virtual void GetDocuments(const GURL& feed_url,
                            int start_changestamp,
                            const std::string& search_query,
                            const GetDataCallback& callback) OVERRIDE;
  virtual void GetAccountMetadata(const GetDataCallback& callback) OVERRIDE;
  virtual void DeleteDocument(const GURL& document_url,
                              const EntryActionCallback& callback) OVERRIDE;
  virtual void DownloadDocument(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      DocumentExportFormat format,
      const DownloadActionCallback& callback) OVERRIDE;
  virtual void DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const DownloadActionCallback& download_action_callback,
      const GetDownloadDataCallback& get_download_data_callback) OVERRIDE;
  virtual void CopyDocument(const std::string& resource_id,
                            const FilePath::StringType& new_name,
                            const GetDataCallback& callback) OVERRIDE;
  virtual void RenameResource(const GURL& document_url,
                              const FilePath::StringType& new_name,
                              const EntryActionCallback& callback) OVERRIDE;
  virtual void AddResourceToDirectory(
      const GURL& parent_content_url,
      const GURL& resource_url,
      const EntryActionCallback& callback) OVERRIDE;
  virtual void RemoveResourceFromDirectory(
      const GURL& parent_content_url,
      const GURL& resource_url,
      const std::string& resource_id,
      const EntryActionCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const GURL& parent_content_url,
                               const FilePath::StringType& directory_name,
                               const GetDataCallback& callback) OVERRIDE;
  virtual void InitiateUpload(const InitiateUploadParams& params,
                              const InitiateUploadCallback& callback) OVERRIDE;
  virtual void ResumeUpload(const ResumeUploadParams& params,
                            const ResumeUploadCallback& callback) OVERRIDE;

  GDataAuthService* gdata_auth_service() { return gdata_auth_service_.get(); }

 private:
  // GDataAuthService::Observer override.
  virtual void OnOAuth2RefreshTokenChanged() OVERRIDE;

  // Starts an operation implementing the GDataOperationInterface interface,
  // and makes the operation retry upon authentication failures by calling
  // back to DocumentsService::RetryOperation.
  void StartOperationWithRetry(GDataOperationInterface* operation);

  // Starts an operation implementing the GDataOperationInterface interface.
  void StartOperation(GDataOperationInterface* operation);

  // Called when the authentication token is refreshed.
  void OnOperationAuthRefresh(GDataOperationInterface* operation,
                              GDataErrorCode error,
                              const std::string& auth_token);

  // Clears any authentication token and retries the operation, which
  // forces an authentication token refresh.
  void RetryOperation(GDataOperationInterface* operation);

  // Data members.
  Profile* profile_;

  scoped_ptr<GDataAuthService> gdata_auth_service_;
  scoped_ptr<GDataOperationRegistry> operation_registry_;
  base::WeakPtrFactory<DocumentsService> weak_ptr_factory_;
  base::WeakPtr<DocumentsService> weak_ptr_bound_to_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(DocumentsService);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DOCUMENTS_SERVICE_H_
