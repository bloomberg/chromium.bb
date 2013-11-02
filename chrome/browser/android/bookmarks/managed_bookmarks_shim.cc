// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks/managed_bookmarks_shim.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/policy/configuration_policy_handler_android.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using policy::ManagedBookmarksPolicyHandler;

ManagedBookmarksShim::ManagedBookmarksShim(PrefService* prefs)
    : prefs_(prefs) {
  registrar_.Init(prefs_);
  registrar_.Add(
      prefs::kManagedBookmarks,
      base::Bind(&ManagedBookmarksShim::Reload, base::Unretained(this)));
  Reload();
}

ManagedBookmarksShim::~ManagedBookmarksShim() {}

void ManagedBookmarksShim::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ManagedBookmarksShim::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ManagedBookmarksShim::HasManagedBookmarks() const {
  return !root_->empty();
}

bool ManagedBookmarksShim::IsManagedBookmark(const BookmarkNode* node) const {
  while (node != NULL) {
    if (node == root_.get())
      return true;
    node = node->parent();
  }
  return false;
}

const BookmarkNode* ManagedBookmarksShim::GetManagedBookmarksRoot() const {
  return root_.get();
}

const BookmarkNode* ManagedBookmarksShim::GetNodeByID(int64 id) const {
  if (root_->id() == id)
    return root_.get();
  for (int i = 0; i < root_->child_count(); ++i) {
    const BookmarkNode* child = root_->GetChild(i);
    if (child->id() == id)
      return child;
  }
  return NULL;
}

void ManagedBookmarksShim::Reload() {
  std::string domain;
  std::string username = prefs_->GetString(prefs::kGoogleServicesUsername);
  if (!username.empty())
    domain = gaia::ExtractDomainName(username);
  string16 root_node_name;
  if (domain.empty()) {
    root_node_name =
        l10n_util::GetStringUTF16(IDS_POLICY_MANAGED_BOOKMARKS_DEFAULT_NAME);
  } else {
    root_node_name = l10n_util::GetStringFUTF16(IDS_POLICY_MANAGED_BOOKMARKS,
                                                base::UTF8ToUTF16(domain));
  }

  root_.reset(new BookmarkPermanentNode(0));
  root_->SetTitle(root_node_name);

  const base::ListValue* list = prefs_->GetList(prefs::kManagedBookmarks);
  int64 id = 1;
  if (list) {
    for (base::ListValue::const_iterator it = list->begin();
         it != list->end(); ++it) {
      const base::DictionaryValue* dict = NULL;
      if (!*it || !(*it)->GetAsDictionary(&dict)) {
        NOTREACHED();
        continue;
      }

      string16 name;
      std::string url;
      if (!dict->GetString(ManagedBookmarksPolicyHandler::kName, &name) ||
          !dict->GetString(ManagedBookmarksPolicyHandler::kUrl, &url)) {
        NOTREACHED();
        continue;
      }

      BookmarkNode* node = new BookmarkNode(id++, GURL(url));
      node->set_type(BookmarkNode::URL);
      node->SetTitle(name);
      root_->Add(node, root_->child_count());
    }
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnManagedBookmarksChanged());
}
