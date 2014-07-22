// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include <algorithm>
#include <set>
#include <utility>

#include "apps/app_load_service.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/guest_view/web_view/web_view_guest.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"
#include "chrome/browser/renderer_context_menu/spellchecker_submenu_observer.h"
#include "chrome/browser/renderer_context_menu/spelling_menu_observer.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/net/url_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "third_party/WebKit/public/web/WebMediaPlayerAction.h"
#include "third_party/WebKit/public/web/WebPluginAction.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"
#include "ui/gfx/text_elider.h"

#if defined(ENABLE_PRINTING)
#include "chrome/common/print_messages.h"

#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/print_preview_context_menu_observer.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)

using base::UserMetricsAction;
using blink::WebContextMenuData;
using blink::WebMediaPlayerAction;
using blink::WebPluginAction;
using blink::WebString;
using blink::WebURL;
using content::BrowserContext;
using content::ChildProcessSecurityPolicy;
using content::DownloadManager;
using content::DownloadUrlParameters;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::SSLStatus;
using content::WebContents;
using extensions::Extension;
using extensions::MenuItem;
using extensions::MenuManager;

namespace {

const int kImageSearchThumbnailMinSize = 300 * 300;
const int kImageSearchThumbnailMaxWidth = 600;
const int kImageSearchThumbnailMaxHeight = 600;

// Maps UMA enumeration to IDC. IDC could be changed so we can't use
// just them and |UMA_HISTOGRAM_CUSTOM_ENUMERATION|.
// Never change mapping or reuse |enum_id|. Always push back new items.
// Items that is not used any more by |RenderViewContextMenu.ExecuteCommand|
// could be deleted, but don't change the rest of |kUmaEnumToControlId|.
const struct UmaEnumCommandIdPair {
  int enum_id;
  int control_id;
} kUmaEnumToControlId[] = {
  {  0, IDC_CONTENT_CONTEXT_CUSTOM_FIRST },
  {  1, IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST },
  {  2, IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST },
  {  3, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB },
  {  4, IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW },
  {  5, IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD },
  {  6, IDC_CONTENT_CONTEXT_SAVELINKAS },
  {  7, IDC_CONTENT_CONTEXT_SAVEAVAS },
  {  8, IDC_CONTENT_CONTEXT_SAVEIMAGEAS },
  {  9, IDC_CONTENT_CONTEXT_COPYLINKLOCATION },
  { 10, IDC_CONTENT_CONTEXT_COPYIMAGELOCATION },
  { 11, IDC_CONTENT_CONTEXT_COPYAVLOCATION },
  { 12, IDC_CONTENT_CONTEXT_COPYIMAGE },
  { 13, IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB },
  { 14, IDC_CONTENT_CONTEXT_OPENAVNEWTAB },
  { 15, IDC_CONTENT_CONTEXT_PLAYPAUSE },
  { 16, IDC_CONTENT_CONTEXT_MUTE },
  { 17, IDC_CONTENT_CONTEXT_LOOP },
  { 18, IDC_CONTENT_CONTEXT_CONTROLS },
  { 19, IDC_CONTENT_CONTEXT_ROTATECW },
  { 20, IDC_CONTENT_CONTEXT_ROTATECCW },
  { 21, IDC_BACK },
  { 22, IDC_FORWARD },
  { 23, IDC_SAVE_PAGE },
  { 24, IDC_RELOAD },
  { 25, IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP },
  { 26, IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP },
  { 27, IDC_PRINT },
  { 28, IDC_VIEW_SOURCE },
  { 29, IDC_CONTENT_CONTEXT_INSPECTELEMENT },
  { 30, IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE },
  { 31, IDC_CONTENT_CONTEXT_VIEWPAGEINFO },
  { 32, IDC_CONTENT_CONTEXT_TRANSLATE },
  { 33, IDC_CONTENT_CONTEXT_RELOADFRAME },
  { 34, IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE },
  { 35, IDC_CONTENT_CONTEXT_VIEWFRAMEINFO },
  { 36, IDC_CONTENT_CONTEXT_UNDO },
  { 37, IDC_CONTENT_CONTEXT_REDO },
  { 38, IDC_CONTENT_CONTEXT_CUT },
  { 39, IDC_CONTENT_CONTEXT_COPY },
  { 40, IDC_CONTENT_CONTEXT_PASTE },
  { 41, IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE },
  { 42, IDC_CONTENT_CONTEXT_DELETE },
  { 43, IDC_CONTENT_CONTEXT_SELECTALL },
  { 44, IDC_CONTENT_CONTEXT_SEARCHWEBFOR },
  { 45, IDC_CONTENT_CONTEXT_GOTOURL },
  { 46, IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS },
  { 47, IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS },
  { 48, IDC_CONTENT_CONTEXT_ADDSEARCHENGINE },
  { 52, IDC_CONTENT_CONTEXT_OPENLINKWITH },
  { 53, IDC_CHECK_SPELLING_WHILE_TYPING },
  { 54, IDC_SPELLCHECK_MENU },
  { 55, IDC_CONTENT_CONTEXT_SPELLING_TOGGLE },
  { 56, IDC_SPELLCHECK_LANGUAGES_FIRST },
  { 57, IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE },
  { 58, IDC_SPELLCHECK_SUGGESTION_0 },
  { 59, IDC_SPELLCHECK_ADD_TO_DICTIONARY },
  { 60, IDC_SPELLPANEL_TOGGLE },
  // Add new items here and use |enum_id| from the next line.
  { 61, 0 },  // Must be the last. Increment |enum_id| when new IDC was added.
};

// Collapses large ranges of ids before looking for UMA enum.
int CollapseCommandsForUMA(int id) {
  if (id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_CONTENT_CONTEXT_CUSTOM_LAST) {
    return IDC_CONTENT_CONTEXT_CUSTOM_FIRST;
  }

  if (id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST;
  }

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    return IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST;
  }

  if (id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      id <= IDC_SPELLCHECK_LANGUAGES_LAST) {
    return IDC_SPELLCHECK_LANGUAGES_FIRST;
  }

  if (id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      id <= IDC_SPELLCHECK_SUGGESTION_LAST) {
    return IDC_SPELLCHECK_SUGGESTION_0;
  }

  return id;
}

// Returns UMA enum value for command specified by |id| or -1 if not found.
int FindUMAEnumValueForCommand(int id) {
  id = CollapseCommandsForUMA(id);
  const size_t kMappingSize = arraysize(kUmaEnumToControlId);
  for (size_t i = 0; i < kMappingSize; ++i) {
    if (kUmaEnumToControlId[i].control_id == id) {
      return kUmaEnumToControlId[i].enum_id;
    }
  }
  return -1;
}

// Increments histogram value for used items specified by |id|.
void RecordUsedItem(int id) {
  int enum_id = FindUMAEnumValueForCommand(id);
  if (enum_id != -1) {
    const size_t kMappingSize = arraysize(kUmaEnumToControlId);
    UMA_HISTOGRAM_ENUMERATION("RenderViewContextMenu.Used", enum_id,
                              kUmaEnumToControlId[kMappingSize - 1].enum_id);
  } else {
    NOTREACHED() << "Update kUmaEnumToControlId. Unhanded IDC: " << id;
  }
}

// Increments histogram value for visible context menu item specified by |id|.
void RecordShownItem(int id) {
  int enum_id = FindUMAEnumValueForCommand(id);
  if (enum_id != -1) {
    const size_t kMappingSize = arraysize(kUmaEnumToControlId);
    UMA_HISTOGRAM_ENUMERATION("RenderViewContextMenu.Shown", enum_id,
                              kUmaEnumToControlId[kMappingSize - 1].enum_id);
  } else {
    // Just warning here. It's harder to maintain list of all possibly
    // visible items than executable items.
    DLOG(ERROR) << "Update kUmaEnumToControlId. Unhanded IDC: " << id;
  }
}

// Usually a new tab is expected where this function is used,
// however users should be able to open a tab in background
// or in a new window.
WindowOpenDisposition ForceNewTabDispositionFromEventFlags(
    int event_flags) {
  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  return disposition == CURRENT_TAB ? NEW_FOREGROUND_TAB : disposition;
}

bool IsCustomItemEnabled(const std::vector<content::MenuItem>& items, int id) {
  DCHECK(id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST &&
         id <= IDC_CONTENT_CONTEXT_CUSTOM_LAST);
  for (size_t i = 0; i < items.size(); ++i) {
    int action_id = IDC_CONTENT_CONTEXT_CUSTOM_FIRST + items[i].action;
    if (action_id == id)
      return items[i].enabled;
    if (items[i].type == content::MenuItem::SUBMENU) {
      if (IsCustomItemEnabled(items[i].submenu, id))
        return true;
    }
  }
  return false;
}

bool IsCustomItemChecked(const std::vector<content::MenuItem>& items, int id) {
  DCHECK(id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST &&
         id <= IDC_CONTENT_CONTEXT_CUSTOM_LAST);
  for (size_t i = 0; i < items.size(); ++i) {
    int action_id = IDC_CONTENT_CONTEXT_CUSTOM_FIRST + items[i].action;
    if (action_id == id)
      return items[i].checked;
    if (items[i].type == content::MenuItem::SUBMENU) {
      if (IsCustomItemChecked(items[i].submenu, id))
        return true;
    }
  }
  return false;
}

const size_t kMaxCustomMenuDepth = 5;
const size_t kMaxCustomMenuTotalItems = 1000;

void AddCustomItemsToMenu(const std::vector<content::MenuItem>& items,
                          size_t depth,
                          size_t* total_items,
                          ui::SimpleMenuModel::Delegate* delegate,
                          ui::SimpleMenuModel* menu_model) {
  if (depth > kMaxCustomMenuDepth) {
    LOG(ERROR) << "Custom menu too deeply nested.";
    return;
  }
  for (size_t i = 0; i < items.size(); ++i) {
    if (IDC_CONTENT_CONTEXT_CUSTOM_FIRST + items[i].action >=
        IDC_CONTENT_CONTEXT_CUSTOM_LAST) {
      LOG(ERROR) << "Custom menu action value too big.";
      return;
    }
    if (*total_items >= kMaxCustomMenuTotalItems) {
      LOG(ERROR) << "Custom menu too large (too many items).";
      return;
    }
    (*total_items)++;
    switch (items[i].type) {
      case content::MenuItem::OPTION:
        menu_model->AddItem(
            items[i].action + IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
            items[i].label);
        break;
      case content::MenuItem::CHECKABLE_OPTION:
        menu_model->AddCheckItem(
            items[i].action + IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
            items[i].label);
        break;
      case content::MenuItem::GROUP:
        // TODO(viettrungluu): I don't know what this is supposed to do.
        NOTREACHED();
        break;
      case content::MenuItem::SEPARATOR:
        menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
        break;
      case content::MenuItem::SUBMENU: {
        ui::SimpleMenuModel* submenu = new ui::SimpleMenuModel(delegate);
        AddCustomItemsToMenu(items[i].submenu, depth + 1, total_items, delegate,
                             submenu);
        menu_model->AddSubMenu(
            items[i].action + IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
            items[i].label,
            submenu);
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }
}

// Helper function to escape "&" as "&&".
void EscapeAmpersands(base::string16* text) {
  base::ReplaceChars(*text, base::ASCIIToUTF16("&"), base::ASCIIToUTF16("&&"),
                     text);
}

}  // namespace

// static
const size_t RenderViewContextMenu::kMaxSelectionTextLength = 50;

// static
bool RenderViewContextMenu::IsDevToolsURL(const GURL& url) {
  return url.SchemeIs(content::kChromeDevToolsScheme);
}

// static
bool RenderViewContextMenu::IsInternalResourcesURL(const GURL& url) {
  if (!url.SchemeIs(content::kChromeUIScheme))
    return false;
  return url.host() == chrome::kChromeUISyncResourcesHost;
}

static const int kSpellcheckRadioGroup = 1;

RenderViewContextMenu::RenderViewContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params)
    : params_(params),
      source_web_contents_(WebContents::FromRenderFrameHost(render_frame_host)),
      render_process_id_(render_frame_host->GetProcess()->GetID()),
      render_frame_id_(render_frame_host->GetRoutingID()),
      profile_(Profile::FromBrowserContext(
          source_web_contents_->GetBrowserContext())),
      menu_model_(this),
      extension_items_(profile_,
                       this,
                       &menu_model_,
                       base::Bind(MenuItemMatchesParams, params_)),
      protocol_handler_submenu_model_(this),
      protocol_handler_registry_(
          ProtocolHandlerRegistryFactory::GetForProfile(profile_)),
      command_executed_(false) {
  content_type_.reset(ContextMenuContentTypeFactory::Create(
                          source_web_contents_, params));
}

RenderViewContextMenu::~RenderViewContextMenu() {
}

// Menu construction functions -------------------------------------------------

void RenderViewContextMenu::Init() {
  InitMenu();
  PlatformInit();
}

void RenderViewContextMenu::Cancel() {
  PlatformCancel();
}

static bool ExtensionPatternMatch(const extensions::URLPatternSet& patterns,
                                  const GURL& url) {
  // No patterns means no restriction, so that implicitly matches.
  if (patterns.is_empty())
    return true;
  return patterns.MatchesURL(url);
}

// static
bool RenderViewContextMenu::ExtensionContextAndPatternMatch(
    const content::ContextMenuParams& params,
    MenuItem::ContextList contexts,
    const extensions::URLPatternSet& target_url_patterns) {
  const bool has_link = !params.link_url.is_empty();
  const bool has_selection = !params.selection_text.empty();
  const bool in_frame = !params.frame_url.is_empty();

  if (contexts.Contains(MenuItem::ALL) ||
      (has_selection && contexts.Contains(MenuItem::SELECTION)) ||
      (params.is_editable && contexts.Contains(MenuItem::EDITABLE)) ||
      (in_frame && contexts.Contains(MenuItem::FRAME)))
    return true;

  if (has_link && contexts.Contains(MenuItem::LINK) &&
      ExtensionPatternMatch(target_url_patterns, params.link_url))
    return true;

  switch (params.media_type) {
    case WebContextMenuData::MediaTypeImage:
      if (contexts.Contains(MenuItem::IMAGE) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    case WebContextMenuData::MediaTypeVideo:
      if (contexts.Contains(MenuItem::VIDEO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    case WebContextMenuData::MediaTypeAudio:
      if (contexts.Contains(MenuItem::AUDIO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    default:
      break;
  }

  // PAGE is the least specific context, so we only examine that if none of the
  // other contexts apply (except for FRAME, which is included in PAGE for
  // backwards compatibility).
  if (!has_link && !has_selection && !params.is_editable &&
      params.media_type == WebContextMenuData::MediaTypeNone &&
      contexts.Contains(MenuItem::PAGE))
    return true;

  return false;
}

static const GURL& GetDocumentURL(const content::ContextMenuParams& params) {
  return params.frame_url.is_empty() ? params.page_url : params.frame_url;
}

// static
bool RenderViewContextMenu::MenuItemMatchesParams(
    const content::ContextMenuParams& params,
    const extensions::MenuItem* item) {
  bool match = ExtensionContextAndPatternMatch(params, item->contexts(),
                                               item->target_url_patterns());
  if (!match)
    return false;

  const GURL& document_url = GetDocumentURL(params);
  return ExtensionPatternMatch(item->document_url_patterns(), document_url);
}

void RenderViewContextMenu::AppendAllExtensionItems() {
  extension_items_.Clear();
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service)
    return;  // In unit-tests, we may not have an ExtensionService.

  MenuManager* menu_manager = MenuManager::Get(profile_);
  if (!menu_manager)
    return;

  base::string16 printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(&printable_selection_text);

  // Get a list of extension id's that have context menu items, and sort by the
  // top level context menu title of the extension.
  std::set<MenuItem::ExtensionKey> ids = menu_manager->ExtensionIds();
  std::vector<base::string16> sorted_menu_titles;
  std::map<base::string16, std::string> map_ids;
  for (std::set<MenuItem::ExtensionKey>::iterator i = ids.begin();
       i != ids.end();
       ++i) {
    const Extension* extension =
        service->GetExtensionById(i->extension_id, false);
    // Platform apps have their context menus created directly in
    // AppendPlatformAppItems.
    if (extension && !extension->is_platform_app()) {
      base::string16 menu_title = extension_items_.GetTopLevelContextMenuTitle(
          *i, printable_selection_text);
      map_ids[menu_title] = i->extension_id;
      sorted_menu_titles.push_back(menu_title);
    }
  }
  if (sorted_menu_titles.empty())
    return;

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  l10n_util::SortStrings16(app_locale, &sorted_menu_titles);

  int index = 0;
  base::TimeTicks begin = base::TimeTicks::Now();
  for (size_t i = 0; i < sorted_menu_titles.size(); ++i) {
    const std::string& id = map_ids[sorted_menu_titles[i]];
    const MenuItem::ExtensionKey extension_key(id);
    extension_items_.AppendExtensionItems(
        extension_key, printable_selection_text, &index);
  }

  UMA_HISTOGRAM_TIMES("Extensions.ContextMenus_BuildTime",
                      base::TimeTicks::Now() - begin);
  UMA_HISTOGRAM_COUNTS("Extensions.ContextMenus_ItemCount", index);
}

void RenderViewContextMenu::AppendCurrentExtensionItems() {
  // Avoid appending extension related items when |extension| is null.
  // For Panel, this happens when the panel is navigated to a url outside of the
  // extension's package.
  const Extension* extension = GetExtension();
  if (extension) {
    // Only add extension items from this extension.
    int index = 0;
    const MenuItem::ExtensionKey key(
        extension->id(), WebViewGuest::GetViewInstanceId(source_web_contents_));
    extension_items_.AppendExtensionItems(
        key, PrintableSelectionText(), &index);
  }
}

void RenderViewContextMenu::InitMenu() {
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_CUSTOM)) {
    AppendCustomItems();

    const bool has_selection = !params_.selection_text.empty();
    if (has_selection) {
      // We will add more items if there's a selection, so add a separator.
      // TODO(lazyboy): Clean up separator logic.
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    }
  }

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE))
    AppendPageItems();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_FRAME)) {
    // Merge in frame items with page items if we clicked within a frame that
    // needs them.
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    AppendFrameItems();
  }

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK)) {
    AppendLinkItems();
    if (params_.media_type != WebContextMenuData::MediaTypeNone)
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE)) {
    AppendImageItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SEARCHWEBFORIMAGE)) {
    AppendSearchWebForImageItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_VIDEO)) {
    AppendVideoItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_AUDIO)) {
    AppendAudioItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_CANVAS)) {
    AppendCanvasItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN)) {
    AppendPluginItems();
  }

  // ITEM_GROUP_MEDIA_FILE has no specific items.

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_EDITABLE))
    AppendEditableItems();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_COPY)) {
    DCHECK(!content_type_->SupportsGroup(
               ContextMenuContentType::ITEM_GROUP_EDITABLE));
    AppendCopyItem();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SEARCH_PROVIDER)) {
    AppendSearchProvider();
  }

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT))
    AppendPrintItem();

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION)) {
    DCHECK(!content_type_->SupportsGroup(
               ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION));
    AppendAllExtensionItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION)) {
    DCHECK(!content_type_->SupportsGroup(
               ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION));
    AppendCurrentExtensionItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_DEVELOPER)) {
    AppendDeveloperItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_DEVTOOLS_UNPACKED_EXT)) {
    AppendDevtoolsForUnpackedExtensions();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_PRINT_PREVIEW)) {
    AppendPrintPreviewItems();
  }
}

void RenderViewContextMenu::AppendPrintPreviewItems() {
#if defined(ENABLE_FULL_PRINTING)
  if (!print_preview_menu_observer_.get()) {
    print_preview_menu_observer_.reset(
        new PrintPreviewContextMenuObserver(source_web_contents_));
  }

  observers_.AddObserver(print_preview_menu_observer_.get());
#endif
}

const Extension* RenderViewContextMenu::GetExtension() const {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  // There is no process manager in some tests.
  if (!system->process_manager())
    return NULL;

  return system->process_manager()->GetExtensionForRenderViewHost(
      source_web_contents_->GetRenderViewHost());
}

void RenderViewContextMenu::AddMenuItem(int command_id,
                                        const base::string16& title) {
  menu_model_.AddItem(command_id, title);
}

void RenderViewContextMenu::AddCheckItem(int command_id,
                                         const base::string16& title) {
  menu_model_.AddCheckItem(command_id, title);
}

void RenderViewContextMenu::AddSeparator() {
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AddSubMenu(int command_id,
                                       const base::string16& label,
                                       ui::MenuModel* model) {
  menu_model_.AddSubMenu(command_id, label, model);
}

void RenderViewContextMenu::UpdateMenuItem(int command_id,
                                           bool enabled,
                                           bool hidden,
                                           const base::string16& label) {
  // This function needs platform-specific implementation.
  NOTIMPLEMENTED();
}

RenderViewHost* RenderViewContextMenu::GetRenderViewHost() const {
  return source_web_contents_->GetRenderViewHost();
}

WebContents* RenderViewContextMenu::GetWebContents() const {
  return source_web_contents_;
}

Profile* RenderViewContextMenu::GetProfile() const {
  return profile_;
}

bool RenderViewContextMenu::AppendCustomItems() {
  size_t total_items = 0;
  AddCustomItemsToMenu(params_.custom_items, 0, &total_items, this,
                       &menu_model_);
  return total_items > 0;
}

void RenderViewContextMenu::AppendDeveloperItems() {
  // Show Inspect Element in DevTools itself only in case of the debug
  // devtools build.
  bool show_developer_items = !IsDevToolsURL(params_.page_url);

#if defined(DEBUG_DEVTOOLS)
  show_developer_items = true;
#endif

  if (!show_developer_items)
    return;

  // In the DevTools popup menu, "developer items" is normally the only
  // section, so omit the separator there.
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTELEMENT,
                                  IDS_CONTENT_CONTEXT_INSPECTELEMENT);
}

void RenderViewContextMenu::AppendDevtoolsForUnpackedExtensions() {
  // Add a separator if there are any items already in the menu.
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP,
                                  IDS_CONTENT_CONTEXT_RELOAD_PACKAGED_APP);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP,
                                  IDS_CONTENT_CONTEXT_RESTART_APP);
  AppendDeveloperItems();
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE,
                                  IDS_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE);
}

void RenderViewContextMenu::AppendLinkItems() {
  if (!params_.link_url.is_empty()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                    IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                                    IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
    if (params_.link_url.is_valid()) {
      AppendProtocolHandlerSubMenu();
    }

    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
                                    IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVELINKAS,
                                    IDS_CONTENT_CONTEXT_SAVELINKAS);
  }

  menu_model_.AddItemWithStringId(
      IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
      params_.link_url.SchemeIs(url::kMailToScheme) ?
          IDS_CONTENT_CONTEXT_COPYEMAILADDRESS :
          IDS_CONTENT_CONTEXT_COPYLINKLOCATION);
}

void RenderViewContextMenu::AppendImageItems() {
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
                                  IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGELOCATION,
                                  IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGE,
                                  IDS_CONTENT_CONTEXT_COPYIMAGE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

void RenderViewContextMenu::AppendSearchWebForImageItems() {
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* const default_provider =
      service->GetDefaultSearchProvider();
  if (params_.has_image_contents && default_provider &&
      !default_provider->image_url().empty() &&
      default_provider->image_url_ref().IsValid(service->search_terms_data())) {
    menu_model_.AddItem(
        IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFORIMAGE,
                                   default_provider->short_name()));
  }
}

void RenderViewContextMenu::AppendAudioItems() {
  AppendMediaItems();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                  IDS_CONTENT_CONTEXT_SAVEAUDIOAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYAVLOCATION,
                                  IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENAVNEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB);
}

void RenderViewContextMenu::AppendCanvasItems() {
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
                                  IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGE,
                                  IDS_CONTENT_CONTEXT_COPYIMAGE);
}

void RenderViewContextMenu::AppendVideoItems() {
  AppendMediaItems();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                  IDS_CONTENT_CONTEXT_SAVEVIDEOAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYAVLOCATION,
                                  IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENAVNEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB);
}

void RenderViewContextMenu::AppendMediaItems() {
  int media_flags = params_.media_flags;

  menu_model_.AddItemWithStringId(
      IDC_CONTENT_CONTEXT_PLAYPAUSE,
      media_flags & WebContextMenuData::MediaPaused ?
          IDS_CONTENT_CONTEXT_PLAY :
          IDS_CONTENT_CONTEXT_PAUSE);

  menu_model_.AddItemWithStringId(
      IDC_CONTENT_CONTEXT_MUTE,
      media_flags & WebContextMenuData::MediaMuted ?
          IDS_CONTENT_CONTEXT_UNMUTE :
          IDS_CONTENT_CONTEXT_MUTE);

  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_LOOP,
                                       IDS_CONTENT_CONTEXT_LOOP);
  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_CONTROLS,
                                       IDS_CONTENT_CONTEXT_CONTROLS);
}

void RenderViewContextMenu::AppendPluginItems() {
  if (params_.page_url == params_.src_url) {
    // Full page plugin, so show page menu items.
    if (params_.link_url.is_empty() && params_.selection_text.empty())
      AppendPageItems();
  } else {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                    IDS_CONTENT_CONTEXT_SAVEPAGEAS);
    menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  }

  if (params_.media_flags & WebContextMenuData::MediaCanRotate) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_ROTATECW,
                                    IDS_CONTENT_CONTEXT_ROTATECW);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_ROTATECCW,
                                    IDS_CONTENT_CONTEXT_ROTATECCW);
  }
}

void RenderViewContextMenu::AppendPageItems() {
  menu_model_.AddItemWithStringId(IDC_BACK, IDS_CONTENT_CONTEXT_BACK);
  menu_model_.AddItemWithStringId(IDC_FORWARD, IDS_CONTENT_CONTEXT_FORWARD);
  menu_model_.AddItemWithStringId(IDC_RELOAD, IDS_CONTENT_CONTEXT_RELOAD);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_SAVE_PAGE,
                                  IDS_CONTENT_CONTEXT_SAVEPAGEAS);
  menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);

  if (TranslateService::IsTranslatableURL(params_.page_url)) {
    std::string locale = g_browser_process->GetApplicationLocale();
    locale = translate::TranslateDownloadManager::GetLanguageCode(locale);
    base::string16 language =
        l10n_util::GetDisplayNameForLocale(locale, locale, true);
    menu_model_.AddItem(
        IDC_CONTENT_CONTEXT_TRANSLATE,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_TRANSLATE, language));
  }

  menu_model_.AddItemWithStringId(IDC_VIEW_SOURCE,
                                  IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_VIEWPAGEINFO,
                                  IDS_CONTENT_CONTEXT_VIEWPAGEINFO);
}

void RenderViewContextMenu::AppendFrameItems() {
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOADFRAME,
                                  IDS_CONTENT_CONTEXT_RELOADFRAME);
  // These two menu items have yet to be implemented.
  // http://code.google.com/p/chromium/issues/detail?id=11827
  //   IDS_CONTENT_CONTEXT_SAVEFRAMEAS
  //   IDS_CONTENT_CONTEXT_PRINTFRAME
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE,
                                  IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_VIEWFRAMEINFO,
                                  IDS_CONTENT_CONTEXT_VIEWFRAMEINFO);
}

void RenderViewContextMenu::AppendCopyItem() {
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPY,
                                  IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendPrintItem() {
  if (profile_->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
      (params_.media_type == WebContextMenuData::MediaTypeNone ||
       params_.media_flags & WebContextMenuData::MediaCanPrint)) {
    menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  }
}

void RenderViewContextMenu::AppendSearchProvider() {
  DCHECK(profile_);

  base::TrimWhitespace(params_.selection_text, base::TRIM_ALL,
                       &params_.selection_text);
  if (params_.selection_text.empty())
    return;

  base::ReplaceChars(params_.selection_text, AutocompleteMatch::kInvalidChars,
                     base::ASCIIToUTF16(" "), &params_.selection_text);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(profile_)->Classify(
      params_.selection_text, false, false,
      metrics::OmniboxEventProto::INVALID_SPEC, &match, NULL);
  selection_navigation_url_ = match.destination_url;
  if (!selection_navigation_url_.is_valid())
    return;

  base::string16 printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(&printable_selection_text);

  if (AutocompleteMatch::IsSearchType(match.type)) {
    const TemplateURL* const default_provider =
        TemplateURLServiceFactory::GetForProfile(profile_)->
        GetDefaultSearchProvider();
    if (!default_provider)
      return;
    menu_model_.AddItem(
        IDC_CONTENT_CONTEXT_SEARCHWEBFOR,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
                                   default_provider->short_name(),
                                   printable_selection_text));
  } else {
    if ((selection_navigation_url_ != params_.link_url) &&
        ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
            selection_navigation_url_.scheme())) {
      menu_model_.AddItem(
          IDC_CONTENT_CONTEXT_GOTOURL,
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_GOTOURL,
                                     printable_selection_text));
    }
  }
}

void RenderViewContextMenu::AppendEditableItems() {
  const bool use_spellcheck_and_search = !chrome::IsRunningInForcedAppMode();

  if (use_spellcheck_and_search)
    AppendSpellingSuggestionsSubMenu();

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_UNDO,
                                  IDS_CONTENT_CONTEXT_UNDO);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_REDO,
                                  IDS_CONTENT_CONTEXT_REDO);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_CUT,
                                  IDS_CONTENT_CONTEXT_CUT);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPY,
                                  IDS_CONTENT_CONTEXT_COPY);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_PASTE,
                                  IDS_CONTENT_CONTEXT_PASTE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE,
                                  IDS_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_DELETE,
                                  IDS_CONTENT_CONTEXT_DELETE);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);

  if (use_spellcheck_and_search && !params_.keyword_url.is_empty()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_ADDSEARCHENGINE,
                                    IDS_CONTENT_CONTEXT_ADDSEARCHENGINE);
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (use_spellcheck_and_search)
    AppendSpellcheckOptionsSubMenu();
  AppendPlatformEditableItems();

  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SELECTALL,
                                  IDS_CONTENT_CONTEXT_SELECTALL);
}

void RenderViewContextMenu::AppendSpellingSuggestionsSubMenu() {
  if (!spelling_menu_observer_.get())
    spelling_menu_observer_.reset(new SpellingMenuObserver(this));
  observers_.AddObserver(spelling_menu_observer_.get());
  spelling_menu_observer_->InitMenu(params_);
}

void RenderViewContextMenu::AppendSpellcheckOptionsSubMenu() {
  if (!spellchecker_submenu_observer_.get()) {
    spellchecker_submenu_observer_.reset(new SpellCheckerSubMenuObserver(
        this, this, kSpellcheckRadioGroup));
  }
  spellchecker_submenu_observer_->InitMenu(params_);
  observers_.AddObserver(spellchecker_submenu_observer_.get());
}

void RenderViewContextMenu::AppendProtocolHandlerSubMenu() {
  const ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      GetHandlersForLinkUrl();
  if (handlers.empty())
    return;
  size_t max = IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST -
      IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST;
  for (size_t i = 0; i < handlers.size() && i <= max; i++) {
    protocol_handler_submenu_model_.AddItem(
        IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST + i,
        base::UTF8ToUTF16(handlers[i].url().host()));
  }
  protocol_handler_submenu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  protocol_handler_submenu_model_.AddItem(
      IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_OPENLINKWITH_CONFIGURE));

  menu_model_.AddSubMenu(
      IDC_CONTENT_CONTEXT_OPENLINKWITH,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_OPENLINKWITH),
      &protocol_handler_submenu_model_);
}

void RenderViewContextMenu::AppendPlatformEditableItems() {
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenu::IsCommandIdEnabled(int id) const {
  // If this command is is added by one of our observers, we dispatch it to the
  // observer.
  ObserverListBase<RenderViewContextMenuObserver>::Iterator it(observers_);
  RenderViewContextMenuObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->IsCommandIdSupported(id))
      return observer->IsCommandIdEnabled(id);
  }

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  int content_restrictions = 0;
  if (core_tab_helper)
    content_restrictions = core_tab_helper->content_restrictions();
  if (id == IDC_PRINT && (content_restrictions & CONTENT_RESTRICTION_PRINT))
    return false;

  if (id == IDC_SAVE_PAGE &&
      (content_restrictions & CONTENT_RESTRICTION_SAVE)) {
    return false;
  }

  // Allow Spell Check language items on sub menu for text area context menu.
  if ((id >= IDC_SPELLCHECK_LANGUAGES_FIRST) &&
      (id < IDC_SPELLCHECK_LANGUAGES_LAST)) {
    return profile_->GetPrefs()->GetBoolean(prefs::kEnableContinuousSpellcheck);
  }

  // Custom items.
  if (id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_CONTENT_CONTEXT_CUSTOM_LAST) {
    return IsCustomItemEnabled(params_.custom_items, id);
  }

  // Extension items.
  if (id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return extension_items_.IsCommandIdEnabled(id);
  }

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    return true;
  }

  IncognitoModePrefs::Availability incognito_avail =
      IncognitoModePrefs::GetAvailability(profile_->GetPrefs());
  switch (id) {
    case IDC_BACK:
      return source_web_contents_->GetController().CanGoBack();

    case IDC_FORWARD:
      return source_web_contents_->GetController().CanGoForward();

    case IDC_RELOAD: {
      CoreTabHelper* core_tab_helper =
          CoreTabHelper::FromWebContents(source_web_contents_);
      if (!core_tab_helper)
        return false;

      CoreTabHelperDelegate* core_delegate = core_tab_helper->delegate();
      return !core_delegate ||
             core_delegate->CanReloadContents(source_web_contents_);
    }

    case IDC_VIEW_SOURCE:
    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      return source_web_contents_->GetController().CanViewSource();

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE:
    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP:
    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP:
      return IsDevCommandEnabled(id);

    case IDC_CONTENT_CONTEXT_VIEWPAGEINFO:
      if (source_web_contents_->GetController().GetVisibleEntry() == NULL)
        return false;
      // Disabled if no browser is associated (e.g. desktop notifications).
      if (chrome::FindBrowserWithWebContents(source_web_contents_) == NULL)
        return false;
      return true;

    case IDC_CONTENT_CONTEXT_TRANSLATE: {
      ChromeTranslateClient* chrome_translate_client =
          ChromeTranslateClient::FromWebContents(source_web_contents_);
      if (!chrome_translate_client)
        return false;
      std::string original_lang =
          chrome_translate_client->GetLanguageState().original_language();
      std::string target_lang = g_browser_process->GetApplicationLocale();
      target_lang =
          translate::TranslateDownloadManager::GetLanguageCode(target_lang);
      // Note that we intentionally enable the menu even if the original and
      // target languages are identical.  This is to give a way to user to
      // translate a page that might contains text fragments in a different
      // language.
      return ((params_.edit_flags & WebContextMenuData::CanTranslate) != 0) &&
             !original_lang.empty() &&  // Did we receive the page language yet?
             !chrome_translate_client->GetLanguageState().IsPageTranslated() &&
             !source_web_contents_->GetInterstitialPage() &&
             // There are some application locales which can't be used as a
             // target language for translation.
             translate::TranslateDownloadManager::IsSupportedLanguage(
                 target_lang) &&
             // Disable on the Instant Extended NTP.
             !chrome::IsInstantNTP(source_web_contents_);
    }

    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB:
    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      return params_.link_url.is_valid();

    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
      return params_.unfiltered_link_url.is_valid();

    case IDC_CONTENT_CONTEXT_SAVELINKAS: {
      PrefService* local_state = g_browser_process->local_state();
      DCHECK(local_state);
      // Test if file-selection dialogs are forbidden by policy.
      if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
        return false;

      return params_.link_url.is_valid() &&
          ProfileIOData::IsHandledProtocol(params_.link_url.scheme());
    }

    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS: {
      PrefService* local_state = g_browser_process->local_state();
      DCHECK(local_state);
      // Test if file-selection dialogs are forbidden by policy.
      if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
        return false;

      if (params_.media_type == WebContextMenuData::MediaTypeCanvas)
        return true;

      return params_.src_url.is_valid() &&
          ProfileIOData::IsHandledProtocol(params_.src_url.scheme());
    }

    // The images shown in the most visited thumbnails can't be opened or
    // searched for conventionally.
    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE:
      return params_.src_url.is_valid() &&
          (params_.src_url.scheme() != content::kChromeUIScheme);

    case IDC_CONTENT_CONTEXT_COPYIMAGE:
      return params_.has_image_contents;

    // Media control commands should all be disabled if the player is in an
    // error state.
    case IDC_CONTENT_CONTEXT_PLAYPAUSE:
    case IDC_CONTENT_CONTEXT_LOOP:
      return (params_.media_flags &
              WebContextMenuData::MediaInError) == 0;

    // Mute and unmute should also be disabled if the player has no audio.
    case IDC_CONTENT_CONTEXT_MUTE:
      return (params_.media_flags &
              WebContextMenuData::MediaHasAudio) != 0 &&
             (params_.media_flags &
              WebContextMenuData::MediaInError) == 0;

    case IDC_CONTENT_CONTEXT_CONTROLS:
      return (params_.media_flags &
              WebContextMenuData::MediaCanToggleControls) != 0;

    case IDC_CONTENT_CONTEXT_ROTATECW:
    case IDC_CONTENT_CONTEXT_ROTATECCW:
      return
          (params_.media_flags & WebContextMenuData::MediaCanRotate) != 0;

    case IDC_CONTENT_CONTEXT_COPYAVLOCATION:
    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
      return params_.src_url.is_valid();

    case IDC_CONTENT_CONTEXT_SAVEAVAS: {
      PrefService* local_state = g_browser_process->local_state();
      DCHECK(local_state);
      // Test if file-selection dialogs are forbidden by policy.
      if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
        return false;

      const GURL& url = params_.src_url;
      bool can_save =
          (params_.media_flags & WebContextMenuData::MediaCanSave) &&
          url.is_valid() && ProfileIOData::IsHandledProtocol(url.scheme());
#if defined(ENABLE_FULL_PRINTING)
          // Do not save the preview PDF on the print preview page.
      can_save = can_save &&
          !(printing::PrintPreviewDialogController::IsPrintPreviewURL(url));
#endif
      return can_save;
    }

    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      return true;

    case IDC_SAVE_PAGE: {
      CoreTabHelper* core_tab_helper =
          CoreTabHelper::FromWebContents(source_web_contents_);
      if (!core_tab_helper)
        return false;

      CoreTabHelperDelegate* core_delegate = core_tab_helper->delegate();
      if (core_delegate &&
          !core_delegate->CanSaveContents(source_web_contents_))
        return false;

      PrefService* local_state = g_browser_process->local_state();
      DCHECK(local_state);
      // Test if file-selection dialogs are forbidden by policy.
      if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
        return false;

      // We save the last committed entry (which the user is looking at), as
      // opposed to any pending URL that hasn't committed yet.
      NavigationEntry* entry =
          source_web_contents_->GetController().GetLastCommittedEntry();
      return content::IsSavableURL(entry ? entry->GetURL() : GURL());
    }

    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      return params_.frame_url.is_valid();

    case IDC_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & WebContextMenuData::CanUndo);

    case IDC_CONTENT_CONTEXT_REDO:
      return !!(params_.edit_flags & WebContextMenuData::CanRedo);

    case IDC_CONTENT_CONTEXT_CUT:
      return !!(params_.edit_flags & WebContextMenuData::CanCut);

    case IDC_CONTENT_CONTEXT_COPY:
      return !!(params_.edit_flags & WebContextMenuData::CanCopy);

    case IDC_CONTENT_CONTEXT_PASTE:
    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      return !!(params_.edit_flags & WebContextMenuData::CanPaste);

    case IDC_CONTENT_CONTEXT_DELETE:
      return !!(params_.edit_flags & WebContextMenuData::CanDelete);

    case IDC_CONTENT_CONTEXT_SELECTALL:
      return !!(params_.edit_flags & WebContextMenuData::CanSelectAll);

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      return !profile_->IsOffTheRecord() && params_.link_url.is_valid() &&
             incognito_avail != IncognitoModePrefs::DISABLED;

    case IDC_PRINT:
      return profile_->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
          (params_.media_type == WebContextMenuData::MediaTypeNone ||
           params_.media_flags & WebContextMenuData::MediaCanPrint);

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_CONTENT_CONTEXT_GOTOURL:
    case IDC_SPELLPANEL_TOGGLE:
    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      return true;
    case IDC_CONTENT_CONTEXT_VIEWFRAMEINFO:
      // Disabled if no browser is associated (e.g. desktop notifications).
      if (chrome::FindBrowserWithWebContents(source_web_contents_) == NULL)
        return false;
      return true;

    case IDC_CHECK_SPELLING_WHILE_TYPING:
      return profile_->GetPrefs()->GetBoolean(
          prefs::kEnableContinuousSpellcheck);

#if !defined(OS_MACOSX) && defined(OS_POSIX)
    // TODO(suzhe): this should not be enabled for password fields.
    case IDC_INPUT_METHODS_MENU:
      return true;
#endif

    case IDC_CONTENT_CONTEXT_ADDSEARCHENGINE:
      return !params_.keyword_url.is_empty();

    case IDC_SPELLCHECK_MENU:
      return true;

    case IDC_CONTENT_CONTEXT_OPENLINKWITH:
      return true;

    case IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS:
      return true;

    default:
      NOTREACHED();
      return false;
  }
}

bool RenderViewContextMenu::IsCommandIdChecked(int id) const {
  // If this command is is added by one of our observers, we dispatch it to the
  // observer.
  ObserverListBase<RenderViewContextMenuObserver>::Iterator it(observers_);
  RenderViewContextMenuObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->IsCommandIdSupported(id))
      return observer->IsCommandIdChecked(id);
  }

  // See if the video is set to looping.
  if (id == IDC_CONTENT_CONTEXT_LOOP) {
    return (params_.media_flags &
            WebContextMenuData::MediaLoop) != 0;
  }

  if (id == IDC_CONTENT_CONTEXT_CONTROLS) {
    return (params_.media_flags &
            WebContextMenuData::MediaControls) != 0;
  }

  // Custom items.
  if (id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_CONTENT_CONTEXT_CUSTOM_LAST) {
    return IsCustomItemChecked(params_.custom_items, id);
  }

  // Extension items.
  if (id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return extension_items_.IsCommandIdChecked(id);
  }

  return false;
}

void RenderViewContextMenu::ExecuteCommand(int id, int event_flags) {
  command_executed_ = true;
  RecordUsedItem(id);

  // If this command is is added by one of our observers, we dispatch it to the
  // observer.
  ObserverListBase<RenderViewContextMenuObserver>::Iterator it(observers_);
  RenderViewContextMenuObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->IsCommandIdSupported(id))
      return observer->ExecuteCommand(id);
  }

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id_, render_frame_id_);

  // Process custom actions range.
  if (id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_CONTENT_CONTEXT_CUSTOM_LAST) {
    unsigned action = id - IDC_CONTENT_CONTEXT_CUSTOM_FIRST;
    const content::CustomContextMenuContext& context = params_.custom_context;
#if defined(ENABLE_PLUGINS)
    if (context.request_id && !context.is_pepper_menu) {
      ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
        source_web_contents_, false, std::string());
    }
#endif
    source_web_contents_->ExecuteCustomContextMenuCommand(action, context);
    return;
  }

  // Process extension menu items.
  if (id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    extension_items_.ExecuteCommand(id, source_web_contents_, params_);
    return;
  }

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    ProtocolHandlerRegistry::ProtocolHandlerList handlers =
        GetHandlersForLinkUrl();
    if (handlers.empty()) {
      return;
    }
    content::RecordAction(
        UserMetricsAction("RegisterProtocolHandler.ContextMenu_Open"));
    int handlerIndex = id - IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST;
    WindowOpenDisposition disposition =
        ForceNewTabDispositionFromEventFlags(event_flags);
    OpenURL(
        handlers[handlerIndex].TranslateUrl(params_.link_url),
        params_.frame_url.is_empty() ? params_.page_url : params_.frame_url,
        disposition, content::PAGE_TRANSITION_LINK);
    return;
  }

  switch (id) {
    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB: {
      Browser* browser =
          chrome::FindBrowserWithWebContents(source_web_contents_);
      OpenURL(
          params_.link_url,
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url,
          !browser || browser->is_app() ?
                  NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB,
          content::PAGE_TRANSITION_LINK);
      break;
    }
    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      OpenURL(
          params_.link_url,
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url,
          NEW_WINDOW, content::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      OpenURL(params_.link_url, GURL(), OFF_THE_RECORD,
              content::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_SAVELINKAS: {
      RecordDownloadSource(DOWNLOAD_INITIATED_BY_CONTEXT_MENU);
      const GURL& referrer =
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url;
      const GURL& url = params_.link_url;
      DownloadManager* dlm = BrowserContext::GetDownloadManager(profile_);
      scoped_ptr<DownloadUrlParameters> dl_params(
          DownloadUrlParameters::FromWebContents(source_web_contents_, url));
      dl_params->set_referrer(
          content::Referrer(referrer, params_.referrer_policy));
      dl_params->set_referrer_encoding(params_.frame_charset);
      dl_params->set_suggested_name(params_.suggested_filename);
      dl_params->set_prompt(true);
      dlm->DownloadUrl(dl_params.Pass());
      break;
    }

    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS: {
      if (params_.media_type == WebContextMenuData::MediaTypeCanvas) {
        source_web_contents_->GetRenderViewHost()->SaveImageAt(
          params_.x, params_.y);
      } else {
        // TODO(zino): We can use SaveImageAt() like a case of canvas.
        RecordDownloadSource(DOWNLOAD_INITIATED_BY_CONTEXT_MENU);
        const GURL& referrer =
            params_.frame_url.is_empty() ? params_.page_url : params_.frame_url;
        const GURL& url = params_.src_url;
        source_web_contents_->SaveFrame(url, content::Referrer(
            referrer, params_.referrer_policy));
      }
      break;
    }

    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
      WriteURLToClipboard(params_.unfiltered_link_url);
      break;

    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
    case IDC_CONTENT_CONTEXT_COPYAVLOCATION:
      WriteURLToClipboard(params_.src_url);
      break;

    case IDC_CONTENT_CONTEXT_COPYIMAGE:
      CopyImageAt(params_.x, params_.y);
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE:
      GetImageThumbnailForSearch();
      break;

    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      OpenURL(
          params_.src_url,
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url,
          NEW_BACKGROUND_TAB, content::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_PLAYPAUSE: {
      bool play = !!(params_.media_flags & WebContextMenuData::MediaPaused);
      if (play) {
        content::RecordAction(UserMetricsAction("MediaContextMenu_Play"));
      } else {
        content::RecordAction(UserMetricsAction("MediaContextMenu_Pause"));
      }
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Play, play));
      break;
    }

    case IDC_CONTENT_CONTEXT_MUTE: {
      bool mute = !(params_.media_flags & WebContextMenuData::MediaMuted);
      if (mute) {
        content::RecordAction(UserMetricsAction("MediaContextMenu_Mute"));
      } else {
        content::RecordAction(UserMetricsAction("MediaContextMenu_Unmute"));
      }
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Mute, mute));
      break;
    }

    case IDC_CONTENT_CONTEXT_LOOP:
      content::RecordAction(UserMetricsAction("MediaContextMenu_Loop"));
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Loop,
                              !IsCommandIdChecked(IDC_CONTENT_CONTEXT_LOOP)));
      break;

    case IDC_CONTENT_CONTEXT_CONTROLS:
      content::RecordAction(UserMetricsAction("MediaContextMenu_Controls"));
      MediaPlayerActionAt(
          gfx::Point(params_.x, params_.y),
          WebMediaPlayerAction(
              WebMediaPlayerAction::Controls,
              !IsCommandIdChecked(IDC_CONTENT_CONTEXT_CONTROLS)));
      break;

    case IDC_CONTENT_CONTEXT_ROTATECW:
      content::RecordAction(
      UserMetricsAction("PluginContextMenu_RotateClockwise"));
      PluginActionAt(
          gfx::Point(params_.x, params_.y),
          WebPluginAction(
              WebPluginAction::Rotate90Clockwise,
              true));
      break;

    case IDC_CONTENT_CONTEXT_ROTATECCW:
      content::RecordAction(
      UserMetricsAction("PluginContextMenu_RotateCounterclockwise"));
      PluginActionAt(
          gfx::Point(params_.x, params_.y),
          WebPluginAction(
              WebPluginAction::Rotate90Counterclockwise,
              true));
      break;

    case IDC_BACK:
      source_web_contents_->GetController().GoBack();
      break;

    case IDC_FORWARD:
      source_web_contents_->GetController().GoForward();
      break;

    case IDC_SAVE_PAGE:
      source_web_contents_->OnSavePage();
      break;

    case IDC_RELOAD:
      source_web_contents_->GetController().Reload(true);
      break;

    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP: {
      const Extension* platform_app = GetExtension();
      DCHECK(platform_app);
      DCHECK(platform_app->is_platform_app());

      extensions::ExtensionSystem::Get(profile_)->extension_service()->
          ReloadExtension(platform_app->id());
      break;
    }

    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP: {
      const Extension* platform_app = GetExtension();
      DCHECK(platform_app);
      DCHECK(platform_app->is_platform_app());

      apps::AppLoadService::Get(profile_)->RestartApplication(
          platform_app->id());
      break;
    }

    case IDC_PRINT:
#if defined(ENABLE_PRINTING)
      if (params_.media_type == WebContextMenuData::MediaTypeNone) {
#if defined(ENABLE_FULL_PRINTING)
        printing::PrintViewManager* print_view_manager =
            printing::PrintViewManager::FromWebContents(source_web_contents_);

        if (!print_view_manager)
          break;
        if (profile_->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled)) {
          print_view_manager->PrintNow();
        } else {
          print_view_manager->PrintPreviewNow(!params_.selection_text.empty());
        }
#else
        printing::PrintViewManagerBasic* print_view_manager =
            printing::PrintViewManagerBasic::FromWebContents(
                source_web_contents_);
        if (!print_view_manager)
          break;
        print_view_manager->PrintNow();
#endif  // defined(ENABLE_FULL_PRINTING)
      } else {
        if (render_frame_host) {
          render_frame_host->Send(new PrintMsg_PrintNodeUnderContextMenu(
              render_frame_host->GetRoutingID()));
        }
      }
#endif  // defined(ENABLE_PRINTING)
      break;

    case IDC_VIEW_SOURCE:
      source_web_contents_->ViewSource();
      break;

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
      Inspect(params_.x, params_.y);
      break;

    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE: {
      const Extension* platform_app = GetExtension();
      DCHECK(platform_app);
      DCHECK(platform_app->is_platform_app());

      extensions::devtools_util::InspectBackgroundPage(platform_app, profile_);
      break;
    }

    case IDC_CONTENT_CONTEXT_VIEWPAGEINFO: {
      NavigationController* controller = &source_web_contents_->GetController();
      // Important to use GetVisibleEntry to match what's showing in the
      // omnibox.  This may return null.
      NavigationEntry* nav_entry = controller->GetVisibleEntry();
      if (!nav_entry)
        return;
      Browser* browser =
          chrome::FindBrowserWithWebContents(source_web_contents_);
      chrome::ShowWebsiteSettings(browser, source_web_contents_,
                                  nav_entry->GetURL(), nav_entry->GetSSL());
      break;
    }

    case IDC_CONTENT_CONTEXT_TRANSLATE: {
      // A translation might have been triggered by the time the menu got
      // selected, do nothing in that case.
      ChromeTranslateClient* chrome_translate_client =
          ChromeTranslateClient::FromWebContents(source_web_contents_);
      if (!chrome_translate_client ||
          chrome_translate_client->GetLanguageState().IsPageTranslated() ||
          chrome_translate_client->GetLanguageState().translation_pending()) {
        return;
      }
      std::string original_lang =
          chrome_translate_client->GetLanguageState().original_language();
      std::string target_lang = g_browser_process->GetApplicationLocale();
      target_lang =
          translate::TranslateDownloadManager::GetLanguageCode(target_lang);
      // Since the user decided to translate for that language and site, clears
      // any preferences for not translating them.
      scoped_ptr<translate::TranslatePrefs> prefs(
          ChromeTranslateClient::CreateTranslatePrefs(profile_->GetPrefs()));
      prefs->UnblockLanguage(original_lang);
      prefs->RemoveSiteFromBlacklist(params_.page_url.HostNoBrackets());
      translate::TranslateManager* manager =
          chrome_translate_client->GetTranslateManager();
      DCHECK(manager);
      manager->TranslatePage(original_lang, target_lang, true);
      break;
    }

    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      // We always obey the cache here.
      // TODO(evanm): Perhaps we could allow shift-clicking the menu item to do
      // a cache-ignoring reload of the frame.
      source_web_contents_->ReloadFocusedFrame(false);
      break;

    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      source_web_contents_->ViewFrameSource(params_.frame_url,
                                            params_.frame_page_state);
      break;

    case IDC_CONTENT_CONTEXT_VIEWFRAMEINFO: {
      Browser* browser = chrome::FindBrowserWithWebContents(
          source_web_contents_);
      chrome::ShowWebsiteSettings(browser, source_web_contents_,
                                  params_.frame_url, params_.security_info);
      break;
    }

    case IDC_CONTENT_CONTEXT_UNDO:
      source_web_contents_->Undo();
      break;

    case IDC_CONTENT_CONTEXT_REDO:
      source_web_contents_->Redo();
      break;

    case IDC_CONTENT_CONTEXT_CUT:
      source_web_contents_->Cut();
      break;

    case IDC_CONTENT_CONTEXT_COPY:
      source_web_contents_->Copy();
      break;

    case IDC_CONTENT_CONTEXT_PASTE:
      source_web_contents_->Paste();
      break;

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      source_web_contents_->PasteAndMatchStyle();
      break;

    case IDC_CONTENT_CONTEXT_DELETE:
      source_web_contents_->Delete();
      break;

    case IDC_CONTENT_CONTEXT_SELECTALL:
      source_web_contents_->SelectAll();
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_CONTENT_CONTEXT_GOTOURL: {
      WindowOpenDisposition disposition =
          ForceNewTabDispositionFromEventFlags(event_flags);
      OpenURL(selection_navigation_url_, GURL(), disposition,
              content::PAGE_TRANSITION_LINK);
      break;
    }
    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS: {
      WindowOpenDisposition disposition =
          ForceNewTabDispositionFromEventFlags(event_flags);
      GURL url = chrome::GetSettingsUrl(chrome::kLanguageOptionsSubPage);
      OpenURL(url, GURL(), disposition, content::PAGE_TRANSITION_LINK);
      break;
    }

    case IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS: {
      content::RecordAction(
          UserMetricsAction("RegisterProtocolHandler.ContextMenu_Settings"));
      WindowOpenDisposition disposition =
          ForceNewTabDispositionFromEventFlags(event_flags);
      GURL url = chrome::GetSettingsUrl(chrome::kHandlerSettingsSubPage);
      OpenURL(url, GURL(), disposition, content::PAGE_TRANSITION_LINK);
      break;
    }

    case IDC_CONTENT_CONTEXT_ADDSEARCHENGINE: {
      // Make sure the model is loaded.
      TemplateURLService* model =
          TemplateURLServiceFactory::GetForProfile(profile_);
      if (!model)
        return;
      model->Load();

      SearchEngineTabHelper* search_engine_tab_helper =
          SearchEngineTabHelper::FromWebContents(source_web_contents_);
      if (search_engine_tab_helper &&
          search_engine_tab_helper->delegate()) {
        base::string16 keyword(TemplateURL::GenerateKeyword(params_.page_url));
        TemplateURLData data;
        data.short_name = keyword;
        data.SetKeyword(keyword);
        data.SetURL(params_.keyword_url.spec());
        data.favicon_url =
            TemplateURL::GenerateFaviconURL(params_.page_url.GetOrigin());
        // Takes ownership of the TemplateURL.
        search_engine_tab_helper->delegate()->
            ConfirmAddSearchProvider(new TemplateURL(data), profile_);
      }
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

ProtocolHandlerRegistry::ProtocolHandlerList
    RenderViewContextMenu::GetHandlersForLinkUrl() {
  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      protocol_handler_registry_->GetHandlersFor(params_.link_url.scheme());
  std::sort(handlers.begin(), handlers.end());
  return handlers;
}

void RenderViewContextMenu::MenuWillShow(ui::SimpleMenuModel* source) {
  for (int i = 0; i < source->GetItemCount(); ++i) {
    if (source->IsVisibleAt(i) &&
        source->GetTypeAt(i) != ui::MenuModel::TYPE_SEPARATOR) {
      RecordShownItem(source->GetCommandIdAt(i));
    }
  }

  // Ignore notifications from submenus.
  if (source != &menu_model_)
    return;

  content::RenderWidgetHostView* view =
      source_web_contents_->GetRenderWidgetHostView();
  if (view)
    view->SetShowingContextMenu(true);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN,
      content::Source<RenderViewContextMenu>(this),
      content::NotificationService::NoDetails());
}

void RenderViewContextMenu::MenuClosed(ui::SimpleMenuModel* source) {
  // Ignore notifications from submenus.
  if (source != &menu_model_)
    return;

  content::RenderWidgetHostView* view =
      source_web_contents_->GetRenderWidgetHostView();
  if (view)
    view->SetShowingContextMenu(false);
  source_web_contents_->NotifyContextMenuClosed(params_.custom_context);

  if (!command_executed_) {
    FOR_EACH_OBSERVER(RenderViewContextMenuObserver,
                      observers_,
                      OnMenuCancel());
  }
}

bool RenderViewContextMenu::IsDevCommandEnabled(int id) const {
  if (id == IDC_CONTENT_CONTEXT_INSPECTELEMENT ||
      id == IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE) {
    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (!profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled) ||
        command_line->HasSwitch(switches::kDisableJavaScript))
      return false;

    // Don't enable the web inspector if the developer tools are disabled via
    // the preference dev-tools-disabled.
    if (profile_->GetPrefs()->GetBoolean(prefs::kDevToolsDisabled))
      return false;
  }

  return true;
}

base::string16 RenderViewContextMenu::PrintableSelectionText() {
  return gfx::TruncateString(params_.selection_text,
                            kMaxSelectionTextLength);
}

// Controller functions --------------------------------------------------------

void RenderViewContextMenu::OpenURL(
    const GURL& url, const GURL& referring_url,
    WindowOpenDisposition disposition,
    content::PageTransition transition) {
  content::Referrer referrer(referring_url.GetAsReferrer(),
      params_.referrer_policy);

  WebContents* new_contents = source_web_contents_->OpenURL(OpenURLParams(
      url, referrer, disposition, transition, false));
  if (!new_contents)
    return;

  RetargetingDetails details;
  details.source_web_contents = source_web_contents_;
  details.source_render_frame_id = render_frame_id_;
  details.target_url = url;
  details.target_web_contents = new_contents;
  details.not_yet_in_tabstrip = false;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_RETARGETING,
      content::Source<Profile>(Profile::FromBrowserContext(
          source_web_contents_->GetBrowserContext())),
      content::Details<RetargetingDetails>(&details));
}

void RenderViewContextMenu::CopyImageAt(int x, int y) {
  source_web_contents_->GetRenderViewHost()->CopyImageAt(x, y);
}

void RenderViewContextMenu::GetImageThumbnailForSearch() {
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;
  render_frame_host->Send(new ChromeViewMsg_RequestThumbnailForContextNode(
      render_frame_host->GetRoutingID(),
      kImageSearchThumbnailMinSize,
      gfx::Size(kImageSearchThumbnailMaxWidth,
                kImageSearchThumbnailMaxHeight)));
}

void RenderViewContextMenu::Inspect(int x, int y) {
  content::RecordAction(UserMetricsAction("DevTools_InspectElement"));
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;
  DevToolsWindow::InspectElement(render_frame_host->GetRenderViewHost(), x, y);
}

void RenderViewContextMenu::WriteURLToClipboard(const GURL& url) {
  chrome_common_net::WriteURLToClipboard(
      url,
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
      ui::Clipboard::GetForCurrentThread());
}

void RenderViewContextMenu::MediaPlayerActionAt(
    const gfx::Point& location,
    const WebMediaPlayerAction& action) {
  source_web_contents_->GetRenderViewHost()->
      ExecuteMediaPlayerActionAtLocation(location, action);
}

void RenderViewContextMenu::PluginActionAt(
    const gfx::Point& location,
    const WebPluginAction& action) {
  source_web_contents_->GetRenderViewHost()->
      ExecutePluginActionAtLocation(location, action);
}
