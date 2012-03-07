// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"

class Profile;

namespace net {
class FileStream;
};

namespace gdata {

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
  TSV,     // Tab Seperated Value (spreadsheets only). Only the first worksheet
           // is returned in TSV by default.
};

// Different callback types for various functionalities in DocumentsService.
typedef base::Callback<void(GDataErrorCode error,
                            const std::string& token)> AuthStatusCallback;
typedef base::Callback<void(GDataErrorCode error,
                            base::Value* feed_data)> GetDataCallback;
// Callback for directory creation calls. If successful, |entry| will be a
// dictionary containing gdata entry represeting newly created directory.
typedef base::Callback<void(GDataErrorCode error,
                            base::Value* entry)> CreateEntryCallback;
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& document_url)> EntryActionCallback;
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& content_url,
                            const FilePath& temp_file)> DownloadActionCallback;
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& upload_location)>
    InitiateUploadCallback;
typedef base::Callback<void(GDataErrorCode error,
                            int64 start_range_received,
                            int64 end_range_received) >
    ResumeUploadCallback;


// Struct for passing params needed for DocumentsService::ResumeUpload() calls.
struct ResumeUploadParams {
  ResumeUploadParams(const std::string& title,
                     int64 start_range,
                     int64 end_range,
                     int64 content_length,
                     const std::string& content_type,
                     scoped_refptr<net::IOBuffer> buf,
                     const GURL& upload_location);
  ~ResumeUploadParams();

  std::string title;  // Title to be used for file to be uploaded.
  int64 start_range;  // Start of range of contents currently stored in |buf|.
  int64 end_range;  // End of range of contents currently stored in |buf|.
  int64 content_length;  // File content-Length.
  std::string content_type;   // Content-Type of file.
  scoped_refptr<net::IOBuffer> buf;  // Holds current content to be uploaded.
  GURL upload_location;   // Url of where to upload the file to.
};


// Struct for passing params needed for DocumentsService::InitiateUpload()
// calls.
struct InitiateUploadParams {
  InitiateUploadParams(const std::string& title,
                       const std::string& content_type,
                       int64 content_length,
                       const GURL& resumable_create_media_link);
  ~InitiateUploadParams();

  std::string title;
  std::string content_type;
  int64 content_length;
  GURL resumable_create_media_link;
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
                           AuthStatusCallback callback);

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
  void OnAuthCompleted(AuthStatusCallback callback,
                       GDataErrorCode error,
                       const std::string& auth_token);

  // Overriden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  Profile* profile_;
  std::string refresh_token_;
  std::string auth_token_;
  ObserverList<Observer> observers_;

  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<GDataAuthService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GDataAuthService);
};

// This calls provides documents feed service calls.
class DocumentsService : public GDataAuthService::Observer {
 public:
  // DocumentsService is usually owned and created by GDataFileSystem.
  DocumentsService();
  virtual ~DocumentsService();

  // Initializes the documents service tied with |profile|.
  void Initialize(Profile* profile);

  // Cancels all in-flight operations.
  void CancelAll();

  // needed. |callback| will be run with the error code and the auth
  // token, on the thread this function is run.
  //
  // Must be called on UI thread.
  void Authenticate(const AuthStatusCallback& callback);

  // Gets the document feed from |feed_url|. If this URL is empty, the call
  // will fetch the default ('root') document feed. Upon completion,
  // invokes |callback| with results on the calling thread.
  //
  // Can be called on any thread.
  void GetDocuments(const GURL& feed_url, const GetDataCallback& callback);

  // Delete a document identified by its 'self' |url| and |etag|.
  // Upon completion, invokes |callback| with results on the calling thread.
  //
  // Can be called on any thread.
  void DeleteDocument(const GURL& document_url,
                      const EntryActionCallback& callback);

  // Downloads a document identified by its |content_url| in a given |format|.
  // Upon completion, invokes |callback| with results on the calling thread.
  //
  // Can be called on any thread.
  void DownloadDocument(const GURL& content_url,
                        DocumentExportFormat format,
                        const DownloadActionCallback& callback);

  // Creates new collection with |directory_name| under parent directory
  // identified with |parent_content_url|. If |parent_content_url| is empty,
  // the new collection will be created in the root. Upon completion,
  // invokes |callback| and passes newly created entry on the calling thread.
  //
  // Can be called on any thread.
  void CreateDirectory(const GURL& parent_content_url,
                       const FilePath::StringType& directory_name,
                       const CreateEntryCallback& callback);

  // Downloads a file identified by its |content_url|. Upon completion, invokes
  // |callback| with results on the calling thread.
  //
  // Can be called on any thread.
  void DownloadFile(const GURL& content_url,
                    const DownloadActionCallback& callback);

  // Initiate uploading of a document/file.
  //
  // Can be called on any thread.
  void InitiateUpload(const InitiateUploadParams& params,
                      const InitiateUploadCallback& callback);

  // Resume uploading of a document/file.
  //
  // Can be called on any thread.
  void ResumeUpload(const ResumeUploadParams& params,
                    const ResumeUploadCallback& callback);

 private:
  // Helper function for GetDocuments(). We should initiate this operation on
  // the UI thread as the underlying GDataAuthService requires to be used on
  // the UI thread. |relay_proxy| is used to post a task to the origin thread.
  void GetDocumentsOnUIThread(
      const GURL& feed_url,
      const GetDataCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy);

  // Helper function for DeleteDocument().
  void DeleteDocumentOnUIThread(
    const GURL& document_url,
    const EntryActionCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy);

  // Helper function for DownloadFile().
  void DownloadFileOnUIThread(
    const GURL& document_url,
    const DownloadActionCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy);

  // Helper function for CreateDirectory.
  void CreateDirectoryOnUIThread(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const CreateEntryCallback& callback,
    scoped_refptr<base::MessageLoopProxy> relay_proxy);

  // Helper function for InitiateUpload.
  void InitiateUploadOnUIThread(
      const InitiateUploadParams& params,
      const InitiateUploadCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy);

  // Helper function for ResumeUpload.
  void ResumeUploadOnUIThread(
      const ResumeUploadParams& params,
      const ResumeUploadCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy);

    // GDataAuthService::Observer override.
  virtual void OnOAuth2RefreshTokenChanged() OVERRIDE;

  // TODO(zelidrag): Figure out how to declare/implement following
  // *OnAuthRefresh and On*Completed callbacks in a way that requires less code
  // duplication.

  // Callback when re-authenticating user during document list fetching.
  void GetDocumentsOnAuthRefresh(
      const GURL& feed_url,
      const GetDataCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      const std::string& auth_token);

  // Pass-through callback for re-authentication during document list fetching.
  // If the call is successful, parsed document feed will be returned as |root|.
  void OnGetDocumentsCompleted(
      const GURL& feed_url,
      const GetDataCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      base::Value* root);

  // Callback when re-authenticating user during document delete call.
  void DownloadDocumentOnAuthRefresh(
      const DownloadActionCallback& callback,
      const GURL& content_url,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      const std::string& token);

  // Pass-through callback for re-authentication during document
  // download request.
  void OnDownloadDocumentCompleted(
      const DownloadActionCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      const GURL& content_url,
      const FilePath& temp_file_path);

  // Callback when re-authenticating user during document delete call.
  void DeleteDocumentOnAuthRefresh(
      const EntryActionCallback& callback,
      const GURL& document_url,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      const std::string& token);

  // Pass-through callback for re-authentication during document delete request.
  void OnDeleteDocumentCompleted(
      const EntryActionCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      const GURL& document_url);

  // Callback when re-authenticating user during initiate upload call.
  void InitiateUploadOnAuthRefresh(
      const InitiateUploadParams& params,
      const InitiateUploadCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      const std::string& token);

  // Pass-through callback for re-authentication during initiate upload request.
  void OnInitiateUploadCompleted(
      const InitiateUploadParams& params,
      const InitiateUploadCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      const GURL& upload_location);

  // Callback when re-authenticating user during resume upload call.
  void ResumeUploadOnAuthRefresh(
      const ResumeUploadParams& params,
      const ResumeUploadCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      const std::string& token);

  // Pass-through callback for re-authentication during resume upload request.
  void OnResumeUploadCompleted(
      const ResumeUploadParams& params,
      const ResumeUploadCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      int64 start_range_received,
      int64 end_range_received);

  // Pass-through callback for re-authentication during directory create
  // request.
  void CreateDirectoryOnAuthRefresh(
      const GURL& parent_content_url,
      const FilePath::StringType& directory_name,
      const CreateEntryCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,

      GDataErrorCode error,
      const std::string& token);

  // Pass-through callback for CreateDirectory() completion request.
  void OnCreateDirectoryCompleted(
      const GURL& parent_content_url,
      const FilePath::StringType& directory_name,
      const CreateEntryCallback& callback,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      GDataErrorCode error,
      base::Value* document_entry);

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
