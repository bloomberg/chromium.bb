// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include <utility>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/query_parser.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/user_metrics.h"
#include "net/base/net_util.h"
#include "ui/base/models/tree_node_iterator.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/bookmarks/scoped_group_bookmark_actions.h"
#endif

using base::Time;

namespace {

void CloneBookmarkNodeImpl(BookmarkModel* model,
                           const BookmarkNodeData::Element& element,
                           const BookmarkNode* parent,
                           int index_to_add_at,
                           bool reset_node_times) {
  const BookmarkNode* cloned_node = NULL;
  if (element.is_url) {
    if (reset_node_times) {
      cloned_node = model->AddURL(parent, index_to_add_at, element.title,
                                  element.url);
    } else {
      DCHECK(!element.date_added.is_null());
      cloned_node = model->AddURLWithCreationTime(parent, index_to_add_at,
                                                  element.title, element.url,
                                                  element.date_added);
    }
  } else {
    cloned_node = model->AddFolder(parent, index_to_add_at, element.title);
    if (!reset_node_times) {
      DCHECK(!element.date_folder_modified.is_null());
      model->SetDateFolderModified(cloned_node, element.date_folder_modified);
    }
    for (int i = 0; i < static_cast<int>(element.children.size()); ++i)
      CloneBookmarkNodeImpl(model, element.children[i], cloned_node, i,
                            reset_node_times);
  }
  model->SetNodeMetaInfoMap(cloned_node, element.meta_info_map);
}

// Comparison function that compares based on date modified of the two nodes.
bool MoreRecentlyModified(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_folder_modified() > n2->date_folder_modified();
}

// Returns true if |text| contains each string in |words|. This is used when
// searching for bookmarks.
bool DoesBookmarkTextContainWords(const base::string16& text,
                                  const std::vector<base::string16>& words) {
  for (size_t i = 0; i < words.size(); ++i) {
    if (!base::i18n::StringSearchIgnoringCaseAndAccents(
            words[i], text, NULL, NULL)) {
      return false;
    }
  }
  return true;
}

// Returns true if |node|s title or url contains the strings in |words|.
// |languages| argument is user's accept-language setting to decode IDN.
bool DoesBookmarkContainWords(const BookmarkNode* node,
                              const std::vector<base::string16>& words,
                              const std::string& languages) {
  return
      DoesBookmarkTextContainWords(node->GetTitle(), words) ||
      DoesBookmarkTextContainWords(
          base::UTF8ToUTF16(node->url().spec()), words) ||
      DoesBookmarkTextContainWords(net::FormatUrl(
          node->url(), languages, net::kFormatUrlOmitNothing,
          net::UnescapeRule::NORMAL, NULL, NULL, NULL), words);
}

// This is used with a tree iterator to skip subtrees which are not visible.
bool PruneInvisibleFolders(const BookmarkNode* node) {
  return !node->IsVisible();
}

// This traces parents up to root, determines if node is contained in a
// selected folder.
bool HasSelectedAncestor(BookmarkModel* model,
                         const std::vector<const BookmarkNode*>& selected_nodes,
                         const BookmarkNode* node) {
  if (!node || model->is_permanent_node(node))
    return false;

  for (size_t i = 0; i < selected_nodes.size(); ++i)
    if (node->id() == selected_nodes[i]->id())
      return true;

  return HasSelectedAncestor(model, selected_nodes, node->parent());
}

}  // namespace

namespace bookmark_utils {

QueryFields::QueryFields() {}
QueryFields::~QueryFields() {}

void CloneBookmarkNode(BookmarkModel* model,
                       const std::vector<BookmarkNodeData::Element>& elements,
                       const BookmarkNode* parent,
                       int index_to_add_at,
                       bool reset_node_times) {
  if (!parent->is_folder() || !model) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < elements.size(); ++i) {
    CloneBookmarkNodeImpl(model, elements[i], parent, index_to_add_at + i,
                          reset_node_times);
  }
}

void CopyToClipboard(BookmarkModel* model,
                     const std::vector<const BookmarkNode*>& nodes,
                     bool remove_nodes) {
  if (nodes.empty())
    return;

  // Create array of selected nodes with descendants filtered out.
  std::vector<const BookmarkNode*> filtered_nodes;
  for (size_t i = 0; i < nodes.size(); ++i)
    if (!HasSelectedAncestor(model, nodes, nodes[i]->parent()))
      filtered_nodes.push_back(nodes[i]);

  BookmarkNodeData(filtered_nodes).
      WriteToClipboard(ui::CLIPBOARD_TYPE_COPY_PASTE);

  if (remove_nodes) {
#if !defined(OS_ANDROID)
    ScopedGroupBookmarkActions group_cut(model);
#endif
    for (size_t i = 0; i < filtered_nodes.size(); ++i) {
      int index = filtered_nodes[i]->parent()->GetIndexOf(filtered_nodes[i]);
      if (index > -1)
        model->Remove(filtered_nodes[i]->parent(), index);
    }
  }
}

void PasteFromClipboard(BookmarkModel* model,
                        const BookmarkNode* parent,
                        int index) {
  if (!parent)
    return;

  BookmarkNodeData bookmark_data;
  if (!bookmark_data.ReadFromClipboard(ui::CLIPBOARD_TYPE_COPY_PASTE))
    return;

  if (index == -1)
    index = parent->child_count();
#if !defined(OS_ANDROID)
  ScopedGroupBookmarkActions group_paste(model);
#endif
  CloneBookmarkNode(model, bookmark_data.elements, parent, index, true);
}

bool CanPasteFromClipboard(const BookmarkNode* node) {
  if (!node)
    return false;
  return BookmarkNodeData::ClipboardContainsBookmarks();
}

std::vector<const BookmarkNode*> GetMostRecentlyModifiedFolders(
    BookmarkModel* model,
    size_t max_count) {
  std::vector<const BookmarkNode*> nodes;
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node(),
                                                    PruneInvisibleFolders);

  while (iterator.has_next()) {
    const BookmarkNode* parent = iterator.Next();
    if (parent->is_folder() && parent->date_folder_modified() > base::Time()) {
      if (max_count == 0) {
        nodes.push_back(parent);
      } else {
        std::vector<const BookmarkNode*>::iterator i =
            std::upper_bound(nodes.begin(), nodes.end(), parent,
                             &MoreRecentlyModified);
        if (nodes.size() < max_count || i != nodes.end()) {
          nodes.insert(i, parent);
          while (nodes.size() > max_count)
            nodes.pop_back();
        }
      }
    }  // else case, the root node, which we don't care about or imported nodes
       // (which have a time of 0).
  }

  if (nodes.size() < max_count) {
    // Add the permanent nodes if there is space. The permanent nodes are the
    // only children of the root_node.
    const BookmarkNode* root_node = model->root_node();

    for (int i = 0; i < root_node->child_count(); ++i) {
      const BookmarkNode* node = root_node->GetChild(i);
      if (node->IsVisible() &&
          std::find(nodes.begin(), nodes.end(), node) == nodes.end()) {
        nodes.push_back(node);

        if (nodes.size() == max_count)
          break;
      }
    }
  }
  return nodes;
}

void GetMostRecentlyAddedEntries(BookmarkModel* model,
                                 size_t count,
                                 std::vector<const BookmarkNode*>* nodes) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->is_url()) {
      std::vector<const BookmarkNode*>::iterator insert_position =
          std::upper_bound(nodes->begin(), nodes->end(), node,
                           &MoreRecentlyAdded);
      if (nodes->size() < count || insert_position != nodes->end()) {
        nodes->insert(insert_position, node);
        while (nodes->size() > count)
          nodes->pop_back();
      }
    }
  }
}

bool MoreRecentlyAdded(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_added() > n2->date_added();
}

void GetBookmarksMatchingProperties(BookmarkModel* model,
                                    const QueryFields& query,
                                    size_t max_count,
                                    const std::string& languages,
                                    std::vector<const BookmarkNode*>* nodes) {
  std::vector<base::string16> query_words;
  QueryParser parser;
  if (query.word_phrase_query) {
    parser.ParseQueryWords(base::i18n::ToLower(*query.word_phrase_query),
                           &query_words);
    if (query_words.empty())
      return;
  }

  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if ((!query_words.empty() &&
        !DoesBookmarkContainWords(node, query_words, languages)) ||
        model->is_permanent_node(node)) {
      continue;
    }
    if (query.url) {
      // Check against bare url spec and IDN-decoded url.
      if (!node->is_url() ||
          !(base::UTF8ToUTF16(node->url().spec()) == *query.url ||
            net::FormatUrl(
                node->url(), languages, net::kFormatUrlOmitNothing,
                net::UnescapeRule::NORMAL, NULL, NULL, NULL) == *query.url)) {
        continue;
      }
    }
    if (query.title && node->GetTitle() != *query.title)
      continue;

    nodes->push_back(node);
    if (nodes->size() == max_count)
      return;
  }
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kShowBookmarkBar,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEditBookmarksEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kShowAppsShortcutInBookmarkBar,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

const BookmarkNode* GetParentForNewNodes(
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& selection,
    int* index) {
  const BookmarkNode* real_parent = parent;

  if (selection.size() == 1 && selection[0]->is_folder())
    real_parent = selection[0];

  if (index) {
    if (selection.size() == 1 && selection[0]->is_url()) {
      *index = real_parent->GetIndexOf(selection[0]) + 1;
      if (*index == 0) {
        // Node doesn't exist in parent, add to end.
        NOTREACHED();
        *index = real_parent->child_count();
      }
    } else {
      *index = real_parent->child_count();
    }
  }

  return real_parent;
}

void DeleteBookmarkFolders(BookmarkModel* model,
                           const std::vector<int64>& ids) {
  // Remove the folders that were removed. This has to be done after all the
  // other changes have been committed.
  for (std::vector<int64>::const_iterator iter = ids.begin();
       iter != ids.end();
       ++iter) {
    const BookmarkNode* node = model->GetNodeByID(*iter);
    if (!node)
      continue;
    const BookmarkNode* parent = node->parent();
    model->Remove(parent, parent->GetIndexOf(node));
  }
}

void AddIfNotBookmarked(BookmarkModel* model,
                        const GURL& url,
                        const base::string16& title) {
  std::vector<const BookmarkNode*> bookmarks;
  model->GetNodesByURL(url, &bookmarks);
  if (!bookmarks.empty())
    return;  // Nothing to do, a bookmark with that url already exists.

  content::RecordAction(base::UserMetricsAction("BookmarkAdded"));
  const BookmarkNode* parent = model->GetParentForNewNodes();
  model->AddURL(parent, parent->child_count(), title, url);
}

void RemoveAllBookmarks(BookmarkModel* model, const GURL& url) {
  std::vector<const BookmarkNode*> bookmarks;
  model->GetNodesByURL(url, &bookmarks);

  // Remove all the bookmarks.
  for (size_t i = 0; i < bookmarks.size(); ++i) {
    const BookmarkNode* node = bookmarks[i];
    int index = node->parent()->GetIndexOf(node);
    if (index > -1)
      model->Remove(node->parent(), index);
  }
}

}  // namespace bookmark_utils
