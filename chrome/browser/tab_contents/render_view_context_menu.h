// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/tab_contents/render_view_context_menu_observer.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/page_transition_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/window_open_disposition.h"

class PrintPreviewContextMenuObserver;
class Profile;
class SpellingMenuObserver;
class SpellCheckerSubMenuObserver;

namespace content {
class RenderViewHost;
class WebContents;
}

namespace extensions {
class Extension;
class MenuItem;
}

namespace gfx {
class Point;
}

namespace WebKit {
struct WebMediaPlayerAction;
struct WebPluginAction;
}

// An interface that controls a RenderViewContextMenu instance from observers.
// This interface is designed mainly for controlling the instance while showing
// so we can add a context-menu item that takes long time to create its text,
// such as retrieving the item text from a server. The simplest usage is:
// 1. Adding an item with temporary text;
// 2. Posting a background task that creates the item text, and;
// 3. Calling UpdateMenuItem() in the callback function.
// The following snippet describes the simple usage that updates a context-menu
// item with this interface.
//
//   class MyTask : public net::URLFetcherDelegate {
//    public:
//     MyTask(RenderViewContextMenuProxy* proxy, int id)
//         : proxy_(proxy),
//           id_(id) {
//     }
//     virtual ~MyTask() {
//     }
//     virtual void OnURLFetchComplete(const net::URLFetcher* source,
//                                     const GURL& url,
//                                     const net::URLRequestStatus& status,
//                                     int response,
//                                     const net::ResponseCookies& cookies,
//                                     const std::string& data) {
//       bool enabled = response == 200;
//       const char* text = enabled ? "OK" : "ERROR";
//       proxy_->UpdateMenuItem(id_, enabled, ASCIIToUTF16(text));
//     }
//     void Start(const GURL* url, net::URLRequestContextGetter* context) {
//       fetcher_.reset(new URLFetcher(url, URLFetcher::GET, this));
//       fetcher_->SetRequestContext(context);
//       content::AssociateURLFetcherWithRenderView(
//           fetcher_.get(),
//           proxy_->GetRenderViewHost()->GetSiteInstance()->GetSite(),
//           proxy_->GetRenderViewHost()->GetProcess()->GetID(),
//           proxy_->GetRenderViewHost()->GetRoutingID());
//       fetcher_->Start();
//     }
//
//    private:
//     URLFetcher fetcher_;
//     RenderViewContextMenuProxy* proxy_;
//     int id_;
//   };
//
//   void RenderViewContextMenu::AppendEditableItems() {
//     // Add a menu item with temporary text shown while we create the final
//     // text.
//     menu_model_.AddItemWithStringId(IDC_MY_ITEM, IDC_MY_TEXT);
//
//     // Start a task that creates the final text.
//     my_task_ = new MyTask(this, IDC_MY_ITEM);
//     my_task_->Start(...);
//   }
//
class RenderViewContextMenuProxy {
 public:
  // Add a menu item to a context menu.
  virtual void AddMenuItem(int command_id, const string16& title) = 0;
  virtual void AddCheckItem(int command_id, const string16& title) = 0;
  virtual void AddSeparator() = 0;

  // Add a submenu item to a context menu.
  virtual void AddSubMenu(int command_id,
                          const string16& label,
                          ui::MenuModel* model) = 0;

  // Update the status and text of the specified context-menu item.
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const string16& title) = 0;

  // Retrieve the given associated objects with a context menu.
  virtual content::RenderViewHost* GetRenderViewHost() const = 0;
  virtual content::WebContents* GetWebContents() const = 0;
  virtual Profile* GetProfile() const = 0;
};

class RenderViewContextMenu : public ui::SimpleMenuModel::Delegate,
                              public RenderViewContextMenuProxy {
 public:
  static const size_t kMaxSelectionTextLength;

  RenderViewContextMenu(content::WebContents* web_contents,
                        const content::ContextMenuParams& params);

  virtual ~RenderViewContextMenu();

  // Initializes the context menu.
  void Init();

  // Programmatically closes the context menu.
  void Cancel();

  // Provide access to the menu model for ExternalTabContainer.
  const ui::MenuModel& menu_model() const { return menu_model_; }

  // SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual void MenuWillShow(ui::SimpleMenuModel* source) OVERRIDE;
  virtual void MenuClosed(ui::SimpleMenuModel* source) OVERRIDE;

  // RenderViewContextMenuDelegate implementation.
  virtual void AddMenuItem(int command_id, const string16& title) OVERRIDE;
  virtual void AddCheckItem(int command_id, const string16& title) OVERRIDE;
  virtual void AddSeparator() OVERRIDE;
  virtual void AddSubMenu(int command_id,
                          const string16& label,
                          ui::MenuModel* model) OVERRIDE;
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const string16& title) OVERRIDE;
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual Profile* GetProfile() const OVERRIDE;

 protected:
  void InitMenu();

  // Platform specific functions.
  virtual void PlatformInit() = 0;
  virtual void PlatformCancel() = 0;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) = 0;
  virtual void AppendPlatformEditableItems();

  content::ContextMenuParams params_;
  content::WebContents* source_web_contents_;
  Profile* profile_;

  ui::SimpleMenuModel menu_model_;
  extensions::ContextMenuMatcher extension_items_;

  // True if we are showing for an external tab contents. The default is false.
  bool external_;

 private:
  friend class RenderViewContextMenuTest;
  friend class RenderViewContextMenuPrefsTest;

  static bool IsDevToolsURL(const GURL& url);
  static bool IsInternalResourcesURL(const GURL& url);
  static bool ExtensionContextAndPatternMatch(
      const content::ContextMenuParams& params,
      extensions::MenuItem::ContextList contexts,
      const extensions::URLPatternSet& target_url_patterns);
  static bool MenuItemMatchesParams(
      const content::ContextMenuParams& params,
      const extensions::MenuItem* item);

  // Gets the extension (if any) associated with the WebContents that we're in.
  const extensions::Extension* GetExtension() const;
  void AppendPlatformAppItems();
  void AppendPopupExtensionItems();
  void AppendPanelItems();
  bool AppendCustomItems();
  void AppendDeveloperItems();
  void AppendLinkItems();
  void AppendImageItems();
  void AppendAudioItems();
  void AppendVideoItems();
  void AppendMediaItems();
  void AppendPluginItems();
  void AppendPageItems();
  void AppendFrameItems();
  void AppendCopyItem();
  void AppendEditableItems();
  void AppendSearchProvider();
  void AppendAllExtensionItems();
  void AppendSpellingSuggestionsSubMenu();
  void AppendSpellcheckOptionsSubMenu();
  void AppendSpeechInputOptionsSubMenu();
  void AppendProtocolHandlerSubMenu();

  // Opens the specified URL string in a new tab.  The |frame_id| specifies the
  // frame in which the context menu was displayed, or 0 if the menu action is
  // independent of that frame (e.g. protocol handler settings).
  void OpenURL(const GURL& url, const GURL& referrer, int64 frame_id,
               WindowOpenDisposition disposition,
               content::PageTransition transition);

  // Copy to the clipboard an image located at a point in the RenderView
  void CopyImageAt(int x, int y);

  // Launch the inspector targeting a point in the RenderView
  void Inspect(int x, int y);

  // Writes the specified text/url to the system clipboard
  void WriteURLToClipboard(const GURL& url);

  void MediaPlayerActionAt(const gfx::Point& location,
                           const WebKit::WebMediaPlayerAction& action);
  void PluginActionAt(const gfx::Point& location,
                      const WebKit::WebPluginAction& action);

  bool IsDevCommandEnabled(int id) const;

  // Returns a list of registered ProtocolHandlers that can handle the clicked
  // on URL.
  ProtocolHandlerRegistry::ProtocolHandlerList GetHandlersForLinkUrl();

  // Returns a (possibly truncated) version of the current selection text
  // suitable or putting in the title of a menu item.
  string16 PrintableSelectionText();

  // The destination URL to use if the user tries to search for or navigate to
  // a text selection.
  GURL selection_navigation_url_;

  ui::SimpleMenuModel speech_input_submenu_model_;
  ui::SimpleMenuModel protocol_handler_submenu_model_;
  ScopedVector<ui::SimpleMenuModel> extension_menu_models_;
  ProtocolHandlerRegistry* protocol_handler_registry_;

  // An observer that handles spelling-menu items.
  scoped_ptr<SpellingMenuObserver> spelling_menu_observer_;

  // An observer that handles a 'spell-checker options' submenu.
  scoped_ptr<SpellCheckerSubMenuObserver> spellchecker_submenu_observer_;

  // An observer that disables menu items when print preview is active.
  scoped_ptr<PrintPreviewContextMenuObserver> print_preview_menu_observer_;

  // Our observers.
  mutable ObserverList<RenderViewContextMenuObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenu);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_H_
