// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_FIREFOX2_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_FIREFOX2_IMPORTER_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/profile_writer.h"
#include "googleurl/src/gurl.h"

class TemplateURL;

// Importer for Mozilla Firefox 2.
class Firefox2Importer : public Importer {
 public:
  Firefox2Importer();

  // Importer:
  virtual void StartImport(const importer::ProfileInfo& profile_info,
                           uint16 items,
                           ImporterBridge* bridge) OVERRIDE;

  // Loads the default bookmarks in the Firefox installed at |firefox_app_path|,
  // and stores their locations in |urls|.
  static void LoadDefaultBookmarks(const FilePath& firefox_app_path,
                                   std::set<GURL> *urls);

  // Creates a TemplateURL with the |keyword| and |url|. |title| may be empty.
  // This function transfers ownership of the created TemplateURL to the caller.
  static TemplateURL* CreateTemplateURL(const std::wstring& title,
                                        const std::wstring& keyword,
                                        const GURL& url);

  // Imports the bookmarks from the specified file. |template_urls| and
  // |favicons| may be null, in which case TemplateURLs and favicons are
  // not parsed. Any bookmarks in |default_urls| are ignored.
  static void ImportBookmarksFile(
      const FilePath& file_path,
      const std::set<GURL>& default_urls,
      bool import_to_bookmark_bar,
      const std::wstring& first_folder_name,
      Importer* importer,
      std::vector<ProfileWriter::BookmarkEntry>* bookmarks,
      std::vector<TemplateURL*>* template_urls,
      std::vector<history::ImportedFaviconUsage>* favicons);

 private:
  FRIEND_TEST_ALL_PREFIXES(FirefoxImporterTest, Firefox2BookmarkParse);
  FRIEND_TEST_ALL_PREFIXES(FirefoxImporterTest, Firefox2CookesParse);
  FRIEND_TEST_ALL_PREFIXES(FirefoxImporterTest, Firefox2BookmarkFileImport);

  virtual ~Firefox2Importer();

  void ImportBookmarks();
  void ImportPasswords();
  void ImportHistory();
  void ImportSearchEngines();
  // Import the user's home page, unless it is set to default home page as
  // defined in browserconfig.properties.
  void ImportHomepage();

  // Fills |files| with the paths to the files containing the search engine
  // descriptions.
  void GetSearchEnginesXMLFiles(std::vector<FilePath>* files);

  // Helper methods for parsing bookmark file.
  // Firefox 2 saves its bookmarks in a html file. We are interested in the
  // bookmarks and folders, and their hierarchy. A folder starts with a
  // heading tag, which contains it title. All bookmarks and sub-folders is
  // following, and bracketed by a <DL> tag:
  //   <DT><H3 PERSONAL_TOOLBAR_FOLDER="true" ...>title</H3>
  //   <DL><p>
  //      ... container ...
  //   </DL><p>
  // And a bookmark is presented by a <A> tag:
  //   <DT><A HREF="url" SHORTCUTURL="shortcut" ADD_DATE="11213014"...>name</A>
  // Reference: http://kb.mozillazine.org/Bookmarks.html
  static bool ParseCharsetFromLine(const std::string& line,
                                   std::string* charset);
  static bool ParseFolderNameFromLine(const std::string& line,
                                      const std::string& charset,
                                      std::wstring* folder_name,
                                      bool* is_toolbar_folder,
                                      base::Time* add_date);
  // See above, this will also put the data: URL of the favicon into *favicon
  // if there is a favicon given.  |post_data| is set for POST base keywords to
  // the contents of the actual POST (with %s for the search term).
  static bool ParseBookmarkFromLine(const std::string& line,
                                    const std::string& charset,
                                    std::wstring* title,
                                    GURL* url,
                                    GURL* favicon,
                                    std::wstring* shortcut,
                                    base::Time* add_date,
                                    std::wstring* post_data);
  // Save bookmarks imported from browsers with Firefox2 compatible bookmark
  // systems such as Epiphany. This bookmark format is the same as that of the
  // basic Firefox bookmark, but it misses additional properties and uses
  // lower-case tag:
  //   ...<h1>Bookmarks</h1><dl>
  //   <dt><a href="url">name</a></dt>
  //   <dt><a href="url">name</a></dt>
  //   </dl>
  static bool ParseMinimumBookmarkFromLine(const std::string& line,
                                           const std::string& charset,
                                           std::wstring* title,
                                           GURL* url);

  // Fetches the given attribute value from the |tag|. Returns true if
  // successful, and |value| will contain the value.
  static bool GetAttribute(const std::string& tag,
                           const std::string& attribute,
                           std::string* value);

  // There are some characters in html file will be escaped:
  //   '<', '>', '"', '\', '&'
  // Un-escapes them if the bookmark name has those characters.
  static void HTMLUnescape(std::wstring* text);

  // Fills |xml_files| with the file with an xml extension found under |dir|.
  static void FindXMLFilesInDir(const FilePath& dir,
                                std::vector<FilePath>* xml_files);

  // Given the URL of a page and a favicon data URL, adds an appropriate record
  // to the given favicon usage vector. Will do nothing if the favicon is not
  // valid.
  static void DataURLToFaviconUsage(
      const GURL& link_url,
      const GURL& favicon_data,
      std::vector<history::ImportedFaviconUsage>* favicons);

  FilePath source_path_;
  FilePath app_path_;
  // If true, we only parse the bookmarks.html file specified as source_path_.
  bool parsing_bookmarks_html_file_;

  DISALLOW_COPY_AND_ASSIGN(Firefox2Importer);
};

#endif  // CHROME_BROWSER_IMPORTER_FIREFOX2_IMPORTER_H_
