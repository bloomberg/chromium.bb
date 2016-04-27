// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/common/context_menu_params.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/vector2d.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#endif

class OpenWithMenuObserver;
class PrintPreviewContextMenuObserver;
class Profile;
class SpellingMenuObserver;
class SpellingOptionsSubMenuObserver;

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

class RenderViewContextMenu : public RenderViewContextMenuBase {
 public:
  RenderViewContextMenu(content::RenderFrameHost* render_frame_host,
                        const content::ContextMenuParams& params);

  ~RenderViewContextMenu() override;

  // Returns the offset amount if the context menu requires off-setting.
  //
  // If |render_frame_host| belongs to a WebContents that is nested within
  // other WebContents(s), then this value is the offset between the topmost
  // WebContents and the frame's WebContents.
  static gfx::Vector2d GetOffset(content::RenderFrameHost* render_frame_host);

  // Adds the spell check service item to the context menu.
  static void AddSpellCheckServiceItem(ui::SimpleMenuModel* menu,
                                       bool is_checked);

  // RenderViewContextMenuBase:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void AddSpellCheckServiceItem(bool is_checked) override;

 protected:
  Profile* GetProfile();

  // Returns a (possibly truncated) version of the current selection text
  // suitable for putting in the title of a menu item.
  base::string16 PrintableSelectionText();

  // Helper function to escape "&" as "&&".
  void EscapeAmpersands(base::string16* text);

#if defined(ENABLE_EXTENSIONS)
  extensions::ContextMenuMatcher extension_items_;
#endif
  void RecordUsedItem(int id) override;

 private:
  friend class RenderViewContextMenuTest;
  friend class TestRenderViewContextMenu;

  static bool IsDevToolsURL(const GURL& url);
  static bool IsInternalResourcesURL(const GURL& url);
#if defined(ENABLE_EXTENSIONS)
  static bool ExtensionContextAndPatternMatch(
      const content::ContextMenuParams& params,
      const extensions::MenuItem::ContextList& contexts,
      const extensions::URLPatternSet& target_url_patterns);
  static bool MenuItemMatchesParams(const content::ContextMenuParams& params,
                                    const extensions::MenuItem* item);
#endif

  // RenderViewContextMenuBase:
  void InitMenu() override;
  void RecordShownItem(int id) override;
#if defined(ENABLE_PLUGINS)
  void HandleAuthorizeAllPlugins() override;
#endif
  void NotifyMenuShown() override;
  void NotifyURLOpened(const GURL& url,
                       content::WebContents* new_contents) override;

  // Gets the extension (if any) associated with the WebContents that we're in.
  const extensions::Extension* GetExtension() const;

  void AppendDeveloperItems();
  void AppendDevtoolsForUnpackedExtensions();
  void AppendLinkItems();
  void AppendOpenWithLinkItems();
  void AppendImageItems();
  void AppendAudioItems();
  void AppendCanvasItems();
  void AppendVideoItems();
  void AppendMediaItems();
  void AppendPluginItems();
  void AppendPageItems();
  void AppendCopyItem();
  void AppendPrintItem();
  void AppendMediaRouterItem();
  void AppendRotationItems();
  void AppendEditableItems();
  void AppendLanguageSettings();
  void AppendSpellingSuggestionItems();
  void AppendSearchProvider();
#if defined(ENABLE_EXTENSIONS)
  void AppendAllExtensionItems();
  void AppendCurrentExtensionItems();
#endif
  void AppendPrintPreviewItems();
  void AppendSearchWebForImageItems();
  void AppendProtocolHandlerSubMenu();
  void AppendPasswordItems();

  // Copy to the clipboard an image located at a point in the RenderView
  void CopyImageAt(int x, int y);

  // Load the original image located at a point in the RenderView.
  void LoadOriginalImage();

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

  // The destination URL to use if the user tries to search for or navigate to
  // a text selection.
  GURL selection_navigation_url_;

  ui::SimpleMenuModel profile_link_submenu_model_;
  std::vector<base::FilePath> profile_link_paths_;
  bool multiple_profiles_open_;
  ui::SimpleMenuModel protocol_handler_submenu_model_;
  ProtocolHandlerRegistry* protocol_handler_registry_;

  // An observer that handles spelling suggestions, "Add to dictionary", and
  // "Ask Google for suggestions" items.
  std::unique_ptr<SpellingMenuObserver> spelling_suggestions_menu_observer_;

#if !defined(OS_MACOSX)
  // An observer that handles the submenu for showing spelling options. This
  // submenu lets users select the spelling language, for example.
  std::unique_ptr<SpellingOptionsSubMenuObserver>
      spelling_options_submenu_observer_;
#endif

  // An observer that handles "Open with <app>" items.
  std::unique_ptr<RenderViewContextMenuObserver> open_with_menu_observer_;

#if defined(ENABLE_PRINT_PREVIEW)
  // An observer that disables menu items when print preview is active.
  std::unique_ptr<PrintPreviewContextMenuObserver> print_preview_menu_observer_;
#endif

  // In the case of a MimeHandlerView this will point to the WebContents that
  // embeds the MimeHandlerViewGuest. Otherwise this will be the same as
  // |source_web_contents_|.
  content::WebContents* const embedder_web_contents_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenu);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_H_
