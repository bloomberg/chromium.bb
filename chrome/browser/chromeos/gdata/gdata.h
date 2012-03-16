// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
#pragma once

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher.h"

class Profile;

namespace net {
class FileStream;
};

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

// This class provides authentication for GData based services.
// It integrates specific service integration with OAuth2 stack
// (TokenService) and provides OAuth2 token refresh infrastructure.
// All public functions must be called on UI thread.
class GDataAuthService : public content::NotificationObserver {
 public:
  class Observer {
   public:
    // Triggered when a new OAuth2 refresh token is received from TokenService.
    virtual void OnOAuth2RefreshTokenChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  GDataAuthService();
  virtual ~GDataAuthService();

  // Adds and removes the observer. AddObserver() should be called before
  // Initialize() as it can change the refresh token.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Initializes the auth service. Starts TokenService to retrieve the
  // refresh token.
  void Initialize(Profile* profile);

  // Starts fetching OAuth2 auth token from the refresh token.
  void StartAuthentication(GDataOperationRegistry* registry,
                           const AuthStatusCallback& callback);

  // True if OAuth2 auth token is retrieved and believed to be fresh.
  bool IsFullyAuthenticated() const { return !auth_token_.empty(); }

  // True if OAuth2 refresh token is present. It's absence means that user
  // is not properly authenticated.
  bool IsPartiallyAuthenticated() const { return !refresh_token_.empty(); }

  // Gets OAuth2 auth token.
  const std::string& oauth2_auth_token() const { return auth_token_; }

  // Clears OAuth2 token.
  void ClearOAuth2Token() { auth_token_.clear(); }

  // Gets OAuth2 refresh token.
  const std::string& GetOAuth2RefreshToken() { return refresh_token_; }

  // Callback for AuthOperation (InternalAuthStatusCallback).
  void OnAuthCompleted(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                       const AuthStatusCallback& callback,
                       GDataErrorCode error,
                       const std::string& auth_token);

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Sets the auth_token as specified.  This should be used only for testing.
  void set_oauth2_auth_token_for_testing(const std::string& token) {
    auth_token_ = token;
  }

 private:
  // Helper function for StartAuthentication() call.
  void StartAuthenticationOnUIThread(
      GDataOperationRegistry* registry,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      const AuthStatusCallback& callback);


  Profile* profile_;
  std::string refresh_token_;
  std::string auth_token_;
  ObserverList<Observer> observers_;

  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<GDataAuthService> weak_ptr_factory_;
  base::WeakPtr<GDataAuthService> weak_ptr_bound_to_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(GDataAuthService);
};

// This defines an interface for sharing by DocumentService and
// MockDocumentService so that we can do testing of clients of DocumentService.
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
  //
  // Can be called on any thread.
  virtual void Authenticate(const AuthStatusCallback& callback) = 0;

  // Gets the document feed from |feed_url|. If this URL is empty, the call
  // will fetch the default ('root') document feed. Upon completion,
  // invokes |callback| with results on the calling thread.
  //
  // Can be called on any thread.
  virtual void GetDocuments(const GURL& feed_url,
                            const GetDataCallback& callback) = 0;

  // Gets the account metadata from the server using the default account
  // metadata URL. Upon completion, invokes |callback| with results on the
  // calling thread.
  //
  // Can be called on any thread.
  virtual void GetAccountMetadata(const GetDataCallback& callback) = 0;

  // Deletes a document identified by its 'self' |url| and |etag|.
  // Upon completion, invokes |callback| with results on the calling thread.
  //
  // Can be called on any thread.
  virtual void DeleteDocument(const GURL& document_url,
                              const EntryActionCallback& callback) = 0;

  // Downloads a document identified by its |content_url| in a given |format|.
  // Upon completion, invokes |callback| with results on the calling thread.
  //
  // Can be called on any thread.
  virtual void DownloadDocument(const FilePath& virtual_path,
                                const GURL& content_url,
                                DocumentExportFormat format,
                                const DownloadActionCallback& callback) = 0;

  // Makes a copy of a document identified by its 'self' link |document_url|.
  // The copy is named as the UTF-8 encoded |new_name| and is not added to any
  // collection. Use AddResourceToDirectory() to add the copy to a collection
  // when needed. Upon completion, invokes |callback| with results on the
  // calling thread.
  //
  // Can be called on any thread.
  virtual void CopyDocument(const GURL& document_url,
                            const FilePath::StringType& new_name,
                            const GetDataCallback& callback) = 0;

  // Renames a document or collection identified by its 'self' link
  // |document_url| to the UTF-8 encoded |new_name|. Upon completion,
  // invokes |callback| with results on the calling thread.
  //
  // Can be called on any thread.
  virtual void RenameResource(const GURL& resource_url,
                              const FilePath::StringType& new_name,
                              const EntryActionCallback& callback) = 0;

  // Adds a resource (document, file, or collection) identified by its
  // 'self' link |resource_url| to a collection with a content link
  // |parent_content_url|. Upon completion, invokes |callback| with
  // results on the calling thread.
  //
  // Can be called on any thread.
  virtual void AddResourceToDirectory(const GURL& parent_content_url,
                                      const GURL& resource_url,
                                      const EntryActionCallback& callback) = 0;

  // Removes a resource (document, file, collection) identified by its
  // 'self' link |resource_url| from a collection with a content link
  // |parent_content_url|. Upon completion, invokes |callback| with
  // results on the calling thread.
  //
  // Can be called on any thread.
  virtual void RemoveResourceFromDirectory(
      const GURL& parent_content_url,
      const GURL& resource_url,
      const std::string& resource_id,
      const EntryActionCallback& callback) = 0;

  // Creates new collection with |directory_name| under parent directory
  // identified with |parent_content_url|. If |parent_content_url| is empty,
  // the new collection will be created in the root. Upon completion,
  // invokes |callback| and passes newly created entry on the calling thread.
  //
  // Can be called on any thread.
  virtual void CreateDirectory(const GURL& parent_content_url,
                               const FilePath::StringType& directory_name,
                               const GetDataCallback& callback) = 0;

  // Downloads a file identified by its |content_url|. Upon completion, invokes
  // |callback| with results on the calling thread.
  //
  // Can be called on any thread.
  virtual void DownloadFile(const FilePath& virtual_path,
                            const GURL& content_url,
                            const DownloadActionCallback& callback) = 0;

  // Initiates uploading of a document/file.
  //
  // Can be called on any thread.
  virtual void InitiateUpload(const InitiateUploadParams& params,
                              const InitiateUploadCallback& callback) = 0;

  // Resumes uploading of a document/file on the calling thread.
  //
  // Can be called on any thread.
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
                            const GetDataCallback& callback) OVERRIDE;
  virtual void GetAccountMetadata(const GetDataCallback& callback) OVERRIDE;
  virtual void DeleteDocument(const GURL& document_url,
                              const EntryActionCallback& callback) OVERRIDE;
  virtual void DownloadDocument(
      const FilePath& virtual_path,
      const GURL& content_url,
      DocumentExportFormat format,
      const DownloadActionCallback& callback) OVERRIDE;
  virtual void CopyDocument(const GURL& document_url,
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
  virtual void DownloadFile(const FilePath& virtual_path,
                            const GURL& content_url,
                            const DownloadActionCallback& callback) OVERRIDE;
  virtual void InitiateUpload(const InitiateUploadParams& params,
                              const InitiateUploadCallback& callback) OVERRIDE;
  virtual void ResumeUpload(const ResumeUploadParams& params,
                            const ResumeUploadCallback& callback) OVERRIDE;

  GDataAuthService* gdata_auth_service() { return gdata_auth_service_.get(); }

 private:
  // GDataAuthService::Observer override.
  virtual void OnOAuth2RefreshTokenChanged() OVERRIDE;

  // Submits an operation implementing the GDataOperationInterface interface
  // to run on the UI thread, and makes the operation retry upon authentication
  // failures by calling back to DocumentsService::RetryOperation.
  //
  // Called on the same thread that creates |operation|.
  void StartOperationOnUIThread(GDataOperationInterface* operation);

  // Starts an operation implementing the GDataOperationInterface interface.
  //
  // Must be called on UI thread.
  void StartOperation(GDataOperationInterface* operation);

  // Called when the authentication token is refreshed.
  //
  // Must be called on UI thread.
  void OnOperationAuthRefresh(GDataOperationInterface* operation,
                              GDataErrorCode error,
                              const std::string& auth_token);

  // Clears any authentication token and retries the operation, which
  // forces an authentication token refresh.
  //
  // Must be called on UI thread.
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

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
