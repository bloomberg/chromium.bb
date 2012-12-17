// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_utils.h"

#include "base/logging.h"
#include "base/prefs/public/pref_service_base.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chrome {

namespace {

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

bool ShouldOpenAll(gfx::NativeWindow parent,
                   const std::vector<const BookmarkNode*>& nodes) {
  int child_count = 0;
  for (size_t i = 0; i < nodes.size(); ++i)
    child_count += ChildURLCount(nodes[i]);

  if (child_count < bookmark_utils::num_urls_before_prompting)
    return true;

  return ShowMessageBox(parent,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringFUTF16(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL,
                                 base::IntToString16(child_count)),
      MESSAGE_BOX_TYPE_QUESTION) == MESSAGE_BOX_RESULT_YES;
}

// Implementation of OpenAll. Opens all nodes of type URL and any children of
// |node| that are of type URL. |navigator| is the PageNavigator used to open
// URLs. After the first url is opened |opened_url| is set to true and
// |navigator| is set to the PageNavigator of the last active tab. This is done
// to handle a window disposition of new window, in which case we want
// subsequent tabs to open in that window.
void OpenAllImpl(const BookmarkNode* node,
                 WindowOpenDisposition initial_disposition,
                 content::PageNavigator** navigator,
                 bool* opened_url,
                 content::BrowserContext* browser_context) {
  if (node->is_url()) {
    // When |initial_disposition| is OFF_THE_RECORD, a node which can't be
    // opened in incognito window, it is detected using |browser_context|, is
    // not opened.
    if (initial_disposition == OFF_THE_RECORD &&
        !IsURLAllowedInIncognito(node->url(), browser_context))
        return;

    WindowOpenDisposition disposition;
    if (*opened_url)
      disposition = NEW_BACKGROUND_TAB;
    else
      disposition = initial_disposition;
    content::WebContents* opened_tab = (*navigator)->OpenURL(
        content::OpenURLParams(node->url(), content::Referrer(), disposition,
                               content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
    if (!*opened_url) {
      *opened_url = true;
      // We opened the first URL which may have opened a new window or clobbered
      // the current page, reset the navigator just to be sure. |opened_tab| may
      // be NULL in tests.
      if (opened_tab)
        *navigator = opened_tab;
    }
  } else {
    // For folders only open direct children.
    for (int i = 0; i < node->child_count(); ++i) {
      const BookmarkNode* child_node = node->GetChild(i);
      if (child_node->is_url())
        OpenAllImpl(child_node, initial_disposition, navigator, opened_url,
                    browser_context);
    }
  }
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

// Returns in |urls|, the url and title pairs for each open tab in browser.
void GetURLsForOpenTabs(Browser* browser,
                        std::vector<std::pair<GURL, string16> >* urls) {
  for (int i = 0; i < browser->tab_count(); ++i) {
    std::pair<GURL, string16> entry;
    GetURLAndTitleToBookmark(GetWebContentsAt(browser, i),
                             &(entry.first), &(entry.second));
    urls->push_back(entry);
  }
}

}  // namespace

void OpenAll(gfx::NativeWindow parent,
             content::PageNavigator* navigator,
             const std::vector<const BookmarkNode*>& nodes,
             WindowOpenDisposition initial_disposition,
             content::BrowserContext* browser_context) {
  if (!ShouldOpenAll(parent, nodes))
    return;

  bool opened_url = false;
  for (size_t i = 0; i < nodes.size(); ++i)
    OpenAllImpl(nodes[i], initial_disposition, &navigator, &opened_url,
                browser_context);
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
  DCHECK(model && model->IsLoaded());

  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(model->GetParentForNewNodes(), -1);
  GetURLsForOpenTabs(browser, &(details.urls));
  DCHECK(!details.urls.empty());

  BookmarkEditor::Show(browser->window()->GetNativeWindow(), profile, details,
                       BookmarkEditor::SHOW_TREE);
}

bool HasBookmarkURLs(const std::vector<const BookmarkNode*>& selection) {
  for (size_t i = 0; i < selection.size(); ++i) {
    if (NodeHasURLs(selection[i]))
      return true;
  }
  return false;
}

void GetURLAndTitleToBookmark(content::WebContents* web_contents,
                              GURL* url,
                              string16* title) {
  *url = web_contents->GetURL();
  *title = web_contents->GetTitle();
}

void ToggleBookmarkBarWhenVisible(content::BrowserContext* browser_context) {
  PrefServiceBase* prefs = PrefServiceBase::FromBrowserContext(browser_context);
  const bool always_show = !prefs->GetBoolean(prefs::kShowBookmarkBar);

  // The user changed when the bookmark bar is shown, update the preferences.
  prefs->SetBoolean(prefs::kShowBookmarkBar, always_show);
}

}  // namespace chrome
