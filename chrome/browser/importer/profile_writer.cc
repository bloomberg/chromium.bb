// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/profile_writer.h"

#include <map>
#include <set>
#include <string>

#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"

namespace {

// Generates a unique folder name. If |folder_name| is not unique, then this
// repeatedly tests for '|folder_name| + (i)' until a unique name is found.
string16 GenerateUniqueFolderName(BookmarkModel* model,
                                  const string16& folder_name) {
  // Build a set containing the bookmark bar folder names.
  std::set<string16> existing_folder_names;
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  for (int i = 0; i < bookmark_bar->child_count(); ++i) {
    const BookmarkNode* node = bookmark_bar->GetChild(i);
    if (node->is_folder())
      existing_folder_names.insert(node->GetTitle());
  }

  // If the given name is unique, use it.
  if (existing_folder_names.find(folder_name) == existing_folder_names.end())
    return folder_name;

  // Otherwise iterate until we find a unique name.
  for (size_t i = 1; i <= existing_folder_names.size(); ++i) {
    string16 name = folder_name + ASCIIToUTF16(" (") + base::IntToString16(i) +
        ASCIIToUTF16(")");
    if (existing_folder_names.find(name) == existing_folder_names.end())
      return name;
  }

  NOTREACHED();
  return folder_name;
}

// Shows the bookmarks toolbar.
void ShowBookmarkBar(Profile* profile) {
  profile->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);
}

}  // namespace

ProfileWriter::BookmarkEntry::BookmarkEntry()
    : in_toolbar(false),
      is_folder(false) {}

ProfileWriter::BookmarkEntry::~BookmarkEntry() {}

bool ProfileWriter::BookmarkEntry::operator==(
    const ProfileWriter::BookmarkEntry& other) const {
  return (in_toolbar == other.in_toolbar &&
          is_folder == other.is_folder &&
          url == other.url &&
          path == other.path &&
          title == other.title &&
          creation_time == other.creation_time);
}

ProfileWriter::ProfileWriter(Profile* profile) : profile_(profile) {}

bool ProfileWriter::BookmarkModelIsLoaded() const {
  return profile_->GetBookmarkModel()->IsLoaded();
}

bool ProfileWriter::TemplateURLServiceIsLoaded() const {
  return TemplateURLServiceFactory::GetForProfile(profile_)->loaded();
}

void ProfileWriter::AddPasswordForm(const webkit::forms::PasswordForm& form) {
  PasswordStoreFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS)->AddLogin(form);
}

#if defined(OS_WIN)
void ProfileWriter::AddIE7PasswordInfo(const IE7PasswordInfo& info) {
  profile_->GetWebDataService(Profile::EXPLICIT_ACCESS)->AddIE7Login(info);
}
#endif

void ProfileWriter::AddHistoryPage(const history::URLRows& page,
                                   history::VisitSource visit_source) {
  profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)->
      AddPagesWithDetails(page, visit_source);
}

void ProfileWriter::AddHomepage(const GURL& home_page) {
  DCHECK(profile_);

  PrefService* prefs = profile_->GetPrefs();
  // NOTE: We set the kHomePage value, but keep the NewTab page as the homepage.
  const PrefService::Preference* pref = prefs->FindPreference(prefs::kHomePage);
  if (pref && !pref->IsManaged()) {
    prefs->SetString(prefs::kHomePage, home_page.spec());
  }
}

void ProfileWriter::AddBookmarks(const std::vector<BookmarkEntry>& bookmarks,
                                 const string16& top_level_folder_name) {
  if (bookmarks.empty())
    return;

  BookmarkModel* model = profile_->GetBookmarkModel();
  DCHECK(model->IsLoaded());

  // If the bookmark bar is currently empty, we should import directly to it.
  // Otherwise, we should import everything to a subfolder.
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  bool import_to_top_level = bookmark_bar->empty();

  // Reorder bookmarks so that the toolbar entries come first.
  std::vector<BookmarkEntry> toolbar_bookmarks;
  std::vector<BookmarkEntry> reordered_bookmarks;
  for (std::vector<BookmarkEntry>::const_iterator it = bookmarks.begin();
       it != bookmarks.end(); ++it) {
    if (it->in_toolbar)
      toolbar_bookmarks.push_back(*it);
    else
      reordered_bookmarks.push_back(*it);
  }
  reordered_bookmarks.insert(reordered_bookmarks.begin(),
                             toolbar_bookmarks.begin(),
                             toolbar_bookmarks.end());

  // If the user currently has no bookmarks in the bookmark bar, make sure that
  // at least some of the imported bookmarks end up there.  Otherwise, we'll end
  // up with just a single folder containing the imported bookmarks, which makes
  // for unnecessary nesting.
  bool add_all_to_top_level = import_to_top_level && toolbar_bookmarks.empty();

  model->BeginExtensiveChanges();

  std::set<const BookmarkNode*> folders_added_to;
  const BookmarkNode* top_level_folder = NULL;
  for (std::vector<BookmarkEntry>::const_iterator bookmark =
         reordered_bookmarks.begin();
       bookmark != reordered_bookmarks.end(); ++bookmark) {
    // Disregard any bookmarks with invalid urls.
    if (!bookmark->is_folder && !bookmark->url.is_valid())
      continue;

    const BookmarkNode* parent = NULL;
    if (import_to_top_level && (add_all_to_top_level || bookmark->in_toolbar)) {
      // Add directly to the bookmarks bar.
      parent = bookmark_bar;
    } else {
      // Add to a folder that will contain all the imported bookmarks not added
      // to the bar.  The first time we do so, create the folder.
      if (!top_level_folder) {
        string16 name = GenerateUniqueFolderName(model, top_level_folder_name);
        top_level_folder = model->AddFolder(bookmark_bar,
                                            bookmark_bar->child_count(),
                                            name);
      }
      parent = top_level_folder;
    }

    // Ensure any enclosing folders are present in the model.  The bookmark's
    // enclosing folder structure should be
    //   path[0] > path[1] > ... > path[size() - 1]
    for (std::vector<string16>::const_iterator folder_name =
             bookmark->path.begin();
         folder_name != bookmark->path.end(); ++folder_name) {
      if (bookmark->in_toolbar && parent == bookmark_bar &&
          folder_name == bookmark->path.begin()) {
        // If we're importing directly to the bookmarks bar, skip over the
        // folder named "Bookmarks Toolbar" (or any non-Firefox equivalent).
        continue;
      }

      const BookmarkNode* child = NULL;
      for (int index = 0; index < parent->child_count(); ++index) {
        const BookmarkNode* node = parent->GetChild(index);
        if (node->is_folder() && node->GetTitle() == *folder_name) {
          child = node;
          break;
        }
      }
      if (!child)
        child = model->AddFolder(parent, parent->child_count(), *folder_name);
      parent = child;
    }

    folders_added_to.insert(parent);
    if (bookmark->is_folder) {
      model->AddFolder(parent, parent->child_count(), bookmark->title);
    } else {
      model->AddURLWithCreationTime(parent, parent->child_count(),
                                    bookmark->title, bookmark->url,
                                    bookmark->creation_time);
    }
  }

  // In order to keep the imported-to folders from appearing in the 'recently
  // added to' combobox, reset their modified times.
  for (std::set<const BookmarkNode*>::const_iterator i =
           folders_added_to.begin();
       i != folders_added_to.end(); ++i) {
    model->ResetDateFolderModified(*i);
  }

  model->EndExtensiveChanges();

  // If the user was previously using a toolbar, we should show the bar.
  if (import_to_top_level && !add_all_to_top_level)
    ShowBookmarkBar(profile_);
}

void ProfileWriter::AddFavicons(
    const std::vector<history::ImportedFaviconUsage>& favicons) {
  profile_->GetFaviconService(Profile::EXPLICIT_ACCESS)->
      SetImportedFavicons(favicons);
}

typedef std::map<std::string, TemplateURL*> HostPathMap;

// Returns the key for the map built by BuildHostPathMap. If url_string is not
// a valid URL, an empty string is returned, otherwise host+path is returned.
static std::string HostPathKeyForURL(const GURL& url) {
  return url.is_valid() ? url.host() + url.path() : std::string();
}

// Builds the key to use in HostPathMap for the specified TemplateURL. Returns
// an empty string if a host+path can't be generated for the TemplateURL.
// If an empty string is returned, the TemplateURL should not be added to
// HostPathMap.
//
// If |try_url_if_invalid| is true, and |t_url| isn't valid, a string is built
// from the raw TemplateURL string. Use a value of true for |try_url_if_invalid|
// when checking imported URLs as the imported URL may not be valid yet may
// match the host+path of one of the default URLs. This is used to catch the
// case of IE using an invalid OSDD URL for Live Search, yet the host+path
// matches our prepopulate data. IE's URL for Live Search is something like
// 'http://...{Language}...'. As {Language} is not a valid OSDD parameter value
// the TemplateURL is invalid.
static std::string BuildHostPathKey(const TemplateURL* t_url,
                                    bool try_url_if_invalid) {
  if (try_url_if_invalid && !t_url->url_ref().IsValid())
    return HostPathKeyForURL(GURL(t_url->url()));

  if (t_url->url_ref().SupportsReplacement()) {
    return HostPathKeyForURL(GURL(
        t_url->url_ref().ReplaceSearchTerms(ASCIIToUTF16("x"),
            TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16())));
  }
  return std::string();
}

// Builds a set that contains an entry of the host+path for each TemplateURL in
// the TemplateURLService that has a valid search url.
static void BuildHostPathMap(TemplateURLService* model,
                             HostPathMap* host_path_map) {
  TemplateURLService::TemplateURLVector template_urls =
      model->GetTemplateURLs();
  for (size_t i = 0; i < template_urls.size(); ++i) {
    const std::string host_path = BuildHostPathKey(template_urls[i], false);
    if (!host_path.empty()) {
      const TemplateURL* existing_turl = (*host_path_map)[host_path];
      if (!existing_turl ||
          (template_urls[i]->show_in_default_list() &&
           !existing_turl->show_in_default_list())) {
        // If there are multiple TemplateURLs with the same host+path, favor
        // those shown in the default list.  If there are multiple potential
        // defaults, favor the first one, which should be the more commonly used
        // one.
        (*host_path_map)[host_path] = template_urls[i];
      }
    }  // else case, TemplateURL doesn't have a search url, doesn't support
       // replacement, or doesn't have valid GURL. Ignore it.
  }
}

void ProfileWriter::AddKeywords(ScopedVector<TemplateURL> template_urls,
                                bool unique_on_host_and_path) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  HostPathMap host_path_map;
  if (unique_on_host_and_path)
    BuildHostPathMap(model, &host_path_map);

  for (ScopedVector<TemplateURL>::iterator i = template_urls.begin();
       i != template_urls.end(); ++i) {
    // TemplateURLService requires keywords to be unique. If there is already a
    // TemplateURL with this keyword, don't import it again.
    if (model->GetTemplateURLForKeyword((*i)->keyword()) != NULL)
      continue;

    // For search engines if there is already a keyword with the same
    // host+path, we don't import it. This is done to avoid both duplicate
    // search providers (such as two Googles, or two Yahoos) as well as making
    // sure the search engines we provide aren't replaced by those from the
    // imported browser.
    if (unique_on_host_and_path &&
        (host_path_map.find(BuildHostPathKey(*i, true)) != host_path_map.end()))
      continue;

    // Only add valid TemplateURLs to the model.
    if ((*i)->url_ref().IsValid()) {
      model->AddAndSetProfile(*i, profile_);  // Takes ownership.
      *i = NULL;  // Prevent the vector from deleting *i later.
    }
  }
}

ProfileWriter::~ProfileWriter() {}
