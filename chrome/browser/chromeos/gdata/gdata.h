// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace gdata {

// HTTP errors that can be returned by GData service.
enum GDataErrorCode {
  HTTP_SUCCESS               = 200,
  HTTP_CREATED               = 201,
  HTTP_FOUND                 = 302,
  HTTP_NOT_MODIFIED          = 304,
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

typedef base::Callback<void(GDataErrorCode error,
                            const std::string& token)> AuthStatusCallback;
typedef base::Callback<void(GDataErrorCode error,
                            base::Value* data)> GetDataCallback;
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& document_url)> EntryActionCallback;
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& content_url,
                            const FilePath& temp_file)> DownloadActionCallback;

// Base class for fetching content from GData based services. It integrates
// specific service integration with OAuth2 stack (TokenService) and provides
// OAuth2 token refresh infrastructure.
class GDataService : public base::SupportsWeakPtr<GDataService>,
                     public content::NotificationObserver {
 public:
  virtual void Initialize(Profile* profile);

 protected:
  GDataService();
  virtual ~GDataService();

  // Triggered when a new OAuth2 refresh token is received from TokenService.
  virtual void OnOAuth2RefreshTokenChanged() {}

  // Starts fetching OAuth2 auth token from the refresh token.
  void StartAuthentication(AuthStatusCallback callback);
  // True if OAuth2 auth token is retrieved and believed to be fresh.
  bool HasOAuth2AuthToken() { return !auth_token_.empty(); }
  // Gets OAuth2 auth token.
  const std::string& GetOAuth2AuthToken() { return auth_token_; }
  // True if OAuth2 refresh token is present. It's absence means that user
  // is not properly authenticated.
  bool HasOAuth2RefreshToken() { return !refresh_token_.empty(); }
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

  // Gets the document list. Upon completion, invokes |callback| with results.
  void GetDocuments(GetDataCallback callback);

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

 private:
  // TODO(zelidrag): Get rid of singleton here and make this service lifetime
  // associated with the Profile down the road.
  friend struct DefaultSingletonTraits<DocumentsService>;

  DocumentsService();
  virtual ~DocumentsService();

  // GDataService overrides.
  virtual void OnOAuth2RefreshTokenChanged() OVERRIDE;

  // TODO(zelidrag): Figure out how to declare/implement following 4 callbacks
  // in way that requires less code duplication.

  // Callback when re-authenticating user during document list fetching.
  void GetDocumentsOnAuthRefresh(GetDataCallback callback,
                                 GDataErrorCode error,
                                 const std::string& auth_token);
  // Pass-through callback for re-authentication during document list fetching.
  void OnGetDocumentsCompleted(GetDataCallback callback,
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

  // TODO(zelidrag): Remove this one once we figure out where the metadata will
  // really live.
  void UpdateFilelist(GDataErrorCode status, base::Value* data);
  scoped_ptr<base::Value> feed_value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DocumentsService);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_H_
