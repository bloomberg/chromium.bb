// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_
#pragma once

#include <list>
#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/history/history.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/file_stream.h"

class BookmarkNode;
class FilePath;
class Profile;

// Observer for bookmark html output. Used only in tests.
class BookmarksExportObserver {
 public:
  // Is invoked on the IO thread.
  virtual void OnExportFinished() = 0;

 protected:
  virtual ~BookmarksExportObserver() {}
};

// Class that fetches favicons for list of bookmarks and
// then starts Writer which outputs bookmarks and favicons to html file.
// Should be used only by WriteBookmarks function.
class BookmarkFaviconFetcher: public content::NotificationObserver {
 public:
  // Map of URL and corresponding favicons.
  typedef std::map<std::string, scoped_refptr<RefCountedMemory> > URLFaviconMap;

  BookmarkFaviconFetcher(Profile* profile,
                         const FilePath& path,
                         BookmarksExportObserver* observer);
  virtual ~BookmarkFaviconFetcher();

  // Executes bookmark export process.
  void ExportBookmarks();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Recursively extracts URLs from bookmarks.
  void ExtractUrls(const BookmarkNode* node);

  // Executes Writer task that writes bookmarks data to html file.
  void ExecuteWriter();

  // Starts async fetch for the next bookmark favicon.
  // Takes single url from bookmark_urls_ and removes it from the list.
  // Returns true if there are more favicons to extract.
  bool FetchNextFavicon();

  // Favicon fetch callback. After all favicons are fetched executes
  // html output on the file thread.
  void OnFaviconDataAvailable(FaviconService::Handle handle,
                              history::FaviconData favicon);

  // The Profile object used for accessing FaviconService, bookmarks model.
  Profile* profile_;

  // All URLs that are extracted from bookmarks. Used to fetch favicons
  // for each of them. After favicon is fetched top url is removed from list.
  std::list<std::string> bookmark_urls_;

  // Consumer for requesting favicons.
  CancelableRequestConsumer favicon_consumer_;

  // Map that stores favicon per URL.
  scoped_ptr<URLFaviconMap> favicons_map_;

  // Path where html output is stored.
  FilePath path_;

  BookmarksExportObserver* observer_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkFaviconFetcher);
};

namespace bookmark_html_writer {

// Writes the bookmarks out in the 'bookmarks.html' format understood by
// Firefox and IE. The results are written to the file at |path|.  The file
// thread is used.
// Before writing to the file favicons are fetched on the main thread.
// TODO(sky): need a callback on failure.
void WriteBookmarks(Profile* profile,
                    const FilePath& path,
                    BookmarksExportObserver* observer);

}  // namespace bookmark_html_writer

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_
