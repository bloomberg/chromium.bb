// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The functionality provided here allows the user to import their bookmarks
// (favorites) from Google Toolbar.

#ifndef CHROME_BROWSER_IMPORTER_TOOLBAR_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_TOOLBAR_IMPORTER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/common/net/url_fetcher.h"

class ImporterBridge;
class XmlReader;

// Toolbar5Importer is a class which exposes the functionality needed to
// communicate with the Google Toolbar v5 front-end, negotiate the download of
// Toolbar bookmarks, parse them, and install them on the client.
// Toolbar5Importer should not have StartImport called more than once. Futher
// if StartImport is called, then the class must not be destroyed until it has
// either completed or Toolbar5Importer->Cancel() has been called.
class Toolbar5Importer : public URLFetcher::Delegate, public Importer {
 public:
  Toolbar5Importer();

  // Begin Importer implementation:

  // This method is called to begin the import process. |items| should only
  // either be NONE or FAVORITES, since as of right now these are the only
  // items this importer supports.
  virtual void StartImport(const importer::SourceProfile& source_profile,
                           uint16 items,
                           ImporterBridge* bridge) OVERRIDE;

  // This method is called when the user clicks the cancel button on the UI
  // dialog. We need to post a message to our loop to cancel network retrieval.
  virtual void Cancel() OVERRIDE;

  // End Importer implementation.

  // URLFetcher::Delegate method called back from the URLFetcher object.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 private:
  FRIEND_TEST_ALL_PREFIXES(Toolbar5ImporterTest, BookmarkParse);

  virtual ~Toolbar5Importer();

  // Internal states of the toolbar importer.
  enum InternalStateEnum {
    NOT_USED = -1,
    INITIALIZED,
    GET_AUTHORIZATION_TOKEN,
    GET_BOOKMARKS,
    PARSE_BOOKMARKS,
    DONE
  };

  typedef std::vector<string16> BookmarkFolderType;

  // URLs for connecting to the toolbar front end are defined below.
  static const char kT5AuthorizationTokenUrl[];
  static const char kT5FrontEndUrlTemplate[];

  // Token replacement tags are defined below.
  static const char kRandomNumberToken[];
  static const char kAuthorizationToken[];
  static const char kAuthorizationTokenPrefix[];
  static const char kAuthorizationTokenSuffix[];
  static const char kMaxNumToken[];
  static const char kMaxTimestampToken[];

  // XML tag names are defined below.
  static const char kXmlApiReplyXmlTag[];
  static const char kBookmarksXmlTag[];
  static const char kBookmarkXmlTag[];
  static const char kTitleXmlTag[];
  static const char kUrlXmlTag[];
  static const char kTimestampXmlTag[];
  static const char kLabelsXmlTag[];
  static const char kLabelsXmlCloseTag[];
  static const char kLabelXmlTag[];
  static const char kAttributesXmlTag[];

  // Flow control for asynchronous import is controlled by the methods below.
  // ContinueImport is called back by each import action taken.  BeginXXX
  // and EndXXX are responsible for updating the state of the asynchronous
  // import.  EndImport is responsible for state cleanup and notifying the
  // caller that import has completed.
  void ContinueImport();
  void EndImport();
  void BeginImportBookmarks();
  void EndImportBookmarks();

  // Network I/O is done by the methods below.  These three methods are called
  // in the order provided.  The last two are called back with the HTML
  // response provided by the Toolbar server.
  void GetAuthenticationFromServer();
  void GetBookmarkDataFromServer(const std::string& response);
  void GetBookmarksFromServerDataResponse(const std::string& response);

  // XML Parsing is implemented with the methods below.
  bool ParseAuthenticationTokenResponse(const std::string& response,
                                        std::string* token);

  static bool ParseBookmarksFromReader(
      XmlReader* reader,
      std::vector<ProfileWriter::BookmarkEntry>* bookmarks,
      const string16& bookmark_group_string);

  static bool LocateNextOpenTag(XmlReader* reader);
  static bool LocateNextTagByName(XmlReader* reader, const std::string& tag);
  static bool LocateNextTagWithStopByName(
      XmlReader* reader,
      const std::string& tag,
      const std::string& stop);

  static bool ExtractBookmarkInformation(
      XmlReader* reader,
      ProfileWriter::BookmarkEntry* bookmark_entry,
      std::vector<BookmarkFolderType>* bookmark_folders,
      const string16& bookmark_group_string);
  static bool ExtractNamedValueFromXmlReader(XmlReader* reader,
                                             const std::string& name,
                                             std::string* buffer);
  static bool ExtractTitleFromXmlReader(XmlReader* reader,
                                        ProfileWriter::BookmarkEntry* entry);
  static bool ExtractUrlFromXmlReader(XmlReader* reader,
                                      ProfileWriter::BookmarkEntry* entry);
  static bool ExtractTimeFromXmlReader(XmlReader* reader,
                                       ProfileWriter::BookmarkEntry* entry);
  static bool ExtractFoldersFromXmlReader(
      XmlReader* reader,
      std::vector<BookmarkFolderType>* bookmark_folders,
      const string16& bookmark_group_string);

  // Bookmark creation is done by the method below.
  void AddBookmarksToChrome(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks);

  InternalStateEnum state_;

  // Bitmask of Importer::ImportItem.
  uint16 items_to_import_;

  // The fetchers need to be available to cancel the network call on user cancel
  // hence they are stored as member variables.
  URLFetcher* token_fetcher_;
  URLFetcher* data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(Toolbar5Importer);
};

#endif  // CHROME_BROWSER_IMPORTER_TOOLBAR_IMPORTER_H_
