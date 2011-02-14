// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include <utility>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#if defined(OS_MACOSX)
#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"
#endif
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/history/query_parser.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "grit/app_strings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/tree_node_iterator.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/base/dragdrop/os_exchange_data.h"
#include "views/drag_utils.h"
#include "views/events/event.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/custom_drag.h"
#endif

using base::Time;

namespace {

// A PageNavigator implementation that creates a new Browser. This is used when
// opening a url and there is no Browser open. The Browser is created the first
// time the PageNavigator method is invoked.
class NewBrowserPageNavigator : public PageNavigator {
 public:
  explicit NewBrowserPageNavigator(Profile* profile)
      : profile_(profile),
        browser_(NULL) {}

  virtual ~NewBrowserPageNavigator() {
    if (browser_)
      browser_->window()->Show();
  }

  Browser* browser() const { return browser_; }

  virtual void OpenURL(const GURL& url,
                       const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) {
    if (!browser_) {
      Profile* profile = (disposition == OFF_THE_RECORD) ?
          profile_->GetOffTheRecordProfile() : profile_;
      browser_ = Browser::Create(profile);
      // Always open the first tab in the foreground.
      disposition = NEW_FOREGROUND_TAB;
    }
    browser_->OpenURL(url, referrer, NEW_FOREGROUND_TAB, transition);
  }

 private:
  Profile* profile_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(NewBrowserPageNavigator);
};

void CloneBookmarkNodeImpl(BookmarkModel* model,
                           const BookmarkNodeData::Element& element,
                           const BookmarkNode* parent,
                           int index_to_add_at) {
  if (element.is_url) {
    model->AddURL(parent, index_to_add_at, element.title, element.url);
  } else {
    const BookmarkNode* new_folder = model->AddGroup(parent,
                                                     index_to_add_at,
                                                     element.title);
    for (int i = 0; i < static_cast<int>(element.children.size()); ++i)
      CloneBookmarkNodeImpl(model, element.children[i], new_folder, i);
  }
}

// Returns the number of descendants of node that are of type url.
int DescendantURLCount(const BookmarkNode* node) {
  int result = 0;
  for (int i = 0; i < node->GetChildCount(); ++i) {
    const BookmarkNode* child = node->GetChild(i);
    if (child->is_url())
      result++;
    else
      result += DescendantURLCount(child);
  }
  return result;
}

// Implementation of OpenAll. Opens all nodes of type URL and recurses for
// groups. |navigator| is the PageNavigator used to open URLs. After the first
// url is opened |opened_url| is set to true and |navigator| is set to the
// PageNavigator of the last active tab. This is done to handle a window
// disposition of new window, in which case we want subsequent tabs to open in
// that window.
void OpenAllImpl(const BookmarkNode* node,
                 WindowOpenDisposition initial_disposition,
                 PageNavigator** navigator,
                 bool* opened_url) {
  if (node->is_url()) {
    WindowOpenDisposition disposition;
    if (*opened_url)
      disposition = NEW_BACKGROUND_TAB;
    else
      disposition = initial_disposition;
    (*navigator)->OpenURL(node->GetURL(), GURL(), disposition,
                          PageTransition::AUTO_BOOKMARK);
    if (!*opened_url) {
      *opened_url = true;
      // We opened the first URL which may have opened a new window or clobbered
      // the current page, reset the navigator just to be sure.
      Browser* new_browser = BrowserList::GetLastActive();
      if (new_browser) {
        TabContents* current_tab = new_browser->GetSelectedTabContents();
        DCHECK(new_browser && current_tab);
        if (new_browser && current_tab)
          *navigator = current_tab;
      }  // else, new_browser == NULL, which happens during testing.
    }
  } else {
    // Group, recurse through children.
    for (int i = 0; i < node->GetChildCount(); ++i) {
      OpenAllImpl(node->GetChild(i), initial_disposition, navigator,
                  opened_url);
    }
  }
}

bool ShouldOpenAll(gfx::NativeWindow parent,
                   const std::vector<const BookmarkNode*>& nodes) {
  int descendant_count = 0;
  for (size_t i = 0; i < nodes.size(); ++i)
    descendant_count += DescendantURLCount(nodes[i]);
  if (descendant_count < bookmark_utils::num_urls_before_prompting)
    return true;

  string16 message = l10n_util::GetStringFUTF16(
      IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL,
      base::IntToString16(descendant_count));
  string16 title = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  return platform_util::SimpleYesNoBox(parent, title, message);
}

// Comparison function that compares based on date modified of the two nodes.
bool MoreRecentlyModified(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_group_modified() > n2->date_group_modified();
}

// Returns true if |text| contains each string in |words|. This is used when
// searching for bookmarks.
bool DoesBookmarkTextContainWords(const string16& text,
                                  const std::vector<string16>& words) {
  for (size_t i = 0; i < words.size(); ++i) {
    if (text.find(words[i]) == string16::npos)
      return false;
  }
  return true;
}

// Returns true if |node|s title or url contains the strings in |words|.
// |languages| argument is user's accept-language setting to decode IDN.
bool DoesBookmarkContainWords(const BookmarkNode* node,
                              const std::vector<string16>& words,
                              const std::string& languages) {
  return
      DoesBookmarkTextContainWords(
          l10n_util::ToLower(node->GetTitle()), words) ||
      DoesBookmarkTextContainWords(
          l10n_util::ToLower(UTF8ToUTF16(node->GetURL().spec())), words) ||
      DoesBookmarkTextContainWords(l10n_util::ToLower(
          net::FormatUrl(node->GetURL(), languages, net::kFormatUrlOmitNothing,
                         UnescapeRule::NORMAL, NULL, NULL, NULL)), words);
}

}  // namespace

namespace bookmark_utils {

int num_urls_before_prompting = 15;

int PreferredDropOperation(int source_operations, int operations) {
  int common_ops = (source_operations & operations);
  if (!common_ops)
    return 0;
  if (ui::DragDropTypes::DRAG_COPY & common_ops)
    return ui::DragDropTypes::DRAG_COPY;
  if (ui::DragDropTypes::DRAG_LINK & common_ops)
    return ui::DragDropTypes::DRAG_LINK;
  if (ui::DragDropTypes::DRAG_MOVE & common_ops)
    return ui::DragDropTypes::DRAG_MOVE;
  return ui::DragDropTypes::DRAG_NONE;
}

int BookmarkDragOperation(const BookmarkNode* node) {
  if (node->is_url()) {
    return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE |
           ui::DragDropTypes::DRAG_LINK;
  }
  return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;
}

#if defined(TOOLKIT_VIEWS)
int BookmarkDropOperation(Profile* profile,
                          const views::DropTargetEvent& event,
                          const BookmarkNodeData& data,
                          const BookmarkNode* parent,
                          int index) {
  if (data.IsFromProfile(profile) && data.size() > 1)
    // Currently only accept one dragged node at a time.
    return ui::DragDropTypes::DRAG_NONE;

  if (!bookmark_utils::IsValidDropLocation(profile, data, parent, index))
    return ui::DragDropTypes::DRAG_NONE;

  if (data.GetFirstNode(profile)) {
    // User is dragging from this profile: move.
    return ui::DragDropTypes::DRAG_MOVE;
  }
  // User is dragging from another app, copy.
  return PreferredDropOperation(event.source_operations(),
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK);
}
#endif  // defined(TOOLKIT_VIEWS)

int PerformBookmarkDrop(Profile* profile,
                        const BookmarkNodeData& data,
                        const BookmarkNode* parent_node,
                        int index) {
  BookmarkModel* model = profile->GetBookmarkModel();
  if (data.IsFromProfile(profile)) {
    const std::vector<const BookmarkNode*> dragged_nodes =
        data.GetNodes(profile);
    if (!dragged_nodes.empty()) {
      // Drag from same profile. Move nodes.
      for (size_t i = 0; i < dragged_nodes.size(); ++i) {
        model->Move(dragged_nodes[i], parent_node, index);
        index = parent_node->IndexOfChild(dragged_nodes[i]) + 1;
      }
      return ui::DragDropTypes::DRAG_MOVE;
    }
    return ui::DragDropTypes::DRAG_NONE;
  }
  // Dropping a group from different profile. Always accept.
  bookmark_utils::CloneBookmarkNode(model, data.elements, parent_node, index);
  return ui::DragDropTypes::DRAG_COPY;
}

bool IsValidDropLocation(Profile* profile,
                         const BookmarkNodeData& data,
                         const BookmarkNode* drop_parent,
                         int index) {
  if (!drop_parent->is_folder()) {
    NOTREACHED();
    return false;
  }

  if (!data.is_valid())
    return false;

  if (data.IsFromProfile(profile)) {
    std::vector<const BookmarkNode*> nodes = data.GetNodes(profile);
    for (size_t i = 0; i < nodes.size(); ++i) {
      // Don't allow the drop if the user is attempting to drop on one of the
      // nodes being dragged.
      const BookmarkNode* node = nodes[i];
      int node_index = (drop_parent == node->GetParent()) ?
          drop_parent->IndexOfChild(nodes[i]) : -1;
      if (node_index != -1 && (index == node_index || index == node_index + 1))
        return false;

      // drop_parent can't accept a child that is an ancestor.
      if (drop_parent->HasAncestor(node))
        return false;
    }
    return true;
  }
  // From the same profile, always accept.
  return true;
}

void CloneBookmarkNode(BookmarkModel* model,
                       const std::vector<BookmarkNodeData::Element>& elements,
                       const BookmarkNode* parent,
                       int index_to_add_at) {
  if (!parent->is_folder() || !model) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < elements.size(); ++i)
    CloneBookmarkNodeImpl(model, elements[i], parent, index_to_add_at + i);
}


// Bookmark dragging
void DragBookmarks(Profile* profile,
                   const std::vector<const BookmarkNode*>& nodes,
                   gfx::NativeView view) {
  DCHECK(!nodes.empty());

#if defined(TOOLKIT_VIEWS)
  // Set up our OLE machinery
  ui::OSExchangeData data;
  BookmarkNodeData drag_data(nodes);
  drag_data.Write(profile, &data);

  views::RootView* root_view =
      views::Widget::GetWidgetFromNativeView(view)->GetRootView();

  // Allow nested message loop so we get DnD events as we drag this around.
  bool was_nested = MessageLoop::current()->IsNested();
  MessageLoop::current()->SetNestableTasksAllowed(true);

  root_view->StartDragForViewFromMouseEvent(NULL, data,
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE |
      ui::DragDropTypes::DRAG_LINK);

  MessageLoop::current()->SetNestableTasksAllowed(was_nested);
#elif defined(OS_MACOSX)
  // Allow nested message loop so we get DnD events as we drag this around.
  bool was_nested = MessageLoop::current()->IsNested();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  bookmark_pasteboard_helper_mac::StartDrag(profile, nodes, view);
  MessageLoop::current()->SetNestableTasksAllowed(was_nested);
#elif defined(TOOLKIT_GTK)
  BookmarkDrag::BeginDrag(profile, nodes);
#endif
}

void OpenAll(gfx::NativeWindow parent,
             Profile* profile,
             PageNavigator* navigator,
             const std::vector<const BookmarkNode*>& nodes,
             WindowOpenDisposition initial_disposition) {
  if (!ShouldOpenAll(parent, nodes))
    return;

  NewBrowserPageNavigator navigator_impl(profile);
  if (!navigator) {
    Browser* browser =
        BrowserList::FindBrowserWithType(profile, Browser::TYPE_NORMAL, false);
    if (!browser || !browser->GetSelectedTabContents()) {
      navigator = &navigator_impl;
    } else {
      if (initial_disposition != NEW_WINDOW &&
          initial_disposition != OFF_THE_RECORD) {
        browser->window()->Activate();
      }
      navigator = browser->GetSelectedTabContents();
    }
  }

  bool opened_url = false;
  for (size_t i = 0; i < nodes.size(); ++i)
    OpenAllImpl(nodes[i], initial_disposition, &navigator, &opened_url);
}

void OpenAll(gfx::NativeWindow parent,
             Profile* profile,
             PageNavigator* navigator,
             const BookmarkNode* node,
             WindowOpenDisposition initial_disposition) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  OpenAll(parent, profile, navigator, nodes, initial_disposition);
}

void CopyToClipboard(BookmarkModel* model,
                     const std::vector<const BookmarkNode*>& nodes,
                     bool remove_nodes) {
  if (nodes.empty())
    return;

  BookmarkNodeData(nodes).WriteToClipboard(NULL);

  if (remove_nodes) {
    for (size_t i = 0; i < nodes.size(); ++i) {
      model->Remove(nodes[i]->GetParent(),
                    nodes[i]->GetParent()->IndexOfChild(nodes[i]));
    }
  }
}

void PasteFromClipboard(BookmarkModel* model,
                        const BookmarkNode* parent,
                        int index) {
  if (!parent)
    return;

  BookmarkNodeData bookmark_data;
  if (!bookmark_data.ReadFromClipboard())
    return;

  if (index == -1)
    index = parent->GetChildCount();
  bookmark_utils::CloneBookmarkNode(
      model, bookmark_data.elements, parent, index);
}

bool CanPasteFromClipboard(const BookmarkNode* node) {
  if (!node)
    return false;
  return BookmarkNodeData::ClipboardContainsBookmarks();
}

string16 GetNameForURL(const GURL& url) {
  if (url.is_valid()) {
    return net::GetSuggestedFilename(url, "", "", string16());
  } else {
    return l10n_util::GetStringUTF16(IDS_APP_UNTITLED_SHORTCUT_FILE_NAME);
  }
}

std::vector<const BookmarkNode*> GetMostRecentlyModifiedGroups(
    BookmarkModel* model,
    size_t max_count) {
  std::vector<const BookmarkNode*> nodes;
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* parent = iterator.Next();
    if (parent->is_folder() && parent->date_group_modified() > base::Time()) {
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
    // Add the bookmark bar and other nodes if there is space.
    if (find(nodes.begin(), nodes.end(), model->GetBookmarkBarNode()) ==
        nodes.end()) {
      nodes.push_back(model->GetBookmarkBarNode());
    }

    if (nodes.size() < max_count &&
        find(nodes.begin(), nodes.end(), model->other_node()) == nodes.end()) {
      nodes.push_back(model->other_node());
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

TitleMatch::TitleMatch()
    : node(NULL) {
}

TitleMatch::~TitleMatch() {}

bool MoreRecentlyAdded(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_added() > n2->date_added();
}

void GetBookmarksContainingText(BookmarkModel* model,
                                const string16& text,
                                size_t max_count,
                                const std::string& languages,
                                std::vector<const BookmarkNode*>* nodes) {
  std::vector<string16> words;
  QueryParser parser;
  parser.ExtractQueryWords(l10n_util::ToLower(text), &words);
  if (words.empty())
    return;

  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->is_url() && DoesBookmarkContainWords(node, words, languages)) {
      nodes->push_back(node);
      if (nodes->size() == max_count)
        return;
    }
  }
}

bool DoesBookmarkContainText(const BookmarkNode* node,
                             const string16& text,
                             const std::string& languages) {
  std::vector<string16> words;
  QueryParser parser;
  parser.ExtractQueryWords(l10n_util::ToLower(text), &words);
  if (words.empty())
    return false;

  return (node->is_url() && DoesBookmarkContainWords(node, words, languages));
}

static const BookmarkNode* CreateNewNode(BookmarkModel* model,
    const BookmarkNode* parent, const BookmarkEditor::EditDetails& details,
    const string16& new_title, const GURL& new_url) {
  const BookmarkNode* node;
  if (details.type == BookmarkEditor::EditDetails::NEW_URL) {
    node = model->AddURL(parent, parent->GetChildCount(), new_title, new_url);
  } else if (details.type == BookmarkEditor::EditDetails::NEW_FOLDER) {
    node = model->AddGroup(parent, parent->GetChildCount(), new_title);
    for (size_t i = 0; i < details.urls.size(); ++i) {
      model->AddURL(node, node->GetChildCount(), details.urls[i].second,
                    details.urls[i].first);
    }
    model->SetDateGroupModified(parent, Time::Now());
  } else {
    NOTREACHED();
    return NULL;
  }

  return node;
}

const BookmarkNode* ApplyEditsWithNoGroupChange(BookmarkModel* model,
    const BookmarkNode* parent, const BookmarkEditor::EditDetails& details,
    const string16& new_title, const GURL& new_url) {
  if (details.type == BookmarkEditor::EditDetails::NEW_URL ||
      details.type == BookmarkEditor::EditDetails::NEW_FOLDER) {
    return CreateNewNode(model, parent, details, new_title, new_url);
  }

  const BookmarkNode* node = details.existing_node;
  DCHECK(node);

  if (node->is_url())
    model->SetURL(node, new_url);
  model->SetTitle(node, new_title);

  return node;
}

const BookmarkNode* ApplyEditsWithPossibleGroupChange(BookmarkModel* model,
    const BookmarkNode* new_parent, const BookmarkEditor::EditDetails& details,
    const string16& new_title, const GURL& new_url) {
  if (details.type == BookmarkEditor::EditDetails::NEW_URL ||
      details.type == BookmarkEditor::EditDetails::NEW_FOLDER) {
    return CreateNewNode(model, new_parent, details, new_title, new_url);
  }

  const BookmarkNode* node = details.existing_node;
  DCHECK(node);

  if (new_parent != node->GetParent())
    model->Move(node, new_parent, new_parent->GetChildCount());
  if (node->is_url())
    model->SetURL(node, new_url);
  model->SetTitle(node, new_title);

  return node;
}

// Formerly in BookmarkBarView
void ToggleWhenVisible(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  const bool always_show = !prefs->GetBoolean(prefs::kShowBookmarkBar);

  // The user changed when the bookmark bar is shown, update the preferences.
  prefs->SetBoolean(prefs::kShowBookmarkBar, always_show);
  prefs->ScheduleSavePersistentPrefs();

  // And notify the notification service.
  Source<Profile> source(profile);
  NotificationService::current()->Notify(
      NotificationType::BOOKMARK_BAR_VISIBILITY_PREF_CHANGED,
      source,
      NotificationService::NoDetails());
}

void RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kShowBookmarkBar, false);
}

void GetURLAndTitleToBookmark(TabContents* tab_contents,
                              GURL* url,
                              string16* title) {
  *url = tab_contents->GetURL();
  *title = tab_contents->GetTitle();
}

void GetURLsForOpenTabs(Browser* browser,
    std::vector<std::pair<GURL, string16> >* urls) {
  for (int i = 0; i < browser->tab_count(); ++i) {
    std::pair<GURL, string16> entry;
    GetURLAndTitleToBookmark(browser->GetTabContentsAt(i), &(entry.first),
                             &(entry.second));
    urls->push_back(entry);
  }
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
      *index = real_parent->IndexOfChild(selection[0]) + 1;
      if (*index == 0) {
        // Node doesn't exist in parent, add to end.
        NOTREACHED();
        *index = real_parent->GetChildCount();
      }
    } else {
      *index = real_parent->GetChildCount();
    }
  }

  return real_parent;
}

bool NodeHasURLs(const BookmarkNode* node) {
  DCHECK(node);

  if (node->is_url())
    return true;

  for (int i = 0; i < node->GetChildCount(); ++i) {
    if (NodeHasURLs(node->GetChild(i)))
      return true;
  }
  return false;
}

}  // namespace bookmark_utils
