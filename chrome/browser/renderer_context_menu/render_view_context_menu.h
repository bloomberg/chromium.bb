// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/page_transition_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/window_open_disposition.h"

class PrintPreviewContextMenuObserver;
class Profile;
class SpellingMenuObserver;
class SpellCheckerSubMenuObserver;

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace extensions {
class Extension;
class MenuItem;
}

namespace gfx {
class Point;
}

namespace blink {
struct WebMediaPlayerAction;
struct WebPluginAction;
}

class RenderViewContextMenu : public ui::SimpleMenuModel::Delegate,
                              public RenderViewContextMenuProxy {
 public:
  // A delegate interface to communicate with the toolkit used by
  // the embedder.
  class ToolkitDelegate {
   public:
    virtual ~ToolkitDelegate() {}
    // Initialize the toolkit's menu.
    virtual void Init(ui::SimpleMenuModel* menu_model) = 0;

    virtual void Cancel() = 0;

    // Updates the actual menu items controlled by the toolkit.
    virtual void UpdateMenuItem(int command_id,
                                bool enabled,
                                bool hidden,
                                const base::string16& title) = 0;
  };

  static const size_t kMaxSelectionTextLength;

  // Convert a command ID so that it fits within the range for
  // content context menu.
  static int ConvertToContentCustomCommandId(int id);

  // True if the given id is the one generated for content context menu.
  static bool IsContentCustomCommandId(int id);

  RenderViewContextMenu(content::RenderFrameHost* render_frame_host,
                        const content::ContextMenuParams& params);

  virtual ~RenderViewContextMenu();

  // Initializes the context menu.
  void Init();

  // Programmatically closes the context menu.
  void Cancel();

  const ui::SimpleMenuModel& menu_model() const { return menu_model_; }
  const content::ContextMenuParams& params() const { return params_; }

  // SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual void MenuWillShow(ui::SimpleMenuModel* source) OVERRIDE;
  virtual void MenuClosed(ui::SimpleMenuModel* source) OVERRIDE;

  // RenderViewContextMenuProxy implementation.
  virtual void AddMenuItem(int command_id,
                           const base::string16& title) OVERRIDE;
  virtual void AddCheckItem(int command_id,
                            const base::string16& title) OVERRIDE;
  virtual void AddSeparator() OVERRIDE;
  virtual void AddSubMenu(int command_id,
                          const base::string16& label,
                          ui::MenuModel* model) OVERRIDE;
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const base::string16& title) OVERRIDE;
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE;

 protected:
  void set_toolkit_delegate(scoped_ptr<ToolkitDelegate> delegate) {
    toolkit_delegate_ = delegate.Pass();
  }

  ToolkitDelegate* toolkit_delegate() {
    return toolkit_delegate_.get();
  }

  void InitMenu();
  Profile* GetProfile();

  // Platform specific functions.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) = 0;
  virtual void AppendPlatformEditableItems();

  content::ContextMenuParams params_;
  content::WebContents* source_web_contents_;
  // The RenderFrameHost's IDs.
  int render_process_id_;
  int render_frame_id_;
  content::BrowserContext* browser_context_;

  ui::SimpleMenuModel menu_model_;
  extensions::ContextMenuMatcher extension_items_;

 private:
  friend class RenderViewContextMenuTest;
  friend class RenderViewContextMenuPrefsTest;

  static bool IsDevToolsURL(const GURL& url);
  static bool IsInternalResourcesURL(const GURL& url);
  static bool ExtensionContextAndPatternMatch(
      const content::ContextMenuParams& params,
      extensions::MenuItem::ContextList contexts,
      const extensions::URLPatternSet& target_url_patterns);
  static bool MenuItemMatchesParams(const content::ContextMenuParams& params,
                                    const extensions::MenuItem* item);

  // Gets the extension (if any) associated with the WebContents that we're in.
  const extensions::Extension* GetExtension() const;
  bool AppendCustomItems();

  void AppendDeveloperItems();
  void AppendDevtoolsForUnpackedExtensions();
  void AppendLinkItems();
  void AppendImageItems();
  void AppendAudioItems();
  void AppendCanvasItems();
  void AppendVideoItems();
  void AppendMediaItems();
  void AppendPluginItems();
  void AppendPageItems();
  void AppendFrameItems();
  void AppendCopyItem();
  void AppendPrintItem();
  void AppendEditableItems();
  void AppendSearchProvider();
  void AppendAllExtensionItems();
  void AppendCurrentExtensionItems();
  void AppendPrintPreviewItems();
  void AppendSearchWebForImageItems();
  void AppendSpellingSuggestionsSubMenu();
  void AppendSpellcheckOptionsSubMenu();
  void AppendProtocolHandlerSubMenu();

  // Opens the specified URL string in a new tab.
  void OpenURL(const GURL& url, const GURL& referrer,
               WindowOpenDisposition disposition,
               content::PageTransition transition);

  // Copy to the clipboard an image located at a point in the RenderView
  void CopyImageAt(int x, int y);

  // Get an image located at a point in the RenderView for search.
  void GetImageThumbnailForSearch();

  // Launch the inspector targeting a point in the RenderView
  void Inspect(int x, int y);

  // Writes the specified text/url to the system clipboard
  void WriteURLToClipboard(const GURL& url);

  void MediaPlayerActionAt(const gfx::Point& location,
                           const blink::WebMediaPlayerAction& action);
  void PluginActionAt(const gfx::Point& location,
                      const blink::WebPluginAction& action);

  bool IsDevCommandEnabled(int id) const;

  // Returns a list of registered ProtocolHandlers that can handle the clicked
  // on URL.
  ProtocolHandlerRegistry::ProtocolHandlerList GetHandlersForLinkUrl();

  // Returns a (possibly truncated) version of the current selection text
  // suitable or putting in the title of a menu item.
  base::string16 PrintableSelectionText();

  // The destination URL to use if the user tries to search for or navigate to
  // a text selection.
  GURL selection_navigation_url_;

  ui::SimpleMenuModel protocol_handler_submenu_model_;
  ProtocolHandlerRegistry* protocol_handler_registry_;

  // An observer that handles spelling-menu items.
  scoped_ptr<SpellingMenuObserver> spelling_menu_observer_;

  // An observer that handles a 'spell-checker options' submenu.
  scoped_ptr<SpellCheckerSubMenuObserver> spellchecker_submenu_observer_;

#if defined(ENABLE_FULL_PRINTING)
  // An observer that disables menu items when print preview is active.
  scoped_ptr<PrintPreviewContextMenuObserver> print_preview_menu_observer_;
#endif

  // Our observers.
  mutable ObserverList<RenderViewContextMenuObserver> observers_;

  // Whether a command has been executed. Used to track whether menu observers
  // should be notified of menu closing without execution.
  bool command_executed_;

  scoped_ptr<ContextMenuContentType> content_type_;

  scoped_ptr<ToolkitDelegate> toolkit_delegate_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenu);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_
