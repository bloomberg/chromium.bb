// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include <utility>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/metrics/histogram.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/history/query_parser.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/simple_message_box.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/page_navigator.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/ui_strings.h"
#include "net/base/net_util.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/tree_node_iterator.h"

#if defined(OS_MACOSX)
#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "views/drag_utils.h"
#endif

#if defined(TOOLKIT_GTK)
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

  // Deprecated. Please use one-argument variant.
  // TODO(adriansc): Remove this method once refactoring changed all call sites.
  virtual TabContents* OpenURL(const GURL& url,
                               const GURL& referrer,
                               WindowOpenDisposition disposition,
                               content::PageTransition transition) OVERRIDE {
    return OpenURL(OpenURLParams(url, referrer, disposition, transition,
                                 false));
  }

  virtual TabContents* OpenURL(const OpenURLParams& params) OVERRIDE {
    if (!browser_) {
      Profile* profile = (params.disposition == OFF_THE_RECORD) ?
          profile_->GetOffTheRecordProfile() : profile_;
      browser_ = Browser::Create(profile);
    }

    OpenURLParams forward_params = params;
    forward_params.disposition = NEW_FOREGROUND_TAB;
    return browser_->OpenURL(forward_params);
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
    const BookmarkNode* new_folder = model->AddFolder(parent,
                                                      index_to_add_at,
                                                      element.title);
    for (int i = 0; i < static_cast<int>(element.children.size()); ++i)
      CloneBookmarkNodeImpl(model, element.children[i], new_folder, i);
  }
}

// Returns the number of children of |node| that are of type url.
int ChildURLCount(const BookmarkNode* node) {
  int result = 0;
  for (int i = 0; i < node->child_count(); ++i) {
    const BookmarkNode* child = node->GetChild(i);
    if (child->is_url())
      result++;
  }
  return result;
}

// Returns the total number of descendants nodes.
int ChildURLCountTotal(const BookmarkNode* node) {
  int result = 0;
  for (int i = 0; i < node->child_count(); ++i) {
    const BookmarkNode* child = node->GetChild(i);
    result++;
    if (child->is_folder())
      result += ChildURLCountTotal(child);
  }
  return result;
}

// Implementation of OpenAll. Opens all nodes of type URL and any children of
// |node| that are of type URL. |navigator| is the PageNavigator used to open
// URLs. After the first url is opened |opened_url| is set to true and
// |navigator| is set to the PageNavigator of the last active tab. This is done
// to handle a window disposition of new window, in which case we want
// subsequent tabs to open in that window.
void OpenAllImpl(const BookmarkNode* node,
                 WindowOpenDisposition initial_disposition,
                 PageNavigator** navigator,
                 Profile* profile,
                 bool* opened_url) {
  if (node->is_url()) {
    WindowOpenDisposition disposition;
    if (*opened_url)
      disposition = NEW_BACKGROUND_TAB;
    else
      disposition = initial_disposition;
    (*navigator)->OpenURL(OpenURLParams(node->url(), GURL(), disposition,
                          content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
    if (!*opened_url) {
      *opened_url = true;
      // We opened the first URL which may have opened a new window or clobbered
      // the current page, reset the navigator just to be sure.
      Browser* new_browser = BrowserList::GetLastActiveWithProfile(profile);
      if (new_browser) {
        TabContents* current_tab = new_browser->GetSelectedTabContents();
        DCHECK(new_browser && current_tab);
        if (new_browser && current_tab)
          *navigator = current_tab;
      }  // else, new_browser == NULL, which happens during testing.
    }
  } else {
    // For folders only open direct children.
    for (int i = 0; i < node->child_count(); ++i) {
      const BookmarkNode* child_node = node->GetChild(i);
      if (child_node->is_url())
        OpenAllImpl(child_node, initial_disposition, navigator, profile,
                    opened_url);
    }
  }
}

bool ShouldOpenAll(gfx::NativeWindow parent,
                   const std::vector<const BookmarkNode*>& nodes) {
  int child_count = 0;
  for (size_t i = 0; i < nodes.size(); ++i)
    child_count += ChildURLCount(nodes[i]);
  if (child_count < bookmark_utils::num_urls_before_prompting)
    return true;

  string16 message = l10n_util::GetStringFUTF16(
      IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL,
      base::IntToString16(child_count));
  string16 title = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  return browser::ShowYesNoBox(parent, title, message);
}

// Comparison function that compares based on date modified of the two nodes.
bool MoreRecentlyModified(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_folder_modified() > n2->date_folder_modified();
}

// Returns true if |text| contains each string in |words|. This is used when
// searching for bookmarks.
bool DoesBookmarkTextContainWords(const string16& text,
                                  const std::vector<string16>& words) {
  for (size_t i = 0; i < words.size(); ++i) {
    if (!base::i18n::StringSearchIgnoringCaseAndAccents(words[i], text))
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
      DoesBookmarkTextContainWords(node->GetTitle(), words) ||
      DoesBookmarkTextContainWords(UTF8ToUTF16(node->url().spec()), words) ||
      DoesBookmarkTextContainWords(net::FormatUrl(
          node->url(), languages, net::kFormatUrlOmitNothing,
          net::UnescapeRule::NORMAL, NULL, NULL, NULL), words);
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

int BookmarkDragOperation(Profile* profile, const BookmarkNode* node) {
  int move = ui::DragDropTypes::DRAG_MOVE;
  if (!profile->GetPrefs()->GetBoolean(prefs::kEditBookmarksEnabled))
    move = 0;
  if (node->is_url()) {
    return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK | move;
  }
  return ui::DragDropTypes::DRAG_COPY | move;
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
        index = parent_node->GetIndexOf(dragged_nodes[i]) + 1;
      }
      return ui::DragDropTypes::DRAG_MOVE;
    }
    return ui::DragDropTypes::DRAG_NONE;
  }
  // Dropping a folder from different profile. Always accept.
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
      int node_index = (drop_parent == node->parent()) ?
          drop_parent->GetIndexOf(nodes[i]) : -1;
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

  // Allow nested message loop so we get DnD events as we drag this around.
  bool was_nested = MessageLoop::current()->IsNested();
  MessageLoop::current()->SetNestableTasksAllowed(true);

  views::Widget* widget = views::Widget::GetWidgetForNativeView(view);
  if (widget) {
    widget->RunShellDrag(NULL, data,
        ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE |
        ui::DragDropTypes::DRAG_LINK);
  }

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
    Browser* browser = BrowserList::FindTabbedBrowser(profile, false);
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
  if (initial_disposition == OFF_THE_RECORD)
    profile = profile->GetOffTheRecordProfile();

  for (size_t i = 0; i < nodes.size(); ++i)
    OpenAllImpl(nodes[i], initial_disposition, &navigator, profile,
                &opened_url);
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
      int index = nodes[i]->parent()->GetIndexOf(nodes[i]);
      if (index > -1)
        model->Remove(nodes[i]->parent(), index);
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
    index = parent->child_count();
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
    return net::GetSuggestedFilename(url, "", "", "", "", std::string());
  } else {
    return l10n_util::GetStringUTF16(IDS_APP_UNTITLED_SHORTCUT_FILE_NAME);
  }
}

// This is used with a tree iterator to skip subtrees which are not visible.
static bool PruneInvisibleFolders(const BookmarkNode* node) {
  return !node->IsVisible();
}

std::vector<const BookmarkNode*> GetMostRecentlyModifiedFolders(
    BookmarkModel* model,
    size_t max_count) {
  std::vector<const BookmarkNode*> nodes;
  ui::TreeNodeIterator<const BookmarkNode>
      iterator(model->root_node(), PruneInvisibleFolders);

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
          find(nodes.begin(), nodes.end(), node) == nodes.end()) {
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
  parser.ParseQueryWords(base::i18n::ToLower(text), &words);
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
  parser.ParseQueryWords(base::i18n::ToLower(text), &words);
  if (words.empty())
    return false;

  return (node->is_url() && DoesBookmarkContainWords(node, words, languages));
}

static const BookmarkNode* CreateNewNode(BookmarkModel* model,
    const BookmarkNode* parent, const BookmarkEditor::EditDetails& details,
    const string16& new_title, const GURL& new_url) {
  const BookmarkNode* node;
  if (details.type == BookmarkEditor::EditDetails::NEW_URL) {
    node = model->AddURL(parent, parent->child_count(), new_title, new_url);
  } else if (details.type == BookmarkEditor::EditDetails::NEW_FOLDER) {
    node = model->AddFolder(parent, parent->child_count(), new_title);
    for (size_t i = 0; i < details.urls.size(); ++i) {
      model->AddURL(node, node->child_count(), details.urls[i].second,
                    details.urls[i].first);
    }
    model->SetDateFolderModified(parent, Time::Now());
  } else {
    NOTREACHED();
    return NULL;
  }

  return node;
}

const BookmarkNode* ApplyEditsWithNoFolderChange(BookmarkModel* model,
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

const BookmarkNode* ApplyEditsWithPossibleFolderChange(BookmarkModel* model,
    const BookmarkNode* new_parent, const BookmarkEditor::EditDetails& details,
    const string16& new_title, const GURL& new_url) {
  if (details.type == BookmarkEditor::EditDetails::NEW_URL ||
      details.type == BookmarkEditor::EditDetails::NEW_FOLDER) {
    return CreateNewNode(model, new_parent, details, new_title, new_url);
  }

  const BookmarkNode* node = details.existing_node;
  DCHECK(node);

  if (new_parent != node->parent())
    model->Move(node, new_parent, new_parent->child_count());
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
}

void RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kShowBookmarkBar,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kEditBookmarksEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
}

void GetURLAndTitleToBookmark(TabContents* tab_contents,
                              GURL* url,
                              string16* title) {
  *url = tab_contents->GetURL();
  *title = tab_contents->GetTitle();
}

void GetURLAndTitleToBookmarkFromCurrentTab(Profile* profile,
                                            GURL* url,
                                            string16* title) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  TabContents* tab_contents = browser ? browser->GetSelectedTabContents()
                                        : NULL;
  if (tab_contents)
    GetURLAndTitleToBookmark(tab_contents, url, title);
}

void GetURLsForOpenTabs(
    Browser* browser,
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

bool NodeHasURLs(const BookmarkNode* node) {
  DCHECK(node);

  if (node->is_url())
    return true;

  for (int i = 0; i < node->child_count(); ++i) {
    if (NodeHasURLs(node->GetChild(i)))
      return true;
  }
  return false;
}

bool ConfirmDeleteBookmarkNode(const BookmarkNode* node,
                               gfx::NativeWindow window) {
  DCHECK(node && node->is_folder() && !node->empty());
  return browser::ShowYesNoBox(window,
      l10n_util::GetStringUTF16(IDS_DELETE),
      l10n_util::GetStringFUTF16Int(IDS_BOOKMARK_EDITOR_CONFIRM_DELETE,
                                    ChildURLCountTotal(node)));
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
                        const string16& title) {
  std::vector<const BookmarkNode*> bookmarks;
  model->GetNodesByURL(url, &bookmarks);
  if (!bookmarks.empty())
    return;  // Nothing to do, a bookmark with that url already exists.

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

void RecordBookmarkLaunch(BookmarkLaunchLocation location) {
#if defined(OS_WIN)
  // TODO(estade): do this on other platforms too. For now it's compiled out
  // so that stats from platforms for which this is incompletely implemented
  // don't mix in with Windows, where it should be implemented exhaustively.
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.LaunchLocation", location, LAUNCH_LIMIT);
#endif
}

}  // namespace bookmark_utils
