// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/public/pref_member.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/printing/print_preview_context_menu_observer.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/speech/chrome_speech_recognition_preferences.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/browser/tab_contents/spellchecker_submenu_observer.h"
#include "chrome/browser/tab_contents/spelling_menu_observer.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/net/url_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/print_messages.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_restriction.h"
#include "content/public/common/ssl_status.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginAction.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/favicon_size.h"
#include "webkit/glue/webmenuitem.h"

#ifdef FILE_MANAGER_EXTENSION
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#endif

using WebKit::WebContextMenuData;
using WebKit::WebMediaPlayerAction;
using WebKit::WebPluginAction;
using WebKit::WebString;
using WebKit::WebURL;
using content::BrowserContext;
using content::ChildProcessSecurityPolicy;
using content::DownloadManager;
using content::DownloadUrlParameters;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderViewHost;
using content::SSLStatus;
using content::UserMetricsAction;
using content::WebContents;
using extensions::Extension;
using extensions::MenuItem;
using extensions::MenuManager;

namespace {

// Usually a new tab is expected where this function is used,
// however users should be able to open a tab in background
// or in a new window.
WindowOpenDisposition ForceNewTabDispositionFromEventFlags(
    int event_flags) {
  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  return disposition == CURRENT_TAB ? NEW_FOREGROUND_TAB : disposition;
}

bool IsCustomItemEnabled(const std::vector<WebMenuItem>& items, int id) {
  DCHECK(id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST &&
         id <= IDC_CONTENT_CONTEXT_CUSTOM_LAST);
  for (size_t i = 0; i < items.size(); ++i) {
    int action_id = IDC_CONTENT_CONTEXT_CUSTOM_FIRST + items[i].action;
    if (action_id == id)
      return items[i].enabled;
    if (items[i].type == WebMenuItem::SUBMENU) {
      if (IsCustomItemEnabled(items[i].submenu, id))
        return true;
    }
  }
  return false;
}

bool IsCustomItemChecked(const std::vector<WebMenuItem>& items, int id) {
  DCHECK(id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST &&
         id <= IDC_CONTENT_CONTEXT_CUSTOM_LAST);
  for (size_t i = 0; i < items.size(); ++i) {
    int action_id = IDC_CONTENT_CONTEXT_CUSTOM_FIRST + items[i].action;
    if (action_id == id)
      return items[i].checked;
    if (items[i].type == WebMenuItem::SUBMENU) {
      if (IsCustomItemChecked(items[i].submenu, id))
        return true;
    }
  }
  return false;
}

const size_t kMaxCustomMenuDepth = 5;
const size_t kMaxCustomMenuTotalItems = 1000;

void AddCustomItemsToMenu(const std::vector<WebMenuItem>& items,
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
      case WebMenuItem::OPTION:
        menu_model->AddItem(
            items[i].action + IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
            items[i].label);
        break;
      case WebMenuItem::CHECKABLE_OPTION:
        menu_model->AddCheckItem(
            items[i].action + IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
            items[i].label);
        break;
      case WebMenuItem::GROUP:
        // TODO(viettrungluu): I don't know what this is supposed to do.
        NOTREACHED();
        break;
      case WebMenuItem::SEPARATOR:
        menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
        break;
      case WebMenuItem::SUBMENU: {
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

bool ShouldShowTranslateItem(const GURL& page_url) {
  if (page_url.SchemeIs("chrome"))
    return false;

#ifdef FILE_MANAGER_EXTENSION
  if (page_url.SchemeIs("chrome-extension") &&
      page_url.DomainIs(kFileBrowserDomain))
    return false;
#endif

  return true;
}

void DevToolsInspectElementAt(RenderViewHost* rvh, int x, int y) {
  DevToolsWindow::InspectElement(rvh, x, y);
}

// Helper function to escape "&" as "&&".
void EscapeAmpersands(string16& text) {
  const char16 ampersand[] = {'&', 0};
  ReplaceChars(text, ampersand, ASCIIToUTF16("&&"), &text);
}

}  // namespace

// static
const size_t RenderViewContextMenu::kMaxSelectionTextLength = 50;

// static
bool RenderViewContextMenu::IsDevToolsURL(const GURL& url) {
  return url.SchemeIs(chrome::kChromeDevToolsScheme) &&
      url.host() == chrome::kChromeUIDevToolsHost;
}

// static
bool RenderViewContextMenu::IsInternalResourcesURL(const GURL& url) {
  if (!url.SchemeIs(chrome::kChromeUIScheme))
    return false;
  return url.host() == chrome::kChromeUISyncResourcesHost;
}

static const int kSpellcheckRadioGroup = 1;

RenderViewContextMenu::RenderViewContextMenu(
    WebContents* web_contents,
    const content::ContextMenuParams& params)
    : params_(params),
      source_web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      ALLOW_THIS_IN_INITIALIZER_LIST(menu_model_(this)),
      extension_items_(profile_, this, &menu_model_,
                    base::Bind(MenuItemMatchesParams, params_)),
      external_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(speech_input_submenu_model_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(protocol_handler_submenu_model_(this)),
      protocol_handler_registry_(profile_->GetProtocolHandlerRegistry()) {
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
  bool has_link = !params.link_url.is_empty();
  bool has_selection = !params.selection_text.empty();
  bool in_frame = !params.frame_url.is_empty();

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
  MenuManager* menu_manager = service->menu_manager();

  // Get a list of extension id's that have context menu items, and sort it by
  // the extension's name.
  std::set<std::string> ids = menu_manager->ExtensionIds();
  std::vector<std::pair<std::string, std::string> > sorted_ids;
  for (std::set<std::string>::iterator i = ids.begin(); i != ids.end(); ++i) {
    const Extension* extension = service->GetExtensionById(*i, false);
    // Platform apps have their context menus created directly in
    // AppendPlatformAppItems.
    if (extension && !extension->is_platform_app())
      sorted_ids.push_back(
          std::pair<std::string, std::string>(extension->name(), *i));
  }
  // TODO(asargent) - See if this works properly for i18n names (bug 32363).
  std::sort(sorted_ids.begin(), sorted_ids.end());

  if (sorted_ids.empty())
    return;

  int index = 0;
  base::TimeTicks begin = base::TimeTicks::Now();
  std::vector<std::pair<std::string, std::string> >::const_iterator i;
  for (i = sorted_ids.begin();
       i != sorted_ids.end(); ++i) {
    string16 printable_selection_text = PrintableSelectionText();
    EscapeAmpersands(printable_selection_text);

    extension_items_.AppendExtensionItems(i->second, printable_selection_text,
                                          &index);
  }
  UMA_HISTOGRAM_TIMES("Extensions.ContextMenus_BuildTime",
                      base::TimeTicks::Now() - begin);
  UMA_HISTOGRAM_COUNTS("Extensions.ContextMenus_ItemCount", index);
}

void RenderViewContextMenu::InitMenu() {
  chrome::ViewType view_type = chrome::GetViewType(source_web_contents_);
  if (view_type == chrome::VIEW_TYPE_APP_SHELL) {
    AppendPlatformAppItems();
    return;
  } else if (view_type == chrome::VIEW_TYPE_EXTENSION_POPUP) {
    AppendPopupExtensionItems();
    return;
  } else if (view_type == chrome::VIEW_TYPE_PANEL) {
    AppendPanelItems();
    return;
  }

  bool has_link = !params_.unfiltered_link_url.is_empty();
  bool has_selection = !params_.selection_text.empty();

  if (AppendCustomItems()) {
    // If there's a selection, don't early return when there are custom items,
    // but fall through to adding the normal ones after the custom ones.
    if (has_selection) {
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    } else {
      // Don't add items for Pepper menu.
      if (!params_.custom_context.is_pepper_menu)
        AppendDeveloperItems();
      return;
    }
  }

  // When no special node or text is selected and selection has no link,
  // show page items.
  if (params_.media_type == WebContextMenuData::MediaTypeNone &&
      !has_link &&
      !params_.is_editable &&
      !has_selection) {
    if (!params_.page_url.is_empty()) {
      bool is_devtools = IsDevToolsURL(params_.page_url);
      if (!is_devtools && !IsInternalResourcesURL(params_.page_url)) {
        AppendPageItems();
        // Merge in frame items if we clicked within a frame that needs them.
        if (!params_.frame_url.is_empty()) {
          is_devtools = IsDevToolsURL(params_.frame_url);
          if (!is_devtools && !IsInternalResourcesURL(params_.frame_url)) {
            menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
            AppendFrameItems();
          }
        }
      }
    } else {
      DCHECK(params_.frame_url.is_empty());
    }
  }

  if (has_link) {
    AppendLinkItems();
    if (params_.media_type != WebContextMenuData::MediaTypeNone)
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  switch (params_.media_type) {
    case WebContextMenuData::MediaTypeNone:
      break;
    case WebContextMenuData::MediaTypeImage:
      AppendImageItems();
      break;
    case WebContextMenuData::MediaTypeVideo:
      AppendVideoItems();
      break;
    case WebContextMenuData::MediaTypeAudio:
      AppendAudioItems();
      break;
    case WebContextMenuData::MediaTypePlugin:
      AppendPluginItems();
      break;
#ifdef WEBCONTEXT_MEDIATYPEFILE_DEFINED
    case WebContextMenuData::MediaTypeFile:
      break;
#endif
  }

  if (params_.is_editable)
    AppendEditableItems();
  else if (has_selection)
    AppendCopyItem();

  if (has_selection) {
    if (!IsDevToolsURL(params_.page_url))
      AppendPrintItem();
    AppendSearchProvider();
  }

  if (!IsDevToolsURL(params_.page_url))
    AppendAllExtensionItems();

  AppendDeveloperItems();

  if (!print_preview_menu_observer_.get()) {
    print_preview_menu_observer_.reset(
        new PrintPreviewContextMenuObserver(source_web_contents_));
  }
  observers_.AddObserver(print_preview_menu_observer_.get());
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

void RenderViewContextMenu::AppendPlatformAppItems() {
  const Extension* platform_app = GetExtension();

  // The RVH might be for a process sandboxed from the extension.
  if (!platform_app)
    return;

  DCHECK(platform_app->is_platform_app());

  bool has_selection = !params_.selection_text.empty();

  // Add undo/redo, cut/copy/paste etc for text fields.
  if (params_.is_editable)
    AppendEditableItems();
  else if (has_selection)
    AppendCopyItem();

  int index = 0;
  extension_items_.AppendExtensionItems(platform_app->id(),
                                        PrintableSelectionText(), &index);

  // Add dev tools for unpacked extensions.
  if (extensions::Manifest::IsUnpackedLocation(platform_app->location()) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDebugPackedApps)) {
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
}

void RenderViewContextMenu::AppendPopupExtensionItems() {
  bool has_selection = !params_.selection_text.empty();

  if (params_.is_editable)
    AppendEditableItems();
  else if (has_selection)
    AppendCopyItem();

  if (has_selection)
    AppendSearchProvider();

  AppendAllExtensionItems();
  AppendDeveloperItems();
}

void RenderViewContextMenu::AppendPanelItems() {
  const Extension* extension = GetExtension();

  bool has_selection = !params_.selection_text.empty();

  if (params_.is_editable)
    AppendEditableItems();
  else if (has_selection)
    AppendCopyItem();

  // Only add extension items from this extension.
  int index = 0;
  extension_items_.AppendExtensionItems(extension->id(),
                                        PrintableSelectionText(), &index);
}

void RenderViewContextMenu::AddMenuItem(int command_id,
                                        const string16& title) {
  menu_model_.AddItem(command_id, title);
}

void RenderViewContextMenu::AddCheckItem(int command_id,
                                         const string16& title) {
  menu_model_.AddCheckItem(command_id, title);
}

void RenderViewContextMenu::AddSeparator() {
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AddSubMenu(int command_id,
                                       const string16& label,
                                       ui::MenuModel* model) {
  menu_model_.AddSubMenu(command_id, label, model);
}

void RenderViewContextMenu::UpdateMenuItem(int command_id,
                                           bool enabled,
                                           bool hidden,
                                           const string16& label) {
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

void RenderViewContextMenu::AppendLinkItems() {
  if (!params_.link_url.is_empty()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                    IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                                    IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
    if (params_.link_url.is_valid()) {
      AppendProtocolHandlerSubMenu();
    }

    if (!external_) {
      menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
                                      IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
    }
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVELINKAS,
                                    IDS_CONTENT_CONTEXT_SAVELINKAS);
  }

  menu_model_.AddItemWithStringId(
      IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
      params_.link_url.SchemeIs(chrome::kMailToScheme) ?
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

  if (ShouldShowTranslateItem(params_.page_url)) {
    std::string locale = g_browser_process->GetApplicationLocale();
    locale = TranslateManager::GetLanguageCode(locale);
    string16 language = l10n_util::GetDisplayNameForLocale(locale, locale,
                                                           true);
    menu_model_.AddItem(
        IDC_CONTENT_CONTEXT_TRANSLATE,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_TRANSLATE, language));
  }

  menu_model_.AddItemWithStringId(IDC_VIEW_SOURCE,
                                  IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  // Only add View Page Info if there's a browser.  This is a temporary thing
  // while View Page Info crashes Chrome Frame; see http://crbug.com/120901.
  // TODO(grt) Remove this once page info is back for Chrome Frame.
  if (!external_) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_VIEWPAGEINFO,
                                    IDS_CONTENT_CONTEXT_VIEWPAGEINFO);
  }
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
  // Only add View Frame Info if there's a browser.  This is a temporary thing
  // while View Frame Info crashes Chrome Frame; see http://crbug.com/120901.
  // TODO(grt) Remove this once frame info is back for Chrome Frame.
  if (!external_) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_VIEWFRAMEINFO,
                                    IDS_CONTENT_CONTEXT_VIEWFRAMEINFO);
  }
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

  TrimWhitespace(params_.selection_text, TRIM_ALL, &params_.selection_text);
  if (params_.selection_text.empty())
    return;

  ReplaceChars(params_.selection_text, AutocompleteMatch::kInvalidChars,
               ASCIIToUTF16(" "), &params_.selection_text);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(profile_)->Classify(
      params_.selection_text, string16(), false, false, &match, NULL);
  selection_navigation_url_ = match.destination_url;
  if (!selection_navigation_url_.is_valid())
    return;

  string16 printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(printable_selection_text);

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

  if (!params_.keyword_url.is_empty()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_ADDSEARCHENGINE,
                                    IDS_CONTENT_CONTEXT_ADDSEARCHENGINE);
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  AppendSpellcheckOptionsSubMenu();
  AppendSpeechInputOptionsSubMenu();
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

void RenderViewContextMenu::AppendSpeechInputOptionsSubMenu() {
  if (params_.speech_input_enabled) {
    speech_input_submenu_model_.AddCheckItem(
        IDC_CONTENT_CONTEXT_SPEECH_INPUT_FILTER_PROFANITIES,
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_SPEECH_INPUT_FILTER_PROFANITIES));

    speech_input_submenu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_SPEECH_INPUT_ABOUT,
        IDS_CONTENT_CONTEXT_SPEECH_INPUT_ABOUT);

    menu_model_.AddSubMenu(
        IDC_SPEECH_INPUT_MENU,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPEECH_INPUT_MENU),
        &speech_input_submenu_model_);
  }
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
        handlers[i].title());
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

  if (id == IDC_PRINT &&
      (source_web_contents_->GetContentRestrictions() &
          content::CONTENT_RESTRICTION_PRINT)) {
    return false;
  }

  if (id == IDC_SAVE_PAGE &&
      (source_web_contents_->GetContentRestrictions() &
          content::CONTENT_RESTRICTION_SAVE)) {
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
      if (source_web_contents_->GetController().GetActiveEntry() == NULL)
        return false;
      // Disabled if no browser is associated (e.g. desktop notifications).
      if (chrome::FindBrowserWithWebContents(source_web_contents_) == NULL)
        return false;
      return true;

    case IDC_CONTENT_CONTEXT_TRANSLATE: {
      TranslateTabHelper* translate_tab_helper =
          TranslateTabHelper::FromWebContents(source_web_contents_);
      if (!translate_tab_helper)
        return false;
      std::string original_lang =
          translate_tab_helper->language_state().original_language();
      std::string target_lang = g_browser_process->GetApplicationLocale();
      target_lang = TranslateManager::GetLanguageCode(target_lang);
      // Note that we intentionally enable the menu even if the original and
      // target languages are identical.  This is to give a way to user to
      // translate a page that might contains text fragments in a different
      // language.
      return !!(params_.edit_flags & WebContextMenuData::CanTranslate) &&
             translate_tab_helper->language_state().page_translatable() &&
             !original_lang.empty() &&  // Did we receive the page language yet?
             // Only allow translating languages we explitly support and the
             // unknown language (in which case the page language is detected on
             // the server side).
             (original_lang == chrome::kUnknownLanguageCode ||
                 TranslateManager::IsSupportedLanguage(original_lang)) &&
             !translate_tab_helper->language_state().IsPageTranslated() &&
             !source_web_contents_->GetInterstitialPage() &&
             TranslateManager::IsTranslatableURL(params_.page_url) &&
             // There are some application locales which can't be used as a
             // target language for translation.
             TranslateManager::IsSupportedLanguage(target_lang);
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

      return params_.src_url.is_valid() &&
          ProfileIOData::IsHandledProtocol(params_.src_url.scheme());
    }

    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
      // The images shown in the most visited thumbnails do not currently open
      // in a new tab as they should. Disabling this context menu option for
      // now, as a quick hack, before we resolve this issue (Issue = 2608).
      // TODO(sidchat): Enable this option once this issue is resolved.
      if (params_.src_url.scheme() == chrome::kChromeUIScheme ||
          !params_.src_url.is_valid())
        return false;
      return true;

    case IDC_CONTENT_CONTEXT_COPYIMAGE:
      return !params_.is_image_blocked;

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

    // Media controls can be toggled only for video player. If we toggle
    // controls for audio then the player disappears, and there is no way to
    // return it back.
    case IDC_CONTENT_CONTEXT_CONTROLS:
      return (params_.media_flags &
              WebContextMenuData::MediaHasVideo) != 0;

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
      return (params_.media_flags & WebContextMenuData::MediaCanSave) &&
          url.is_valid() && ProfileIOData::IsHandledProtocol(url.scheme()) &&
          // Do not save the preview PDF on the print preview page.
          !(printing::PrintPreviewDialogController::IsPrintPreviewURL(url));
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

      // Instead of using GetURL here, we use url() (which is the "real" url of
      // the page) from the NavigationEntry because its reflects their origin
      // rather than the display one (returned by GetURL) which may be
      // different (like having "view-source:" on the front).
      NavigationEntry* active_entry =
          source_web_contents_->GetController().GetActiveEntry();
      return download_util::IsSavableURL(
          (active_entry) ? active_entry->GetURL() : GURL());
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

    case IDC_CONTENT_CONTEXT_SPEECH_INPUT_FILTER_PROFANITIES:
    case IDC_CONTENT_CONTEXT_SPEECH_INPUT_ABOUT:
    case IDC_SPEECH_INPUT_MENU:
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

#if defined(ENABLE_INPUT_SPEECH)
  // Check box for menu item 'Block offensive words'.
  if (id == IDC_CONTENT_CONTEXT_SPEECH_INPUT_FILTER_PROFANITIES) {
    return ChromeSpeechRecognitionPreferences::GetForProfile(profile_)->
        FilterProfanities();
  }
#endif

  return false;
}

void RenderViewContextMenu::ExecuteCommand(int id) {
  ExecuteCommand(id, 0);
}

void RenderViewContextMenu::ExecuteCommand(int id, int event_flags) {
  // If this command is is added by one of our observers, we dispatch it to the
  // observer.
  ObserverListBase<RenderViewContextMenuObserver>::Iterator it(observers_);
  RenderViewContextMenuObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->IsCommandIdSupported(id))
      return observer->ExecuteCommand(id);
  }

  RenderViewHost* rvh = source_web_contents_->GetRenderViewHost();

  // Process custom actions range.
  if (id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_CONTENT_CONTEXT_CUSTOM_LAST) {
    unsigned action = id - IDC_CONTENT_CONTEXT_CUSTOM_FIRST;
    const content::CustomContextMenuContext& context = params_.custom_context;
#if defined(ENABLE_PLUGINS)
    if (context.request_id && !context.is_pepper_menu) {
      ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
        rvh->GetProcess()->GetID());
    }
#endif
    rvh->ExecuteCustomContextMenuCommand(action, context);
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
        params_.frame_id,
        disposition,
        content::PAGE_TRANSITION_LINK);
    return;
  }

  switch (id) {
    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB: {
      Browser* browser =
          chrome::FindBrowserWithWebContents(source_web_contents_);
      OpenURL(
          params_.link_url,
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url,
          params_.frame_id,
          !browser || browser->is_app() ?
                  NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB,
          content::PAGE_TRANSITION_LINK);
      break;
    }
    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      OpenURL(
          params_.link_url,
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url,
          params_.frame_id,
          NEW_WINDOW, content::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      OpenURL(params_.link_url,
              GURL(),
              params_.frame_id,
              OFF_THE_RECORD,
              content::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_SAVELINKAS: {
      download_util::RecordDownloadSource(
          download_util::INITIATED_BY_CONTEXT_MENU);
      const GURL& referrer =
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url;
      const GURL& url = params_.link_url;
      DownloadManager* dlm = BrowserContext::GetDownloadManager(profile_);
      scoped_ptr<DownloadUrlParameters> dl_params(
          DownloadUrlParameters::FromWebContents(source_web_contents_, url));
      dl_params->set_referrer(
          content::Referrer(referrer, params_.referrer_policy));
      dl_params->set_referrer_encoding(params_.frame_charset);
      dl_params->set_prompt(true);
      dlm->DownloadUrl(dl_params.Pass());
      break;
    }

    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS: {
      download_util::RecordDownloadSource(
          download_util::INITIATED_BY_CONTEXT_MENU);
      const GURL& referrer =
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url;
      const GURL& url = params_.src_url;
      int64 post_id = -1;
      if (url == source_web_contents_->GetURL()) {
        const NavigationEntry* entry =
            source_web_contents_->GetController().GetActiveEntry();
        if (entry)
          post_id = entry->GetPostID();
      }
      DownloadManager* dlm = BrowserContext::GetDownloadManager(profile_);
      scoped_ptr<DownloadUrlParameters> dl_params(
          DownloadUrlParameters::FromWebContents(source_web_contents_, url));
      dl_params->set_referrer(
          content::Referrer(referrer, params_.referrer_policy));
      dl_params->set_post_id(post_id);
      dl_params->set_prefer_cache(true);
      if (post_id >= 0)
        dl_params->set_method("POST");
      dl_params->set_prompt(true);
      dlm->DownloadUrl(dl_params.Pass());
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

    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      OpenURL(
          params_.src_url,
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url,
          params_.frame_id,
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
      // Prevent the modal "Resubmit form post" dialog from appearing in the
      // context of an external context menu.
      source_web_contents_->GetController().Reload(!external_);
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

      extensions::ExtensionSystem::Get(profile_)->extension_service()->
          RestartExtension(platform_app->id());
      break;
    }

    case IDC_PRINT:
      if (params_.media_type == WebContextMenuData::MediaTypeNone) {
        printing::PrintViewManager* print_view_manager =
            printing::PrintViewManager::FromWebContents(source_web_contents_);
        if (!print_view_manager)
          break;
        if (profile_->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled)) {
          print_view_manager->PrintNow();
        } else {
          print_view_manager->PrintPreviewNow(!params_.selection_text.empty());
        }
      } else {
        rvh->Send(new PrintMsg_PrintNodeUnderContextMenu(rvh->GetRoutingID()));
      }
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

      extensions::ExtensionSystem::Get(profile_)->extension_service()->
          InspectBackgroundPage(platform_app);
      break;
    }

    case IDC_CONTENT_CONTEXT_VIEWPAGEINFO: {
      NavigationController* controller = &source_web_contents_->GetController();
      NavigationEntry* nav_entry = controller->GetActiveEntry();
      Browser* browser =
          chrome::FindBrowserWithWebContents(source_web_contents_);
      chrome::ShowWebsiteSettings(browser, source_web_contents_,
                                  nav_entry->GetURL(), nav_entry->GetSSL(),
                                  true);
      break;
    }

    case IDC_CONTENT_CONTEXT_TRANSLATE: {
      // A translation might have been triggered by the time the menu got
      // selected, do nothing in that case.
      TranslateTabHelper* translate_tab_helper =
          TranslateTabHelper::FromWebContents(source_web_contents_);
      if (!translate_tab_helper ||
          translate_tab_helper->language_state().IsPageTranslated() ||
          translate_tab_helper->language_state().translation_pending()) {
        return;
      }
      std::string original_lang =
          translate_tab_helper->language_state().original_language();
      std::string target_lang = g_browser_process->GetApplicationLocale();
      target_lang = TranslateManager::GetLanguageCode(target_lang);
      // Since the user decided to translate for that language and site, clears
      // any preferences for not translating them.
      TranslatePrefs prefs(profile_->GetPrefs());
      prefs.RemoveLanguageFromBlacklist(original_lang);
      prefs.RemoveSiteFromBlacklist(params_.page_url.HostNoBrackets());
      TranslateManager::GetInstance()->TranslatePage(
          source_web_contents_, original_lang, target_lang);
      break;
    }

    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      rvh->ReloadFrame();
      break;

    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      source_web_contents_->ViewFrameSource(params_.frame_url,
                                            params_.frame_content_state);
      break;

    case IDC_CONTENT_CONTEXT_VIEWFRAMEINFO: {
      Browser* browser = chrome::FindBrowserWithWebContents(
          source_web_contents_);
      chrome::ShowWebsiteSettings(browser, source_web_contents_,
                                  params_.frame_url, params_.security_info,
                                  false);
      break;
    }

    case IDC_CONTENT_CONTEXT_UNDO:
      rvh->Undo();
      break;

    case IDC_CONTENT_CONTEXT_REDO:
      rvh->Redo();
      break;

    case IDC_CONTENT_CONTEXT_CUT:
      rvh->Cut();
      break;

    case IDC_CONTENT_CONTEXT_COPY:
      rvh->Copy();
      break;

    case IDC_CONTENT_CONTEXT_PASTE:
      rvh->Paste();
      break;

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      rvh->PasteAndMatchStyle();
      break;

    case IDC_CONTENT_CONTEXT_DELETE:
      rvh->Delete();
      break;

    case IDC_CONTENT_CONTEXT_SELECTALL:
      rvh->SelectAll();
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_CONTENT_CONTEXT_GOTOURL: {
      WindowOpenDisposition disposition =
          ForceNewTabDispositionFromEventFlags(event_flags);
      OpenURL(selection_navigation_url_,
              GURL(),
              params_.frame_id,
              disposition,
              content::PAGE_TRANSITION_LINK);
      break;
    }
    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS: {
      WindowOpenDisposition disposition =
          ForceNewTabDispositionFromEventFlags(event_flags);
      std::string url = std::string(chrome::kChromeUISettingsURL) +
          chrome::kLanguageOptionsSubPage;
      OpenURL(GURL(url), GURL(), 0, disposition, content::PAGE_TRANSITION_LINK);
      break;
    }

    case IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS: {
      content::RecordAction(
          UserMetricsAction("RegisterProtocolHandler.ContextMenu_Settings"));
      WindowOpenDisposition disposition =
          ForceNewTabDispositionFromEventFlags(event_flags);
      std::string url = std::string(chrome::kChromeUISettingsURL) +
          chrome::kHandlerSettingsSubPage;
      OpenURL(GURL(url), GURL(), 0, disposition, content::PAGE_TRANSITION_LINK);
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
        string16 keyword(TemplateURLService::GenerateKeyword(params_.page_url));
        TemplateURLData data;
        data.short_name = keyword;
        data.SetKeyword(keyword);
        data.SetURL(params_.keyword_url.spec());
        data.favicon_url =
            TemplateURL::GenerateFaviconURL(params_.page_url.GetOrigin());
        // Takes ownership of the TemplateURL.
        search_engine_tab_helper->delegate()->
            ConfirmAddSearchProvider(new TemplateURL(profile_, data), profile_);
      }
      break;
    }

#if defined(ENABLE_INPUT_SPEECH)
    case IDC_CONTENT_CONTEXT_SPEECH_INPUT_FILTER_PROFANITIES: {
      ChromeSpeechRecognitionPreferences::GetForProfile(profile_)->
          ToggleFilterProfanities();
      break;
    }
#endif
    case IDC_CONTENT_CONTEXT_SPEECH_INPUT_ABOUT: {
      GURL url(chrome::kSpeechInputAboutURL);
      GURL localized_url = google_util::AppendGoogleLocaleParam(url);
      // Open URL with no referrer field (because user clicked on menu item).
      OpenURL(localized_url, GURL(), 0, NEW_FOREGROUND_TAB,
          content::PAGE_TRANSITION_LINK);
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
  RenderViewHost* rvh = source_web_contents_->GetRenderViewHost();
  if (rvh) {
    rvh->NotifyContextMenuClosed(params_.custom_context);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_CLOSED,
      content::Source<RenderViewContextMenu>(this),
      content::NotificationService::NoDetails());
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

string16 RenderViewContextMenu::PrintableSelectionText() {
  return ui::TruncateString(params_.selection_text,
                            kMaxSelectionTextLength);
}

// Controller functions --------------------------------------------------------

void RenderViewContextMenu::OpenURL(
    const GURL& url, const GURL& referrer, int64 frame_id,
    WindowOpenDisposition disposition,
    content::PageTransition transition) {
  WebContents* new_contents = source_web_contents_->OpenURL(OpenURLParams(
      url, content::Referrer(referrer, params_.referrer_policy), disposition,
      transition, false));
  if (!new_contents)
    return;

  RetargetingDetails details;
  details.source_web_contents = source_web_contents_;
  details.source_frame_id = frame_id;
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

void RenderViewContextMenu::Inspect(int x, int y) {
  content::RecordAction(UserMetricsAction("DevTools_InspectElement"));
  source_web_contents_->GetRenderViewHostAtPosition(
      x, y, base::Bind(&DevToolsInspectElementAt));
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
