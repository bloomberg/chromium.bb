// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_

#include <map>
#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/window_open_disposition.h"

class ExtensionMenuItem;
class Profile;
class TabContents;

namespace gfx {
class Point;
}

namespace WebKit {
struct WebMediaPlayerAction;
}

class RenderViewContextMenu {
 public:
  RenderViewContextMenu(TabContents* tab_contents,
                        const ContextMenuParams& params);

  virtual ~RenderViewContextMenu();

  // Initializes the context menu.
  void Init();

 protected:
  void InitMenu();

  // Functions to be implemented by platform-specific subclasses ---------------

  // Platform specific initialization goes here.
  virtual void DoInit() {}

  // Append a normal menu item, taking the name from the id.
  virtual void AppendMenuItem(int id) = 0;

  // Append a normal menu item, using |label| for the name.
  virtual void AppendMenuItem(int id, const string16& label) = 0;

  // Append a radio menu item.
  virtual void AppendRadioMenuItem(int id, const string16& label) = 0;

  // Append a checkbox menu item.
  virtual void AppendCheckboxMenuItem(int id, const string16& label) = 0;

  // Append a separator.
  virtual void AppendSeparator() = 0;

  // Start creating a submenu. Any calls to Append*() between calls to
  // StartSubMenu() and FinishSubMenu() will apply to the submenu rather than
  // the main menu we are building. We only support at most single-depth
  // submenus, so calls to StartSubMenu() while we are already building a
  // submenu will be ignored.
  virtual void StartSubMenu(int id, const string16& label) = 0;

  // Finish creating the submenu and attach it to the main menu.
  virtual void FinishSubMenu() = 0;

  // Delegate functions --------------------------------------------------------

  bool IsItemCommandEnabled(int id) const;
  bool ItemIsChecked(int id) const;
  virtual void ExecuteItemCommand(int id);

 protected:
  ContextMenuParams params_;
  TabContents* source_tab_contents_;
  Profile* profile_;

 private:
  static bool IsDevToolsURL(const GURL& url);
  bool AppendCustomItems();
  void AppendDeveloperItems();
  void AppendLinkItems();
  void AppendImageItems();
  void AppendAudioItems();
  void AppendVideoItems();
  void AppendMediaItems();
  void AppendPageItems();
  void AppendFrameItems();
  void AppendCopyItem();
  void AppendEditableItems();
  void AppendSearchProvider();
  void AppendAllExtensionItems();

  // When extensions have more than 1 top-level item or a single parent item
  // with children, we will start a sub menu. In the case of 1 parent with
  // children, we will remove the parent from |items| and insert the children
  // into it. The |index| parameter is incremented if we start a submenu. This
  // returns true if a submenu was started. If we had multiple top-level items
  // that needed to be pushed into a submenu, we'll use |extension_name| as the
  // title.
  bool MaybeStartExtensionSubMenu(const string16& selection_text,
                                  const std::string& extension_name,
                                  std::vector<const ExtensionMenuItem*>* items,
                                  int* index);

  // Fills in |items| with matching items for extension with |extension_id|.
  void GetItemsForExtension(const std::string& extension_id,
                            std::vector<const ExtensionMenuItem*>* items);

  // This is a helper function to append items for one particular extension.
  // The |index| parameter is used for assigning id's, and is incremented for
  // each item actually added.
  void AppendExtensionItems(const std::string& extension_id, int* index);

  // Opens the specified URL string in a new tab.  If |in_current_window| is
  // false, a new window is created to hold the new tab.
  void OpenURL(const GURL& url,
               WindowOpenDisposition disposition,
               PageTransition::Type transition);

  // Copy to the clipboard an image located at a point in the RenderView
  void CopyImageAt(int x, int y);

  // Launch the inspector targeting a point in the RenderView
  void Inspect(int x, int y);

  // Writes the specified text/url to the system clipboard
  void WriteURLToClipboard(const GURL& url);

  void MediaPlayerActionAt(const gfx::Point& location,
                           const WebKit::WebMediaPlayerAction& action);

  bool IsDevCommandEnabled(int id) const;

  // Returns a (possibly truncated) version of the current selection text
  // suitable or putting in the title of a menu item.
  string16 PrintableSelectionText();

  // Attempts to get an ExtensionMenuItem given the id of a context menu item.
  ExtensionMenuItem* GetExtensionMenuItem(int id) const;

  // The destination URL to use if the user tries to search for or navigate to
  // a text selection.
  GURL selection_navigation_url_;

  // Maps the id from a context menu item to the ExtensionMenuItem's internal
  // id.
  std::map<int, int> extension_item_map_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenu);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_
