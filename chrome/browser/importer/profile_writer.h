// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_PROFILE_WRITER_H_
#define CHROME_BROWSER_IMPORTER_PROFILE_WRITER_H_
#pragma once

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/history/history_types.h"
#include "googleurl/src/gurl.h"

class Profile;
class TemplateURL;

struct IE7PasswordInfo;

namespace history {
struct ImportedFaviconUsage;
class URLRow;
}

namespace webkit_glue {
struct PasswordForm;
}

// ProfileWriter encapsulates profile for writing entries into it.
// This object must be invoked on UI thread.
class ProfileWriter : public base::RefCountedThreadSafe<ProfileWriter> {
 public:
  // Used to identify how the bookmarks are added.
  enum BookmarkOptions {
    // Indicates the bookmark should only be added if unique. Uniqueness
    // is done by title, url and path. That is, if this is passed to
    // AddBookmarkEntry the bookmark is added only if there is no other
    // URL with the same url, path and title.
    ADD_IF_UNIQUE = 1 << 0,

    // Indicates the bookmarks should be added to the bookmark bar.
    IMPORT_TO_BOOKMARK_BAR = 1 << 1,

    // Indicates the bookmark bar is not shown.
    BOOKMARK_BAR_DISABLED = 1 << 2
  };

  // A bookmark entry.
  // TODO(mirandac): remove instances of wstring from ProfileWriter
  // (http://crbug.com/43460).
  struct BookmarkEntry {
    BookmarkEntry();
    ~BookmarkEntry();

    bool in_toolbar;
    bool is_folder;
    GURL url;
    std::vector<std::wstring> path;
    std::wstring title;
    base::Time creation_time;
  };

  explicit ProfileWriter(Profile* profile);

  // These functions return true if the corresponding model has been loaded.
  // If the models haven't been loaded, the importer waits to run until they've
  // completed.
  virtual bool BookmarkModelIsLoaded() const;
  virtual bool TemplateURLModelIsLoaded() const;

  // Helper methods for adding data to local stores.
  virtual void AddPasswordForm(const webkit_glue::PasswordForm& form);
#if defined(OS_WIN)
  virtual void AddIE7PasswordInfo(const IE7PasswordInfo& info);
#endif
  virtual void AddHistoryPage(const std::vector<history::URLRow>& page,
                              history::VisitSource visit_source);
  virtual void AddHomepage(const GURL& homepage);
  // Adds the bookmarks to the BookmarkModel.
  // |options| is a bitmask of BookmarkOptions and dictates how and
  // which bookmarks are added. If the bitmask contains IMPORT_TO_BOOKMARK_BAR,
  // then any entries with a value of true for in_toolbar are added to
  // the bookmark bar. If the bitmask does not contain IMPORT_TO_BOOKMARK_BAR
  // then the folder name the bookmarks are added to is uniqued based on
  // |first_folder_name|. For example, if |first_folder_name| is 'foo'
  // and a folder with the name 'foo' already exists in the other
  // bookmarks folder, then the folder name 'foo 2' is used.
  // If |options| contains ADD_IF_UNIQUE, then the bookmark is added only
  // if another bookmarks does not exist with the same title, path and
  // url.
  virtual void AddBookmarkEntry(const std::vector<BookmarkEntry>& bookmark,
                                const std::wstring& first_folder_name,
                                int options);
  virtual void AddFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicons);
  // Add the TemplateURLs in |template_urls| to the local store and make the
  // TemplateURL at |default_keyword_index| the default keyword (does not set
  // a default keyword if it is -1).  The local store becomes the owner of the
  // TemplateURLs.  Some TemplateURLs in |template_urls| may conflict (same
  // keyword or same host name in the URL) with existing TemplateURLs in the
  // local store, in which case the existing ones takes precedence and the
  // duplicate in |template_urls| are deleted.
  // If unique_on_host_and_path a TemplateURL is only added if there is not an
  // existing TemplateURL that has a replaceable search url with the same
  // host+path combination.
  virtual void AddKeywords(const std::vector<TemplateURL*>& template_urls,
                           int default_keyword_index,
                           bool unique_on_host_and_path);

  // Shows the bookmarks toolbar.
  void ShowBookmarkBar();

  Profile* profile() const { return profile_; }

 protected:
  friend class base::RefCountedThreadSafe<ProfileWriter>;

  virtual ~ProfileWriter();

 private:
  // Generates a unique folder name. If folder_name is not unique, then this
  // repeatedly tests for '|folder_name| + (i)' until a unique name is found.
  std::wstring GenerateUniqueFolderName(BookmarkModel* model,
                                        const std::wstring& folder_name);

  // Returns true if a bookmark exists with the same url, title and path
  // as |entry|. |first_folder_name| is the name to use for the first
  // path entry if |import_to_bookmark_bar| is true.
  bool DoesBookmarkExist(BookmarkModel* model,
                         const BookmarkEntry& entry,
                         const std::wstring& first_folder_name,
                         bool import_to_bookmark_bar);

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ProfileWriter);
};

#endif  // CHROME_BROWSER_IMPORTER_PROFILE_WRITER_H_
