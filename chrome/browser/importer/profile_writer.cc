// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/profile_writer.h"

#include <string>

#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"

namespace {

// Generates a unique folder name. If |folder_name| is not unique, then this
// repeatedly tests for '|folder_name| + (i)' until a unique name is found.
string16 GenerateUniqueFolderName(BookmarkModel* model,
                                  const string16& folder_name) {
  // Build a set containing the bookmark bar folder names.
  std::set<string16> existing_folder_names;
  const BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
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
  PrefService* prefs = profile->GetPrefs();
  // Check whether the bookmark bar is shown in current pref.
  if (!prefs->GetBoolean(prefs::kShowBookmarkBar)) {
    // Set the pref and notify the notification service.
    prefs->SetBoolean(prefs::kShowBookmarkBar, true);
    prefs->ScheduleSavePersistentPrefs();
    Source<Profile> source(profile);
    NotificationService::current()->Notify(
        NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED, source,
        NotificationService::NoDetails());
  }
}

}  // namespace

ProfileWriter::BookmarkEntry::BookmarkEntry()
    : in_toolbar(false),
      is_folder(false) {}

ProfileWriter::BookmarkEntry::~BookmarkEntry() {}

ProfileWriter::ProfileWriter(Profile* profile) : profile_(profile) {}

bool ProfileWriter::BookmarkModelIsLoaded() const {
  return profile_->GetBookmarkModel()->IsLoaded();
}

bool ProfileWriter::TemplateURLModelIsLoaded() const {
  return profile_->GetTemplateURLModel()->loaded();
}

void ProfileWriter::AddPasswordForm(const webkit_glue::PasswordForm& form) {
  profile_->GetPasswordStore(Profile::EXPLICIT_ACCESS)->AddLogin(form);
}

#if defined(OS_WIN)
void ProfileWriter::AddIE7PasswordInfo(const IE7PasswordInfo& info) {
  profile_->GetWebDataService(Profile::EXPLICIT_ACCESS)->AddIE7Login(info);
}
#endif

void ProfileWriter::AddHistoryPage(const std::vector<history::URLRow>& page,
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
    prefs->ScheduleSavePersistentPrefs();
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
  const BookmarkNode* bookmark_bar = model->GetBookmarkBarNode();
  bool import_to_top_level = bookmark_bar->child_count() == 0;

  // If the user currently has no bookmarks in the bookmark bar, make sure that
  // at least some of the imported bookmarks end up there.  Otherwise, we'll end
  // up with just a single folder containing the imported bookmarks, which makes
  // for unnecessary nesting.
  bool add_all_to_top_level = import_to_top_level;
  for (std::vector<BookmarkEntry>::const_iterator it = bookmarks.begin();
       it != bookmarks.end() && add_all_to_top_level; ++it) {
    if (it->in_toolbar)
      add_all_to_top_level = false;
  }

  model->BeginImportMode();

  std::set<const BookmarkNode*> folders_added_to;
  const BookmarkNode* top_level_folder = NULL;
  for (std::vector<BookmarkEntry>::const_iterator bookmark = bookmarks.begin();
       bookmark != bookmarks.end(); ++bookmark) {
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

  model->EndImportMode();

  // If the user was previously using a toolbar, we should show the bar.
  if (import_to_top_level && !add_all_to_top_level)
    ShowBookmarkBar(profile_);
}

void ProfileWriter::AddFavicons(
    const std::vector<history::ImportedFaviconUsage>& favicons) {
  profile_->GetFaviconService(Profile::EXPLICIT_ACCESS)->
      SetImportedFavicons(favicons);
}

typedef std::map<std::string, const TemplateURL*> HostPathMap;

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
  if (t_url->url()) {
    if (try_url_if_invalid && !t_url->url()->IsValid())
      return HostPathKeyForURL(GURL(t_url->url()->url()));

    if (t_url->url()->SupportsReplacement()) {
      return HostPathKeyForURL(GURL(
          t_url->url()->ReplaceSearchTerms(
          *t_url, ASCIIToUTF16("random string"),
          TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16())));
    }
  }
  return std::string();
}

// Builds a set that contains an entry of the host+path for each TemplateURL in
// the TemplateURLModel that has a valid search url.
static void BuildHostPathMap(const TemplateURLModel& model,
                             HostPathMap* host_path_map) {
  std::vector<const TemplateURL*> template_urls = model.GetTemplateURLs();
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

void ProfileWriter::AddKeywords(const std::vector<TemplateURL*>& template_urls,
                                int default_keyword_index,
                                bool unique_on_host_and_path) {
  TemplateURLModel* model = profile_->GetTemplateURLModel();
  HostPathMap host_path_map;
  if (unique_on_host_and_path)
    BuildHostPathMap(*model, &host_path_map);

  for (std::vector<TemplateURL*>::const_iterator i = template_urls.begin();
       i != template_urls.end(); ++i) {
    TemplateURL* t_url = *i;
    bool default_keyword =
        default_keyword_index >= 0 &&
        (i - template_urls.begin() == default_keyword_index);

    // TemplateURLModel requires keywords to be unique. If there is already a
    // TemplateURL with this keyword, don't import it again.
    const TemplateURL* turl_with_keyword =
        model->GetTemplateURLForKeyword(t_url->keyword());
    if (turl_with_keyword != NULL) {
      if (default_keyword)
        model->SetDefaultSearchProvider(turl_with_keyword);
      delete t_url;
      continue;
    }

    // For search engines if there is already a keyword with the same
    // host+path, we don't import it. This is done to avoid both duplicate
    // search providers (such as two Googles, or two Yahoos) as well as making
    // sure the search engines we provide aren't replaced by those from the
    // imported browser.
    if (unique_on_host_and_path &&
        host_path_map.find(
            BuildHostPathKey(t_url, true)) != host_path_map.end()) {
      if (default_keyword) {
        const TemplateURL* turl_with_host_path =
            host_path_map[BuildHostPathKey(t_url, true)];
        if (turl_with_host_path)
          model->SetDefaultSearchProvider(turl_with_host_path);
        else
          NOTREACHED();  // BuildHostPathMap should only insert non-null values.
      }
      delete t_url;
      continue;
    }
    if (t_url->url() && t_url->url()->IsValid()) {
      model->Add(t_url);
      if (default_keyword && TemplateURL::SupportsReplacement(t_url))
        model->SetDefaultSearchProvider(t_url);
    } else {
      // Don't add invalid TemplateURLs to the model.
      delete t_url;
    }
  }
}

ProfileWriter::~ProfileWriter() {}
