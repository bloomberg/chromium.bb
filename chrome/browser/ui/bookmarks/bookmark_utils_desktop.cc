// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/url_formatter/url_formatter.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#endif

#if defined(OS_WIN)
#include "chrome/grit/theme_resources.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace chrome {

int num_bookmark_urls_before_prompting = 15;

namespace {

// Iterator that iterates through a set of BookmarkNodes returning the URLs
// for nodes that are urls, or the URLs for the children of non-url urls.
// This does not recurse through all descendants, only immediate children.
// The following illustrates
// typical usage:
// OpenURLIterator iterator(nodes);
// while (iterator.has_next()) {
//   const GURL* url = iterator.NextURL();
//   // do something with |urll|.
// }
class OpenURLIterator {
 public:
  explicit OpenURLIterator(const std::vector<const BookmarkNode*>& nodes)
      : child_index_(0),
        next_(NULL),
        parent_(nodes.begin()),
        end_(nodes.end()) {
    FindNext();
  }

  bool has_next() { return next_ != NULL;}

  const GURL* NextURL() {
    if (!has_next()) {
      NOTREACHED();
      return NULL;
    }

    const GURL* next = next_;
    FindNext();
    return next;
  }

 private:
  // Seach next node which has URL.
  void FindNext() {
    for (; parent_ < end_; ++parent_, child_index_ = 0) {
      if ((*parent_)->is_url()) {
        next_ = &(*parent_)->url();
        ++parent_;
        child_index_ = 0;
        return;
      } else {
        for (; child_index_ < (*parent_)->child_count(); ++child_index_) {
          const BookmarkNode* child = (*parent_)->GetChild(child_index_);
          if (child->is_url()) {
            next_ = &child->url();
            ++child_index_;
            return;
          }
        }
      }
    }
    next_ = NULL;
  }

  int child_index_;
  const GURL* next_;
  std::vector<const BookmarkNode*>::const_iterator parent_;
  const std::vector<const BookmarkNode*>::const_iterator end_;

  DISALLOW_COPY_AND_ASSIGN(OpenURLIterator);
};

#if !defined(OS_ANDROID) && !defined(OS_IOS)
bool ShouldOpenAll(gfx::NativeWindow parent,
                   const std::vector<const BookmarkNode*>& nodes) {
  int child_count = 0;
  OpenURLIterator iterator(nodes);
  while (iterator.has_next()) {
    iterator.NextURL();
    child_count++;
  }

  if (child_count < num_bookmark_urls_before_prompting)
    return true;

  return ShowMessageBox(parent,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringFUTF16(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL,
                                 base::IntToString16(child_count)),
      MESSAGE_BOX_TYPE_QUESTION) == MESSAGE_BOX_RESULT_YES;
}
#endif

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

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Returns in |urls|, the url and title pairs for each open tab in browser.
void GetURLsForOpenTabs(Browser* browser,
                        std::vector<std::pair<GURL, base::string16> >* urls) {
  for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
    std::pair<GURL, base::string16> entry;
    GetURLAndTitleToBookmark(browser->tab_strip_model()->GetWebContentsAt(i),
                             &(entry.first), &(entry.second));
    urls->push_back(entry);
  }
}
#endif

}  // namespace

#if !defined(OS_ANDROID) && !defined(OS_IOS)
void OpenAll(gfx::NativeWindow parent,
             content::PageNavigator* navigator,
             const std::vector<const BookmarkNode*>& nodes,
             WindowOpenDisposition initial_disposition,
             content::BrowserContext* browser_context) {
  if (!ShouldOpenAll(parent, nodes))
    return;

  // Opens all |nodes| of type URL and any children of |nodes| that are of type
  // URL. |navigator| is the PageNavigator used to open URLs. After the first
  // url is opened |opened_first_url| is set to true and |navigator| is set to
  // the PageNavigator of the last active tab. This is done to handle a window
  // disposition of new window, in which case we want subsequent tabs to open in
  // that window.
  bool opened_first_url = false;
  WindowOpenDisposition disposition = initial_disposition;
  OpenURLIterator iterator(nodes);
  while (iterator.has_next()) {
    const GURL* url = iterator.NextURL();
    // When |initial_disposition| is OFF_THE_RECORD, a node which can't be
    // opened in incognito window, it is detected using |browser_context|, is
    // not opened.
    if (initial_disposition == OFF_THE_RECORD &&
        !IsURLAllowedInIncognito(*url, browser_context))
      continue;

    content::WebContents* opened_tab = navigator->OpenURL(
        content::OpenURLParams(*url, content::Referrer(), disposition,
                               ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));

    if (!opened_first_url) {
      opened_first_url = true;
      disposition = NEW_BACKGROUND_TAB;
      // We opened the first URL which may have opened a new window or clobbered
      // the current page, reset the navigator just to be sure. |opened_tab| may
      // be NULL in tests.
      if (opened_tab)
        navigator = opened_tab;
    }
  }
}

void OpenAll(gfx::NativeWindow parent,
             content::PageNavigator* navigator,
             const BookmarkNode* node,
             WindowOpenDisposition initial_disposition,
             content::BrowserContext* browser_context) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  OpenAll(parent, navigator, nodes, initial_disposition, browser_context);
}

bool ConfirmDeleteBookmarkNode(const BookmarkNode* node,
                               gfx::NativeWindow window) {
  DCHECK(node && node->is_folder() && !node->empty());
  return ShowMessageBox(window,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringFUTF16Int(IDS_BOOKMARK_EDITOR_CONFIRM_DELETE,
                                    ChildURLCountTotal(node)),
      MESSAGE_BOX_TYPE_QUESTION) == MESSAGE_BOX_RESULT_YES;
}

void ShowBookmarkAllTabsDialog(Browser* browser) {
  Profile* profile = browser->profile();
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile);
  DCHECK(model && model->loaded());

  const BookmarkNode* parent = model->GetParentForNewNodes();
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(parent, parent->child_count());
  GetURLsForOpenTabs(browser, &(details.urls));
  DCHECK(!details.urls.empty());

  BookmarkEditor::Show(browser->window()->GetNativeWindow(), profile, details,
                       BookmarkEditor::SHOW_TREE);
}

bool HasBookmarkURLs(const std::vector<const BookmarkNode*>& selection) {
  OpenURLIterator iterator(selection);
  return iterator.has_next();
}

bool HasBookmarkURLsAllowedInIncognitoMode(
    const std::vector<const BookmarkNode*>& selection,
    content::BrowserContext* browser_context) {
  OpenURLIterator iterator(selection);
  while (iterator.has_next()) {
    const GURL* url = iterator.NextURL();
    if (IsURLAllowedInIncognito(*url, browser_context))
      return true;
  }
  return false;
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

}  // namespace chrome
