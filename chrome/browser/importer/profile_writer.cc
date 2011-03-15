// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/profile_writer.h"

#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"

using webkit_glue::PasswordForm;

ProfileWriter::BookmarkEntry::BookmarkEntry() : in_toolbar(false),
    is_folder(false) {}

ProfileWriter::BookmarkEntry::~BookmarkEntry() {}

ProfileWriter::ProfileWriter(Profile* profile) : profile_(profile) {}

bool ProfileWriter::BookmarkModelIsLoaded() const {
  return profile_->GetBookmarkModel()->IsLoaded();
}

bool ProfileWriter::TemplateURLModelIsLoaded() const {
  return profile_->GetTemplateURLModel()->loaded();
}

void ProfileWriter::AddPasswordForm(const PasswordForm& form) {
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

void ProfileWriter::AddBookmarkEntry(
    const std::vector<BookmarkEntry>& bookmark,
    const std::wstring& first_folder_name,
    int options) {
  BookmarkModel* model = profile_->GetBookmarkModel();
  DCHECK(model->IsLoaded());

  bool import_to_bookmark_bar = ((options & IMPORT_TO_BOOKMARK_BAR) != 0);
  std::wstring real_first_folder = import_to_bookmark_bar ? first_folder_name :
      GenerateUniqueFolderName(model, first_folder_name);

  bool show_bookmark_toolbar = false;
  std::set<const BookmarkNode*> groups_added_to;
  bool import_mode = false;
  if (bookmark.size() > 1) {
    model->BeginImportMode();
    import_mode = true;
  }
  for (std::vector<BookmarkEntry>::const_iterator it = bookmark.begin();
       it != bookmark.end(); ++it) {
    // Don't insert this url if it isn't valid.
    if (!it->is_folder && !it->url.is_valid())
      continue;

    // We suppose that bookmarks are unique by Title, URL, and Folder.  Since
    // checking for uniqueness may not be always the user's intention we have
    // this as an option.
    if (options & ADD_IF_UNIQUE && DoesBookmarkExist(model, *it,
        real_first_folder, import_to_bookmark_bar))
      continue;

    // Set up groups in BookmarkModel in such a way that path[i] is
    // the subgroup of path[i-1]. Finally they construct a path in the
    // model:
    //   path[0] \ path[1] \ ... \ path[size() - 1]
    const BookmarkNode* parent =
        (it->in_toolbar ? model->GetBookmarkBarNode() : model->other_node());
    for (std::vector<std::wstring>::const_iterator i = it->path.begin();
         i != it->path.end(); ++i) {
      const BookmarkNode* child = NULL;
      const std::wstring& folder_name = (!import_to_bookmark_bar &&
          !it->in_toolbar && (i == it->path.begin())) ? real_first_folder : *i;

      for (int index = 0; index < parent->child_count(); ++index) {
        const BookmarkNode* node = parent->GetChild(index);
        if ((node->type() == BookmarkNode::BOOKMARK_BAR ||
             node->type() == BookmarkNode::FOLDER) &&
            node->GetTitle() == WideToUTF16Hack(folder_name)) {
          child = node;
          break;
        }
      }
      if (child == NULL)
        child = model->AddGroup(parent, parent->child_count(),
                                WideToUTF16Hack(folder_name));
      parent = child;
    }
    groups_added_to.insert(parent);
    if (it->is_folder) {
      model->AddGroup(parent, parent->child_count(),
                      WideToUTF16Hack(it->title));
    } else {
      model->AddURLWithCreationTime(parent, parent->child_count(),
          WideToUTF16Hack(it->title), it->url, it->creation_time);
    }

    // If some items are put into toolbar, it looks like the user was using
    // it in their last browser. We turn on the bookmarks toolbar.
    if (it->in_toolbar)
      show_bookmark_toolbar = true;
  }

  // Reset the date modified time of the groups we added to. We do this to
  // make sure the 'recently added to' combobox in the bubble doesn't get random
  // groups.
  for (std::set<const BookmarkNode*>::const_iterator i =
          groups_added_to.begin();
       i != groups_added_to.end(); ++i) {
    model->ResetDateFolderModified(*i);
  }

  if (import_mode) {
    model->EndImportMode();
  }

  if (show_bookmark_toolbar && !(options & BOOKMARK_BAR_DISABLED))
    ShowBookmarkBar();
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

void ProfileWriter::ShowBookmarkBar() {
  DCHECK(profile_);

  PrefService* prefs = profile_->GetPrefs();
  // Check whether the bookmark bar is shown in current pref.
  if (!prefs->GetBoolean(prefs::kShowBookmarkBar)) {
    // Set the pref and notify the notification service.
    prefs->SetBoolean(prefs::kShowBookmarkBar, true);
    prefs->ScheduleSavePersistentPrefs();
    Source<Profile> source(profile_);
    NotificationService::current()->Notify(
        NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED, source,
        NotificationService::NoDetails());
  }
}

ProfileWriter::~ProfileWriter() {}

std::wstring ProfileWriter::GenerateUniqueFolderName(
    BookmarkModel* model,
    const std::wstring& folder_name) {
  // Build a set containing the folder names of the other folder.
  std::set<std::wstring> other_folder_names;
  const BookmarkNode* other = model->other_node();

  for (int i = 0, child_count = other->child_count(); i < child_count; ++i) {
    const BookmarkNode* node = other->GetChild(i);
    if (node->is_folder())
      other_folder_names.insert(UTF16ToWideHack(node->GetTitle()));
  }

  if (other_folder_names.find(folder_name) == other_folder_names.end())
    return folder_name;  // Name is unique, use it.

  // Otherwise iterate until we find a unique name.
  for (int i = 1; i < 100; ++i) {
    std::wstring name = folder_name + StringPrintf(L" (%d)", i);
    if (other_folder_names.find(name) == other_folder_names.end())
      return name;
  }

  return folder_name;
}

bool ProfileWriter::DoesBookmarkExist(
    BookmarkModel* model,
    const BookmarkEntry& entry,
    const std::wstring& first_folder_name,
    bool import_to_bookmark_bar) {
  std::vector<const BookmarkNode*> nodes_with_same_url;
  model->GetNodesByURL(entry.url, &nodes_with_same_url);
  if (nodes_with_same_url.empty())
    return false;

  for (size_t i = 0; i < nodes_with_same_url.size(); ++i) {
    const BookmarkNode* node = nodes_with_same_url[i];
    if (WideToUTF16Hack(entry.title) != node->GetTitle())
      continue;

    // Does the path match?
    bool found_match = true;
    const BookmarkNode* parent = node->parent();
    for (std::vector<std::wstring>::const_reverse_iterator path_it =
             entry.path.rbegin();
         (path_it != entry.path.rend()) && found_match; ++path_it) {
      const std::wstring& folder_name =
          (!import_to_bookmark_bar && path_it + 1 == entry.path.rend()) ?
          first_folder_name : *path_it;
      if (NULL == parent || *path_it != folder_name)
        found_match = false;
      else
        parent = parent->parent();
    }

    // We need a post test to differentiate checks such as
    // /home/hello and /hello. The parent should either by the other folder
    // node, or the bookmarks bar, depending upon import_to_bookmark_bar and
    // entry.in_toolbar.
    if (found_match &&
        ((import_to_bookmark_bar && entry.in_toolbar && parent !=
          model->GetBookmarkBarNode()) ||
         ((!import_to_bookmark_bar || !entry.in_toolbar) &&
           parent != model->other_node()))) {
      found_match = false;
    }

    if (found_match)
      return true;  // Found a match with the same url path and title.
  }
  return false;
}
