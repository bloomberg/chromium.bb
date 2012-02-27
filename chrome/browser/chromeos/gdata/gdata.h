// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
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

class GDataUploader;

// HTTP errors that can be returned by GData service.
enum GDataErrorCode {
  HTTP_SUCCESS               = 200,
  HTTP_CREATED               = 201,
  HTTP_FOUND                 = 302,
  HTTP_NOT_MODIFIED          = 304,
  HTTP_RESUME_INCOMPLETE     = 308,
  HTTP_BAD_REQUEST           = 400,
  HTTP_UNAUTHORIZED          = 401,
  HTTP_FORBIDDEN             = 403,
  HTTP_NOT_FOUND             = 404,
  HTTP_CONFLICT              = 409,
  HTTP_LENGTH_REQUIRED       = 411,
  HTTP_PRECONDITION          = 412,
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_SERVICE_UNAVAILABLE   = 503,
  GDATA_PARSE_ERROR          = -100,
  GDATA_FILE_ERROR           = -101,
};

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

// Structure containing current upload information of file, passed between
// DocumentsService methods and callbacks.
struct UploadFileInfo {
  UploadFileInfo();
  ~UploadFileInfo();

  // Data to be initialized by caller before initiating upload request.
  // URL of physical file to be uploaded, used as main identifier in callbacks.
  GURL file_url;  // file: url of the file to the uploaded.
  FilePath file_path;  // The path of the file to be uploaded.
  size_t file_size;  // Last known size of the file.

  std::string title;  // Title to be used for file to be uploaded.
  std::string content_type;  // Content-Type of file.
  int64 content_length;  // Header content-Length.

  // Data cached by caller and used when preparing upload data in chunks for
  // multiple ResumeUpload requests.
  // Location URL where file is to be uploaded to, returned from InitiateUpload.
  GURL upload_location;
  // TODO(kuan): Use generic stream object after FileStream is refactored to
  // extend a generic stream.
  net::FileStream* file_stream;  // For opening and reading from physical file.
  scoped_refptr<net::IOBuffer> buf;  // Holds current content to be uploaded.
  // Size of |buf|, max is 512KB; Google Docs requires size of each upload chunk
  // to be a multiple of 512KB.
  size_t buf_len;

  // Data to be updated by caller before sending each ResumeUpload request.
  // Note that end_range is inclusive, so start_range=0, end_range=8 is 9 bytes.
  int64 start_range;  // Start of range of contents currently stored in |buf|.
  int64 end_range;  // End of range of contents currently stored in |buf|.

  bool download_complete;  // Whether this file has finished downloading.
};

// Different callback types for various functionalities in DocumentsService.
typedef base::Callback<void(GDataErrorCode error,
                            const std::string& token)> AuthStatusCallback;
typedef base::Callback<void(GDataErrorCode error,
                            base::Value* data)> GetDataCallback;
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& document_url)> EntryActionCallback;
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& content_url,
                            const FilePath& temp_file)> DownloadActionCallback;
typedef base::Callback<void(GDataErrorCode error,
                            const UploadFileInfo& upload_file_info,
                            const GURL& upload_location)>
    InitiateUploadCallback;
typedef base::Callback<void(GDataErrorCode error,
                            const UploadFileInfo& upload_file_info,
                            int64 start_range_received,
                            int64 end_range_received) >
    ResumeUploadCallback;

// Base class for fetching content from GData based services. It integrates
// specific service integration with OAuth2 stack (TokenService) and provides
// OAuth2 token refresh infrastructure.
class GDataService : public base::SupportsWeakPtr<GDataService>,
                     public content::NotificationObserver {
 public:
  virtual void Initialize(Profile* profile);

  // Starts fetching OAuth2 auth token from the refresh token.
  void StartAuthentication(AuthStatusCallback callback);

  // True if OAuth2 auth token is retrieved and believed to be fresh.
  bool IsFullyAuthenticated() const { return !auth_token_.empty(); }

  // True if OAuth2 refresh token is present. It's absence means that user
  // is not properly authenticated.
  bool IsPartiallyAuthenticated() const { return !refresh_token_.empty(); }

  // Gets OAuth2 auth token.
  const std::string& oauth2_auth_token() const { return auth_token_; }

 protected:
  GDataService();
  virtual ~GDataService();

  // Triggered when a new OAuth2 refresh token is received from TokenService.
  virtual void OnOAuth2RefreshTokenChanged() {}

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

  Profile* profile_;
  std::string refresh_token_;
  std::string auth_token_;

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(GDataService);
};

// This calls provides documents feed service calls.
class DocumentsService : public GDataService {
 public:
  // Get Singleton instance of DocumentsService.
  static DocumentsService* GetInstance();

  // GDataService override.
  virtual void Initialize(Profile* profile) OVERRIDE;

  // Gets the document feed from |feed_url|. If this URL is empty, the call
  // will fetch the default ('root') document feed. Upon completion,
  // invokes |callback| with results.
  void GetDocuments(const GURL& feed_url, const GetDataCallback& callback);

  // Delete a document identified by its 'self' |url| and |etag|.
  // Upon completion, invokes |callback| with results.
  void DeleteDocument(const GURL& document_url,
                      EntryActionCallback callback);

  // Downloads a document identified by its |content_url| in a given |format|.
  // Upon completion, invokes |callback| with results.
  void DownloadDocument(const GURL& content_url,
                        DocumentExportFormat format,
                        DownloadActionCallback callback);

  // Downloads a file identified by its |content_url|. Upon completion, invokes
  // |callback| with results.
  void DownloadFile(const GURL& content_url,
                    DownloadActionCallback callback);

  // Initiate uploading of a document/file.
  void InitiateUpload(const UploadFileInfo& upload_file_info,
                      InitiateUploadCallback callback);

  // Resume uploading of a document/file.
  void ResumeUpload(const UploadFileInfo& upload_file_info,
                    ResumeUploadCallback callback);

 private:
  // TODO(zelidrag): Get rid of singleton here and make this service lifetime
  // associated with the Profile down the road.
  friend struct DefaultSingletonTraits<DocumentsService>;

  // Used to queue callers of InitiateUpload when document feed is not ready.
  struct InitiateUploadCaller {
    InitiateUploadCaller(InitiateUploadCallback in_callback,
                         const UploadFileInfo& in_upload_file_info)
        : callback(in_callback),
          upload_file_info(in_upload_file_info) {
    }
    ~InitiateUploadCaller() {}

    InitiateUploadCallback callback;
    UploadFileInfo upload_file_info;
  };

  typedef std::vector<InitiateUploadCaller> InitiateUploadCallerQueue;

  struct SameInitiateUploadCaller {
   public:
    explicit SameInitiateUploadCaller(const GURL& in_url)
        : url(in_url) {
    }

    bool operator()(const InitiateUploadCaller& caller) const {
      return caller.upload_file_info.file_url == url;
    }

    GURL url;
  };

  DocumentsService();
  virtual ~DocumentsService();

  // GDataService overrides.
  virtual void OnOAuth2RefreshTokenChanged() OVERRIDE;

  // TODO(zelidrag): Figure out how to declare/implement following
  // *OnAuthRefresh and On*Completed callbacks in a way that requires less code
  // duplication.

  // Callback when re-authenticating user during document list fetching.
  void GetDocumentsOnAuthRefresh(const GURL& feed_url,
                                 const GetDataCallback& callback,
                                 GDataErrorCode error,
                                 const std::string& auth_token);

  // Pass-through callback for re-authentication during document list fetching.
  // If the call is successful, parsed document feed will be returned as |root|.
  void OnGetDocumentsCompleted(const GURL& feed_url,
                               const GetDataCallback& callback,
                               GDataErrorCode error,
                               base::Value* root);

  // Callback when re-authenticating user during document delete call.
  void DownloadDocumentOnAuthRefresh(DownloadActionCallback callback,
                                     const GURL& content_url,
                                     GDataErrorCode error,
                                     const std::string& token);

  // Pass-through callback for re-authentication during document
  // download request.
  void OnDownloadDocumentCompleted(DownloadActionCallback callback,
                                   GDataErrorCode error,
                                   const GURL& content_url,
                                   const FilePath& temp_file_path);

  // Callback when re-authenticating user during document delete call.
  void DeleteDocumentOnAuthRefresh(EntryActionCallback callback,
                                   const GURL& document_url,
                                   GDataErrorCode error,
                                   const std::string& token);

  // Pass-through callback for re-authentication during document delete request.
  void OnDeleteDocumentCompleted(EntryActionCallback callback,
                                 GDataErrorCode error,
                                 const GURL& document_url);

  // Callback when re-authenticating user during initiate upload call.
  void InitiateUploadOnAuthRefresh(InitiateUploadCallback callback,
                                   const UploadFileInfo& upload_file_info,
                                   GDataErrorCode error,
                                   const std::string& token);

  // Pass-through callback for re-authentication during initiate upload request.
  void OnInitiateUploadCompleted(InitiateUploadCallback callback,
                                 GDataErrorCode error,
                                 const UploadFileInfo& upload_file_info,
                                 const GURL& upload_location);

  // Callback when re-authenticating user during resume upload call.
  void ResumeUploadOnAuthRefresh(ResumeUploadCallback callback,
                                 const UploadFileInfo& upload_file_info,
                                 GDataErrorCode error,
                                 const std::string& token);

  // Pass-through callback for re-authentication during resume upload request.
  void OnResumeUploadCompleted(ResumeUploadCallback callback,
                               GDataErrorCode error,
                               const UploadFileInfo& upload_file_info,
                               int64 start_range_received,
                               int64 end_range_received);

  // TODO(zelidrag): Remove this one once we figure out where the metadata will
  // really live.
  // Callback for GetDocuments.
  void UpdateFilelist(GDataErrorCode status, base::Value* data);

  // Data members.
  scoped_ptr<base::Value> feed_value_;

  // True if GetDocuments has been started.
  // We can't just check if |feed_value_| is NULL, because GetDocuments may have
  // started but waiting for server response.
  bool get_documents_started_;

  // Queue of InitiateUpload callers while we wait for feed info from server
  // via GetDocuments.
  InitiateUploadCallerQueue initiate_upload_callers_;

  scoped_ptr<GDataUploader> uploader_;

  DISALLOW_COPY_AND_ASSIGN(DocumentsService);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
