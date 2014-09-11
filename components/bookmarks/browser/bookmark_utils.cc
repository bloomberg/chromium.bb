// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_utils.h"

#include <utility>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/metrics/user_metrics_action.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/query_parser/query_parser.h"
#include "net/base/net_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/models/tree_node_iterator.h"
#include "url/gurl.h"

using base::Time;

namespace bookmarks {

namespace {

// The maximum length of URL or title returned by the Cleanup functions.
const size_t kCleanedUpUrlMaxLength = 1024u;
const size_t kCleanedUpTitleMaxLength = 1024u;

void CloneBookmarkNodeImpl(BookmarkModel* model,
                           const BookmarkNodeData::Element& element,
                           const BookmarkNode* parent,
                           int index_to_add_at,
                           bool reset_node_times) {
  if (element.is_url) {
    Time date_added = reset_node_times ? Time::Now() : element.date_added;
    DCHECK(!date_added.is_null());

    model->AddURLWithCreationTimeAndMetaInfo(parent,
                                             index_to_add_at,
                                             element.title,
                                             element.url,
                                             date_added,
                                             &element.meta_info_map);
  } else {
    const BookmarkNode* cloned_node = model->AddFolderWithMetaInfo(
        parent, index_to_add_at, element.title, &element.meta_info_map);
    if (!reset_node_times) {
      DCHECK(!element.date_folder_modified.is_null());
      model->SetDateFolderModified(cloned_node, element.date_folder_modified);
    }
    for (int i = 0; i < static_cast<int>(element.children.size()); ++i)
      CloneBookmarkNodeImpl(model, element.children[i], cloned_node, i,
                            reset_node_times);
  }
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

const BookmarkNode* GetNodeByID(const BookmarkNode* node, int64 id) {
  if (node->id() == id)
    return node;

  for (int i = 0, child_count = node->child_count(); i < child_count; ++i) {
    const BookmarkNode* result = GetNodeByID(node->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

// Attempts to shorten a URL safely (i.e., by preventing the end of the URL
// from being in the middle of an escape sequence) to no more than
// kCleanedUpUrlMaxLength characters, returning the result.
std::string TruncateUrl(const std::string& url) {
  if (url.length() <= kCleanedUpUrlMaxLength)
    return url;

  // If we're in the middle of an escape sequence, truncate just before it.
  if (url[kCleanedUpUrlMaxLength - 1] == '%')
    return url.substr(0, kCleanedUpUrlMaxLength - 1);
  if (url[kCleanedUpUrlMaxLength - 2] == '%')
    return url.substr(0, kCleanedUpUrlMaxLength - 2);

  return url.substr(0, kCleanedUpUrlMaxLength);
}

// Returns the URL from the clipboard. If there is no URL an empty URL is
// returned.
GURL GetUrlFromClipboard() {
  base::string16 url_text;
#if !defined(OS_IOS)
  ui::Clipboard::GetForCurrentThread()->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE,
                                                 &url_text);
#endif
  return GURL(url_text);
}

}  // namespace

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
    CloneBookmarkNodeImpl(model, elements[i], parent,
                          index_to_add_at + static_cast<int>(i),
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
    ScopedGroupBookmarkActions group_cut(model);
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
  if (!bookmark_data.ReadFromClipboard(ui::CLIPBOARD_TYPE_COPY_PASTE)) {
    GURL url = GetUrlFromClipboard();
    if (!url.is_valid())
      return;
    BookmarkNode node(url);
    node.SetTitle(base::ASCIIToUTF16(url.spec()));
    bookmark_data = BookmarkNodeData(&node);
  }
  if (index == -1)
    index = parent->child_count();
  ScopedGroupBookmarkActions group_paste(model);
  CloneBookmarkNode(model, bookmark_data.elements, parent, index, true);
}

bool CanPasteFromClipboard(BookmarkModel* model, const BookmarkNode* node) {
  if (!node || !model->client()->CanBeEditedByUser(node))
    return false;
  return (BookmarkNodeData::ClipboardContainsBookmarks() ||
          GetUrlFromClipboard().is_valid());
}

std::vector<const BookmarkNode*> GetMostRecentlyModifiedUserFolders(
    BookmarkModel* model,
    size_t max_count) {
  std::vector<const BookmarkNode*> nodes;
  ui::TreeNodeIterator<const BookmarkNode> iterator(
      model->root_node(), base::Bind(&PruneInvisibleFolders));

  while (iterator.has_next()) {
    const BookmarkNode* parent = iterator.Next();
    if (!model->client()->CanBeEditedByUser(parent))
      continue;
    if (parent->is_folder() && parent->date_folder_modified() > Time()) {
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
      if (node->IsVisible() && model->client()->CanBeEditedByUser(node) &&
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
  query_parser::QueryParser parser;
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
  registry->RegisterBooleanPref(
      prefs::kShowManagedBookmarksInBookmarkBar,
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
    const BookmarkNode* node = GetBookmarkNodeByID(model, *iter);
    if (!node)
      continue;
    const BookmarkNode* parent = node->parent();
    model->Remove(parent, parent->GetIndexOf(node));
  }
}

void AddIfNotBookmarked(BookmarkModel* model,
                        const GURL& url,
                        const base::string16& title) {
  if (IsBookmarkedByUser(model, url))
    return;  // Nothing to do, a user bookmark with that url already exists.
  model->client()->RecordAction(base::UserMetricsAction("BookmarkAdded"));
  const BookmarkNode* parent = model->GetParentForNewNodes();
  model->AddURL(parent, parent->child_count(), title, url);
}

void RemoveAllBookmarks(BookmarkModel* model, const GURL& url) {
  std::vector<const BookmarkNode*> bookmarks;
  model->GetNodesByURL(url, &bookmarks);

  // Remove all the user bookmarks.
  for (size_t i = 0; i < bookmarks.size(); ++i) {
    const BookmarkNode* node = bookmarks[i];
    int index = node->parent()->GetIndexOf(node);
    if (index > -1 && model->client()->CanBeEditedByUser(node))
      model->Remove(node->parent(), index);
  }
}

base::string16 CleanUpUrlForMatching(
    const GURL& gurl,
    const std::string& languages,
    base::OffsetAdjuster::Adjustments* adjustments) {
  base::OffsetAdjuster::Adjustments tmp_adjustments;
  return base::i18n::ToLower(net::FormatUrlWithAdjustments(
      GURL(TruncateUrl(gurl.spec())), languages,
      net::kFormatUrlOmitUsernamePassword,
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS,
      NULL, NULL, adjustments ? adjustments : &tmp_adjustments));
}

base::string16 CleanUpTitleForMatching(const base::string16& title) {
  return base::i18n::ToLower(title.substr(0u, kCleanedUpTitleMaxLength));
}

bool CanAllBeEditedByUser(BookmarkClient* client,
                          const std::vector<const BookmarkNode*>& nodes) {
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (!client->CanBeEditedByUser(nodes[i]))
      return false;
  }
  return true;
}

bool IsBookmarkedByUser(BookmarkModel* model, const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  model->GetNodesByURL(url, &nodes);
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (model->client()->CanBeEditedByUser(nodes[i]))
      return true;
  }
  return false;
}

const BookmarkNode* GetBookmarkNodeByID(const BookmarkModel* model, int64 id) {
  // TODO(sky): TreeNode needs a method that visits all nodes using a predicate.
  return GetNodeByID(model->root_node(), id);
}

}  // namespace bookmarks
