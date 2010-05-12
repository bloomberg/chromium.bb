// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "chrome/browser/tab_contents/render_view_context_menu.h"

#include "app/clipboard/clipboard.h"
#include "app/clipboard/scoped_clipboard_writer.h"
#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/extensions/extension_menu_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/fonts_languages_window.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/spellcheck_host.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/webmenuitem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/WebKit/WebKit/chromium/public/WebTextDirection.h"

using WebKit::WebContextMenuData;
using WebKit::WebMediaPlayerAction;

// static
bool RenderViewContextMenu::IsDevToolsURL(const GURL& url) {
  return url.SchemeIs(chrome::kChromeUIScheme) &&
      url.host() == chrome::kChromeUIDevToolsHost;
}

static const int kSpellcheckRadioGroup = 1;
static const int kExtensionsRadioGroup = 2;

RenderViewContextMenu::RenderViewContextMenu(
    TabContents* tab_contents,
    const ContextMenuParams& params)
    : params_(params),
      source_tab_contents_(tab_contents),
      profile_(tab_contents->profile()),
      ALLOW_THIS_IN_INITIALIZER_LIST(menu_model_(this)),
      external_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(spellcheck_submenu_model_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(bidi_submenu_model_(this)) {
}

RenderViewContextMenu::~RenderViewContextMenu() {
}

// Menu construction functions -------------------------------------------------

void RenderViewContextMenu::Init() {
  InitMenu();
  PlatformInit();
}

static bool ExtensionContextMatch(ContextMenuParams params,
                                  ExtensionMenuItem::ContextList contexts) {
  bool has_link = !params.link_url.is_empty();
  bool has_selection = !params.selection_text.empty();

  if (contexts.Contains(ExtensionMenuItem::ALL) ||
      (has_selection && contexts.Contains(ExtensionMenuItem::SELECTION)) ||
      (has_link && contexts.Contains(ExtensionMenuItem::LINK)) ||
      (params.is_editable && contexts.Contains(ExtensionMenuItem::EDITABLE))) {
    return true;
  }

  switch (params.media_type) {
    case WebContextMenuData::MediaTypeImage:
      return contexts.Contains(ExtensionMenuItem::IMAGE);

    case WebContextMenuData::MediaTypeVideo:
      return contexts.Contains(ExtensionMenuItem::VIDEO);

    case WebContextMenuData::MediaTypeAudio:
      return contexts.Contains(ExtensionMenuItem::AUDIO);

    default:
      break;
  }

  // PAGE is the least specific context, so we only examine that if none of the
  // other contexts apply.
  if (!has_link && !has_selection && !params.is_editable &&
      params.media_type == WebContextMenuData::MediaTypeNone &&
      contexts.Contains(ExtensionMenuItem::PAGE))
    return true;

  return false;
}

void RenderViewContextMenu::GetItemsForExtension(
    const std::string& extension_id,
    std::vector<const ExtensionMenuItem*>* items) {
  ExtensionsService* service = profile_->GetExtensionsService();

  // Get the set of possible items, and iterate to find which ones are
  // applicable.
  std::vector<const ExtensionMenuItem*> potential_items =
      service->menu_manager()->MenuItems(extension_id);

  std::vector<const ExtensionMenuItem*>::const_iterator i;
  for (i = potential_items.begin(); i != potential_items.end(); ++i) {
    const ExtensionMenuItem* item = *i;
    if (ExtensionContextMatch(params_, item->contexts()))
      items->push_back(item);
  }
}

void RenderViewContextMenu::AppendExtensionItems(
    const std::string& extension_id, int* index) {
  Extension* extension =
      profile_->GetExtensionsService()->GetExtensionById(extension_id, false);
  DCHECK_GE(*index, 0);
  int max_index =
      IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST - IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST;
  if (!extension || *index >= max_index)
    return;

  std::vector<const ExtensionMenuItem*> items;
  GetItemsForExtension(extension_id, &items);
  if (items.empty())
    return;

  string16 selection_text = PrintableSelectionText();

  // If this is the first extension-provided menu item, add a separator.
  if (*index == 0)
    menu_model_.AddSeparator();

  // When extensions have more than 1 top-level item or a single parent item
  // with children, we will start a sub menu. In the case of 1 parent with
  // children, we will remove the parent from |items| and insert the children
  // into it. The |index| parameter is incremented if we start a submenu. This
  // returns true if a submenu was started. If we had multiple top-level items
  // that needed to be pushed into a submenu, we'll use |extension_name| as the
  // title.
  menus::SimpleMenuModel* menu_model;
  if (items.empty() || (items.size() == 1 && items[0]->child_count() == 0)) {
    menu_model = &menu_model_;
  } else {
    menu_model = new menus::SimpleMenuModel(this);
    extension_menu_models_.push_back(menu_model);

    int menu_id = IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + (*index)++;
    string16 title;
    if (items[0]->child_count() > 0) {
      title = items[0]->TitleWithReplacement(selection_text);
      extension_item_map_[menu_id] = items[0]->id();
    } else {
      title = UTF8ToUTF16(extension->name());
    }

    menu_model_.AddSubMenu(menu_id, title, menu_model);

    // If we have 1 parent item with a submenu of children, pull the
    // parent out of |items| and put the children in.
    if (items.size() == 1 && items[0]->child_count() > 0) {
      const ExtensionMenuItem* parent = items[0];
      items.clear();
      for (int j = 0; j < parent->child_count(); j++) {
        const ExtensionMenuItem* child = parent->ChildAt(j);
        if (ExtensionContextMatch(params_, child->contexts()))
          items.push_back(child);
      }
    }
  }

  ExtensionMenuItem::Type last_type = ExtensionMenuItem::NORMAL;
  int radio_group_id = kExtensionsRadioGroup;
  for (std::vector<const ExtensionMenuItem*>::iterator i = items.begin();
       i != items.end(); ++i) {
    const ExtensionMenuItem* item = *i;
    if (item->type() == ExtensionMenuItem::SEPARATOR) {
      // We don't want the case of an extension with one top-level item that is
      // just a separator, so make sure this is inside a submenu.
      if (menu_model != &menu_model_) {
        menu_model->AddSeparator();
        last_type = ExtensionMenuItem::SEPARATOR;
      }
      continue;
    }

    // Auto-prepend a separator, if needed, to visually group radio items
    // together.
    if (item->type() != ExtensionMenuItem::RADIO &&
        item->type() != ExtensionMenuItem::SEPARATOR &&
        last_type == ExtensionMenuItem::RADIO) {
      menu_model->AddSeparator();
      radio_group_id++;
    }

    int menu_id = IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + (*index)++;
    if (menu_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST)
      return;
    extension_item_map_[menu_id] = item->id();
    string16 title = item->TitleWithReplacement(selection_text);
    if (item->type() == ExtensionMenuItem::NORMAL) {
      menu_model->AddItem(menu_id, title);
    } else if (item->type() == ExtensionMenuItem::CHECKBOX) {
      menu_model->AddCheckItem(menu_id, title);
    } else if (item->type() == ExtensionMenuItem::RADIO) {
      // Auto-append a separator if needed to visually group radio items
      // together.
      if (*index > 0 && last_type != ExtensionMenuItem::RADIO &&
          last_type != ExtensionMenuItem::SEPARATOR) {
        menu_model->AddSeparator();
        radio_group_id++;
      }

      menu_model->AddRadioItem(menu_id, title, radio_group_id);
    } else {
      NOTREACHED();
    }
    last_type = item->type();
  }
}

void RenderViewContextMenu::AppendAllExtensionItems() {
  extension_item_map_.clear();
  ExtensionsService* service = profile_->GetExtensionsService();
  if (!service)
    return;  // In unit-tests, we may not have an ExtensionService.
  ExtensionMenuManager* menu_manager = service->menu_manager();

  // Get a list of extension id's that have context menu items, and sort it by
  // the extension's name.
  std::set<std::string> ids = menu_manager->ExtensionIds();
  std::vector<std::pair<std::string, std::string> > sorted_ids;
  for (std::set<std::string>::iterator i = ids.begin(); i != ids.end(); ++i) {
    Extension* extension = service->GetExtensionById(*i, false);
    if (extension)
      sorted_ids.push_back(
          std::pair<std::string, std::string>(extension->name(), *i));
  }
  // TODO(asargent) - See if this works properly for i18n names (bug 32363).
  std::sort(sorted_ids.begin(), sorted_ids.end());

  int index = 0;
  std::vector<std::pair<std::string, std::string> >::const_iterator i;
  for (i = sorted_ids.begin();
       i != sorted_ids.end(); ++i) {
    AppendExtensionItems(i->second, &index);
  }
}

void RenderViewContextMenu::InitMenu() {
  bool has_link = !params_.link_url.is_empty();
  bool has_selection = !params_.selection_text.empty();

  if (AppendCustomItems()) {
    menu_model_.AddSeparator();
    AppendDeveloperItems();
    return;
  }

  // When no special node or text is selected and selection has no link,
  // show page items.
  bool is_devtools = false;
  if (params_.media_type == WebContextMenuData::MediaTypeNone &&
      !has_link &&
      !params_.is_editable &&
      !has_selection) {
    // If context is in subframe, show subframe options instead.
    if (!params_.frame_url.is_empty()) {
      is_devtools = IsDevToolsURL(params_.frame_url);
      if (!is_devtools)
        AppendFrameItems();
    } else if (!params_.page_url.is_empty()) {
      is_devtools = IsDevToolsURL(params_.page_url);
      if (!is_devtools)
        AppendPageItems();
    }
  }

  if (has_link) {
    AppendLinkItems();
    if (params_.media_type != WebContextMenuData::MediaTypeNone)
      menu_model_.AddSeparator();
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
  }

  if (params_.is_editable)
    AppendEditableItems();
  else if (has_selection)
    AppendCopyItem();

  if (has_selection)
    AppendSearchProvider();

  if (!is_devtools)
    AppendAllExtensionItems();

  // In the DevTools popup menu, "developer items" is normally the only section,
  // so omit the separator there.
  if (!is_devtools)
    menu_model_.AddSeparator();
  AppendDeveloperItems();
}

bool RenderViewContextMenu::AppendCustomItems() {
  std::vector<WebMenuItem>& custom_items = params_.custom_items;
  for (size_t i = 0; i < custom_items.size(); ++i) {
    DCHECK(IDC_CONTENT_CONTEXT_CUSTOM_FIRST + custom_items[i].action <
        IDC_CONTENT_CONTEXT_CUSTOM_LAST);
    menu_model_.AddItem(
        custom_items[i].action + IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
        custom_items[i].label);
  }
  return custom_items.size() > 0;
}

void RenderViewContextMenu::AppendDeveloperItems() {
  if (g_browser_process->have_inspector_files())
    menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_INSPECTELEMENT);
}

void RenderViewContextMenu::AppendLinkItems() {
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  menu_model_.AddItemIdentifiedByStringId(
      IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
  if (!external_) {
    menu_model_.AddItemIdentifiedByStringId(
        IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
  }
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_SAVELINKAS);

  if (params_.link_url.SchemeIs(chrome::kMailToScheme)) {
    menu_model_.AddItem(IDS_CONTENT_CONTEXT_COPYLINKLOCATION,
                   l10n_util::GetStringUTF16(
                       IDS_CONTENT_CONTEXT_COPYEMAILADDRESS));
  } else {
    menu_model_.AddItemIdentifiedByStringId(
        IDS_CONTENT_CONTEXT_COPYLINKLOCATION);
  }
}

void RenderViewContextMenu::AppendImageItems() {
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  menu_model_.AddItemIdentifiedByStringId(
      IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_COPYIMAGE);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

void RenderViewContextMenu::AppendAudioItems() {
  AppendMediaItems();
  menu_model_.AddSeparator();
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_SAVEAUDIOAS);
  menu_model_.AddItemIdentifiedByStringId(
      IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB);
}

void RenderViewContextMenu::AppendVideoItems() {
  AppendMediaItems();
  menu_model_.AddSeparator();
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_SAVEVIDEOAS);
  menu_model_.AddItemIdentifiedByStringId(
      IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB);
}

void RenderViewContextMenu::AppendMediaItems() {
  int media_flags = params_.media_flags;
  if (media_flags & WebContextMenuData::MediaPaused) {
    menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_PLAY);
  } else {
    menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_PAUSE);
  }

  if (media_flags & WebContextMenuData::MediaMuted) {
    menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_UNMUTE);
  } else {
    menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_MUTE);
  }

  menu_model_.AddCheckItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_LOOP);
  menu_model_.AddCheckItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_CONTROLS);
}

void RenderViewContextMenu::AppendPageItems() {
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_BACK);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_FORWARD);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_RELOAD);
  menu_model_.AddSeparator();
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_SAVEPAGEAS);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_PRINT);
  std::string locale = g_browser_process->GetApplicationLocale();
  locale = TranslateManager::GetLanguageCode(locale);
  string16 language = l10n_util::GetDisplayNameForLocale(locale, locale, true);
  menu_model_.AddItem(IDS_CONTENT_CONTEXT_TRANSLATE,
      l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_TRANSLATE, language));
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_VIEWPAGEINFO);
}

void RenderViewContextMenu::AppendFrameItems() {
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_BACK);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_FORWARD);
  menu_model_.AddSeparator();
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_RELOADFRAME);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB);
  menu_model_.AddItemIdentifiedByStringId(
      IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW);
  menu_model_.AddItemIdentifiedByStringId(
      IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD);
  menu_model_.AddSeparator();
  // These two menu items have yet to be implemented.
  // http://code.google.com/p/chromium/issues/detail?id=11827
  // menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_SAVEFRAMEAS);
  // menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_PRINTFRAME);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_VIEWFRAMEINFO);
}

void RenderViewContextMenu::AppendCopyItem() {
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendSearchProvider() {
  DCHECK(profile_);

  TrimWhitespace(params_.selection_text, TRIM_ALL, &params_.selection_text);
  if (params_.selection_text.empty())
    return;

  AutocompleteMatch match;
  profile_->GetAutocompleteClassifier()->Classify(params_.selection_text,
                                                  std::wstring(), &match, NULL);
  selection_navigation_url_ = match.destination_url;
  if (!selection_navigation_url_.is_valid())
    return;

  string16 printable_selection_text = PrintableSelectionText();
  // Escape "&" as "&&".
  for (size_t i = printable_selection_text.find('&'); i != string16::npos;
       i = printable_selection_text.find('&', i + 2))
    printable_selection_text.insert(i, 1, '&');

  if (match.transition == PageTransition::TYPED) {
    if (ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
        selection_navigation_url_.scheme())) {
      menu_model_.AddItem(
          IDS_CONTENT_CONTEXT_GOTOURL,
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_GOTOURL,
                                     printable_selection_text));
    }
  } else {
    const TemplateURL* const default_provider =
        profile_->GetTemplateURLModel()->GetDefaultSearchProvider();
    if (!default_provider)
      return;
    menu_model_.AddItem(
        IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
                                   WideToUTF16(default_provider->short_name()),
                                   printable_selection_text));
  }
}

void RenderViewContextMenu::AppendEditableItems() {
  // Append Dictionary spell check suggestions.
  for (size_t i = 0; i < params_.dictionary_suggestions.size() &&
       IDC_SPELLCHECK_SUGGESTION_0 + i <= IDC_SPELLCHECK_SUGGESTION_LAST;
       ++i) {
    menu_model_.AddItem(IDC_SPELLCHECK_SUGGESTION_0 + static_cast<int>(i),
                        params_.dictionary_suggestions[i]);
  }
  if (params_.dictionary_suggestions.size() > 0)
    menu_model_.AddSeparator();

  // If word is misspelled, give option for "Add to dictionary"
  if (!params_.misspelled_word.empty()) {
    if (params_.dictionary_suggestions.size() == 0) {
      menu_model_.AddItem(0,
          l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS));
    }
    menu_model_.AddItemIdentifiedByStringId(
        IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY);
    menu_model_.AddSeparator();
  }

  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_UNDO);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_REDO);
  menu_model_.AddSeparator();
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_CUT);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_COPY);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_PASTE);
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_DELETE);
  menu_model_.AddSeparator();

  AppendSpellcheckOptionsSubMenu();

#if defined(OS_MACOSX)
  // OS X provides a contextual menu to set writing direction for BiDi
  // languages.
  // This functionality is exposed as a keyboard shortcut on Windows & Linux.
  AppendBidiSubMenu();
#endif  // OS_MACOSX

  menu_model_.AddSeparator();
  menu_model_.AddItemIdentifiedByStringId(IDS_CONTENT_CONTEXT_SELECTALL);
}

void RenderViewContextMenu::AppendSpellcheckOptionsSubMenu() {
  // Add Spell Check languages to sub menu.
  std::vector<std::string> spellcheck_languages;
  SpellCheckHost::GetSpellCheckLanguages(profile_,
      &spellcheck_languages);
  DCHECK(spellcheck_languages.size() <
         IDC_SPELLCHECK_LANGUAGES_LAST - IDC_SPELLCHECK_LANGUAGES_FIRST);
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < spellcheck_languages.size(); ++i) {
    string16 display_name(l10n_util::GetDisplayNameForLocale(
        spellcheck_languages[i], app_locale, true));
    spellcheck_submenu_model_.AddRadioItem(
        IDC_SPELLCHECK_LANGUAGES_FIRST + i,
        display_name,
        kSpellcheckRadioGroup);
  }

  // Add item in the sub menu to pop up the fonts and languages options menu.
  spellcheck_submenu_model_.AddSeparator();
  spellcheck_submenu_model_.AddItemIdentifiedByStringId(
      IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS);

  // Add 'Check the spelling of this field' item in the sub menu.
  spellcheck_submenu_model_.AddCheckItem(
      IDC_CHECK_SPELLING_OF_THIS_FIELD,
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_CHECK_SPELLING_OF_THIS_FIELD));

  // Add option for showing the spelling panel if the platform spellchecker
  // supports it.
  if (SpellCheckerPlatform::SpellCheckerAvailable() &&
      SpellCheckerPlatform::SpellCheckerProvidesPanel()) {
    spellcheck_submenu_model_.AddCheckItem(
        IDC_SPELLPANEL_TOGGLE,
        l10n_util::GetStringUTF16(
            SpellCheckerPlatform::SpellingPanelVisible() ?
                IDS_CONTENT_CONTEXT_HIDE_SPELLING_PANEL :
                IDS_CONTENT_CONTEXT_SHOW_SPELLING_PANEL));
  }

  menu_model_.AddSubMenu(
      IDC_SPELLCHECK_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLCHECK_MENU),
      &spellcheck_submenu_model_);
}

#if defined(OS_MACOSX)
void RenderViewContextMenu::AppendBidiSubMenu() {
  bidi_submenu_model_.AddCheckItem(IDC_WRITING_DIRECTION_DEFAULT,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_DEFAULT));
  bidi_submenu_model_.AddCheckItem(IDC_WRITING_DIRECTION_LTR,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR));
  bidi_submenu_model_.AddCheckItem(IDC_WRITING_DIRECTION_RTL,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL));

  menu_model_.AddSubMenu(
      IDC_WRITING_DIRECTION_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_MENU),
      &bidi_submenu_model_);
}
#endif  // OS_MACOSX

ExtensionMenuItem* RenderViewContextMenu::GetExtensionMenuItem(int id) const {
  ExtensionMenuManager* manager =
      profile_->GetExtensionsService()->menu_manager();
  std::map<int, int>::const_iterator i = extension_item_map_.find(id);
  if (i != extension_item_map_.end()) {
    ExtensionMenuItem* item = manager->GetItemById(i->second);
    if (item)
      return item;
  }
  return NULL;
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenu::IsCommandIdEnabled(int id) const {
  // Allow Spell Check language items on sub menu for text area context menu.
  if ((id >= IDC_SPELLCHECK_LANGUAGES_FIRST) &&
      (id < IDC_SPELLCHECK_LANGUAGES_LAST)) {
    return profile_->GetPrefs()->GetBoolean(prefs::kEnableSpellCheck);
  }

  // Process custom actions range.
  if ((id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST) &&
      (id < IDC_CONTENT_CONTEXT_CUSTOM_LAST)) {
    unsigned action = id - IDC_CONTENT_CONTEXT_CUSTOM_FIRST;
    for (size_t i = 0; i < params_.custom_items.size(); ++i) {
      if (params_.custom_items[i].action == action)
        return params_.custom_items[i].enabled;
    }
    NOTREACHED();
    return false;
  }

  // Extension items.
  if (id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    ExtensionMenuItem* item = GetExtensionMenuItem(id);
    if (item)
      return ExtensionContextMatch(params_, item->enabled_contexts());
    else
      return false;
  }

  switch (id) {
    case IDS_CONTENT_CONTEXT_BACK:
      return source_tab_contents_->controller().CanGoBack();

    case IDS_CONTENT_CONTEXT_FORWARD:
      return source_tab_contents_->controller().CanGoForward();

    case IDS_CONTENT_CONTEXT_RELOAD:
      return source_tab_contents_->delegate()->CanReloadContents(
          source_tab_contents_);

    case IDS_CONTENT_CONTEXT_VIEWPAGESOURCE:
    case IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      return source_tab_contents_->controller().CanViewSource();

    case IDS_CONTENT_CONTEXT_INSPECTELEMENT:
    // Viewing page info is not a developer command but is meaningful for the
    // same set of pages which developer commands are meaningful for.
    case IDS_CONTENT_CONTEXT_VIEWPAGEINFO:
      return IsDevCommandEnabled(id);

    case IDS_CONTENT_CONTEXT_TRANSLATE: {
      std::string original_lang =
          source_tab_contents_->language_state().original_language();
      std::string target_lang = g_browser_process->GetApplicationLocale();
      target_lang = TranslateManager::GetLanguageCode(target_lang);
      return original_lang != target_lang &&
             !source_tab_contents_->language_state().IsPageTranslated() &&
             !source_tab_contents_->interstitial_page() &&
             TranslateManager::IsTranslatableURL(params_.page_url);
    }

    case IDS_CONTENT_CONTEXT_OPENLINKNEWTAB:
    case IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      return params_.link_url.is_valid();

    case IDS_CONTENT_CONTEXT_COPYLINKLOCATION:
      return params_.unfiltered_link_url.is_valid();

    case IDS_CONTENT_CONTEXT_SAVELINKAS:
      return params_.link_url.is_valid() &&
             URLRequest::IsHandledURL(params_.link_url);

    case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
      return params_.src_url.is_valid() &&
             URLRequest::IsHandledURL(params_.src_url);

    case IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB:
      // The images shown in the most visited thumbnails do not currently open
      // in a new tab as they should. Disabling this context menu option for
      // now, as a quick hack, before we resolve this issue (Issue = 2608).
      // TODO(sidchat): Enable this option once this issue is resolved.
      if (params_.src_url.scheme() == chrome::kChromeUIScheme)
        return false;
      return true;

    case IDS_CONTENT_CONTEXT_COPYIMAGE:
      return !params_.is_image_blocked;

    case IDS_CONTENT_CONTEXT_FULLSCREEN:
      // TODO(ajwong): Enable fullscreen after we actually implement this.
      return false;

    // Media control commands should all be disabled if the player is in an
    // error state.
    case IDS_CONTENT_CONTEXT_PLAY:
    case IDS_CONTENT_CONTEXT_PAUSE:
    case IDS_CONTENT_CONTEXT_LOOP:
      return (params_.media_flags &
              WebContextMenuData::MediaInError) == 0;

    // Mute and unmute should also be disabled if the player has no audio.
    case IDS_CONTENT_CONTEXT_MUTE:
    case IDS_CONTENT_CONTEXT_UNMUTE:
      return (params_.media_flags &
              WebContextMenuData::MediaHasAudio) != 0 &&
             (params_.media_flags &
              WebContextMenuData::MediaInError) == 0;

    // Media controls can be toggled only for video player. If we toggle
    // controls for audio then the player disappears, and there is no way to
    // return it back.
    case IDS_CONTENT_CONTEXT_CONTROLS:
      return (params_.media_flags &
              WebContextMenuData::MediaHasVideo) != 0;

    case IDS_CONTENT_CONTEXT_SAVESCREENSHOTAS:
      // TODO(ajwong): Enable save screenshot after we actually implement
      // this.
      return false;

    case IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION:
    case IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION:
    case IDS_CONTENT_CONTEXT_COPYIMAGELOCATION:
      return params_.src_url.is_valid();

    case IDS_CONTENT_CONTEXT_SAVEAUDIOAS:
    case IDS_CONTENT_CONTEXT_SAVEVIDEOAS:
      return (params_.media_flags &
              WebContextMenuData::MediaCanSave) &&
             params_.src_url.is_valid() &&
             URLRequest::IsHandledURL(params_.src_url);

    case IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB:
    case IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB:
      return true;

    case IDS_CONTENT_CONTEXT_SAVEPAGEAS: {
      // Instead of using GetURL here, we use url() (which is the "real" url of
      // the page) from the NavigationEntry because its reflects their origin
      // rather than the display one (returned by GetURL) which may be
      // different (like having "view-source:" on the front).
      NavigationEntry* active_entry =
          source_tab_contents_->controller().GetActiveEntry();
      return SavePackage::IsSavableURL(
          (active_entry) ? active_entry->url() : GURL());
    }

    case IDS_CONTENT_CONTEXT_RELOADFRAME:
    case IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB:
    case IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW:
      return params_.frame_url.is_valid();

    case IDS_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & WebContextMenuData::CanUndo);

    case IDS_CONTENT_CONTEXT_REDO:
      return !!(params_.edit_flags & WebContextMenuData::CanRedo);

    case IDS_CONTENT_CONTEXT_CUT:
      return !!(params_.edit_flags & WebContextMenuData::CanCut);

    case IDS_CONTENT_CONTEXT_COPY:
      return !!(params_.edit_flags & WebContextMenuData::CanCopy);

    case IDS_CONTENT_CONTEXT_PASTE:
      return !!(params_.edit_flags & WebContextMenuData::CanPaste);

    case IDS_CONTENT_CONTEXT_DELETE:
      return !!(params_.edit_flags & WebContextMenuData::CanDelete);

    case IDS_CONTENT_CONTEXT_SELECTALL:
      return !!(params_.edit_flags & WebContextMenuData::CanSelectAll);

    case IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      return !profile_->IsOffTheRecord() && params_.link_url.is_valid();

    case IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD:
      return !profile_->IsOffTheRecord() && params_.frame_url.is_valid();

    case IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY:
      return !params_.misspelled_word.empty();

#if !defined(OS_CHROMEOS)
    case IDS_CONTENT_CONTEXT_PRINT:
#endif
    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDS_CONTENT_CONTEXT_GOTOURL:
    case IDC_SPELLCHECK_SUGGESTION_0:
    case IDC_SPELLCHECK_SUGGESTION_1:
    case IDC_SPELLCHECK_SUGGESTION_2:
    case IDC_SPELLCHECK_SUGGESTION_3:
    case IDC_SPELLCHECK_SUGGESTION_4:
    case IDC_SPELLPANEL_TOGGLE:
#if !defined(OS_MACOSX)
    // TODO(jeremy): re-enable - http://crbug.com/34512 .
    case IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
#endif
    case IDS_CONTENT_CONTEXT_VIEWFRAMEINFO:
      return true;

    case IDC_CHECK_SPELLING_OF_THIS_FIELD:
      return profile_->GetPrefs()->GetBoolean(prefs::kEnableSpellCheck);

#if defined(OS_MACOSX)
    // TODO(jeremy): re-enable - http://crbug.com/34512 .
    case IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      return false;
#endif

#if defined(OS_MACOSX)
    case IDC_WRITING_DIRECTION_DEFAULT:  // Provided to match OS defaults.
      return params_.writing_direction_default &
          WebContextMenuData::CheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_RTL:
      return params_.writing_direction_right_to_left &
          WebContextMenuData::CheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_LTR:
      return params_.writing_direction_left_to_right &
          WebContextMenuData::CheckableMenuItemEnabled;
    case IDC_WRITING_DIRECTION_MENU:
      return true;
#endif  // OS_MACOSX

#if defined(OS_LINUX)
    // TODO(suzhe): this should not be enabled for password fields.
    case IDC_INPUT_METHODS_MENU:
      return true;
#endif

    case IDS_CONTENT_CONTEXT_SAVEFRAMEAS:
    case IDS_CONTENT_CONTEXT_PRINTFRAME:
    case IDS_CONTENT_CONTEXT_ADDSEARCHENGINE:  // Not implemented.
      return false;

    case IDC_SPELLCHECK_MENU:
      return true;

    default:
      NOTREACHED();
      return false;
  }
}

bool RenderViewContextMenu::IsCommandIdChecked(int id) const {
  // See if the video is set to looping.
  if (id == IDS_CONTENT_CONTEXT_LOOP) {
    return (params_.media_flags &
            WebContextMenuData::MediaLoop) != 0;
  }

  if (id == IDS_CONTENT_CONTEXT_CONTROLS) {
    return (params_.media_flags &
            WebContextMenuData::MediaControls) != 0;
  }

  if (id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    ExtensionMenuItem* item = GetExtensionMenuItem(id);
    if (item)
      return item->checked();
    else
      return false;
  }

#if defined(OS_MACOSX)
    if (id == IDC_WRITING_DIRECTION_DEFAULT)
      return params_.writing_direction_default &
          WebContextMenuData::CheckableMenuItemChecked;
    if (id == IDC_WRITING_DIRECTION_RTL)
      return params_.writing_direction_right_to_left &
          WebContextMenuData::CheckableMenuItemChecked;
    if (id == IDC_WRITING_DIRECTION_LTR)
      return params_.writing_direction_left_to_right &
          WebContextMenuData::CheckableMenuItemChecked;
#endif  // OS_MACOSX

  // Check box for 'Check the Spelling of this field'.
  if (id == IDC_CHECK_SPELLING_OF_THIS_FIELD) {
    return (params_.spellcheck_enabled &&
            profile_->GetPrefs()->GetBoolean(prefs::kEnableSpellCheck));
  }

  // Don't bother getting the display language vector if this isn't a spellcheck
  // language.
  if ((id < IDC_SPELLCHECK_LANGUAGES_FIRST) ||
      (id >= IDC_SPELLCHECK_LANGUAGES_LAST))
    return false;

  std::vector<std::string> languages;
  return SpellCheckHost::GetSpellCheckLanguages(profile_, &languages) ==
      (id - IDC_SPELLCHECK_LANGUAGES_FIRST);
}

void RenderViewContextMenu::ExecuteCommand(int id) {
  // Check to see if one of the spell check language ids have been clicked.
  if (id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    const size_t language_number = id - IDC_SPELLCHECK_LANGUAGES_FIRST;
    std::vector<std::string> languages;
    SpellCheckHost::GetSpellCheckLanguages(profile_, &languages);
    if (language_number < languages.size()) {
      StringPrefMember dictionary_language;
      dictionary_language.Init(prefs::kSpellCheckDictionary,
          profile_->GetPrefs(), NULL);
      dictionary_language.SetValue(ASCIIToWide(languages[language_number]));
    }
    return;
  }

  // Process custom actions range.
  if ((id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST) &&
      (id < IDC_CONTENT_CONTEXT_CUSTOM_LAST)) {
    unsigned action = id - IDC_CONTENT_CONTEXT_CUSTOM_FIRST;
    source_tab_contents_->render_view_host()->
        PerformCustomContextMenuAction(action);
    return;
  }

  // Process extension menu items.
  if (id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    ExtensionMenuManager* manager =
        profile_->GetExtensionsService()->menu_manager();
    std::map<int, int>::const_iterator i = extension_item_map_.find(id);
    if (i != extension_item_map_.end()) {
      manager->ExecuteCommand(profile_, source_tab_contents_, params_,
                              i->second);
    }
    return;
  }


  switch (id) {
    case IDS_CONTENT_CONTEXT_OPENLINKNEWTAB:
      OpenURL(params_.link_url,
              source_tab_contents_->delegate() &&
              source_tab_contents_->delegate()->IsApplication() ?
                  NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB,
              PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      OpenURL(params_.link_url, NEW_WINDOW, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      OpenURL(params_.link_url, OFF_THE_RECORD, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_SAVEAUDIOAS:
    case IDS_CONTENT_CONTEXT_SAVEVIDEOAS:
    case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
    case IDS_CONTENT_CONTEXT_SAVELINKAS: {
      const GURL& referrer =
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url;
      const GURL& url =
          (id == IDS_CONTENT_CONTEXT_SAVELINKAS ? params_.link_url :
                                                  params_.src_url);
      DownloadManager* dlm = profile_->GetDownloadManager();
      dlm->DownloadUrl(url, referrer, params_.frame_charset,
                       source_tab_contents_);
      break;
    }

    case IDS_CONTENT_CONTEXT_COPYLINKLOCATION:
      WriteURLToClipboard(params_.unfiltered_link_url);
      break;

    case IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION:
    case IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION:
    case IDS_CONTENT_CONTEXT_COPYIMAGELOCATION:
      WriteURLToClipboard(params_.src_url);
      break;

    case IDS_CONTENT_CONTEXT_COPYIMAGE:
      CopyImageAt(params_.x, params_.y);
      break;

    case IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB:
    case IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB:
    case IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB:
      OpenURL(params_.src_url, NEW_BACKGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_PLAY:
      UserMetrics::RecordAction(UserMetricsAction("MediaContextMenu_Play"),
                                profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Play, true));
      break;

    case IDS_CONTENT_CONTEXT_PAUSE:
      UserMetrics::RecordAction(UserMetricsAction("MediaContextMenu_Pause"),
                                profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Play, false));
      break;

    case IDS_CONTENT_CONTEXT_MUTE:
      UserMetrics::RecordAction(UserMetricsAction("MediaContextMenu_Mute"),
                                profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Mute, true));
      break;

    case IDS_CONTENT_CONTEXT_UNMUTE:
      UserMetrics::RecordAction(UserMetricsAction("MediaContextMenu_Unmute"),
                                profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Mute, false));
      break;

    case IDS_CONTENT_CONTEXT_LOOP:
      UserMetrics::RecordAction(UserMetricsAction("MediaContextMenu_Loop"),
                                profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Loop,
                              !IsCommandIdChecked(IDS_CONTENT_CONTEXT_LOOP)));
      break;

    case IDS_CONTENT_CONTEXT_CONTROLS:
      UserMetrics::RecordAction(UserMetricsAction("MediaContextMenu_Controls"),
                                profile_);
      MediaPlayerActionAt(
          gfx::Point(params_.x, params_.y),
          WebMediaPlayerAction(
              WebMediaPlayerAction::Controls,
              !IsCommandIdChecked(IDS_CONTENT_CONTEXT_CONTROLS)));
      break;

    case IDS_CONTENT_CONTEXT_BACK:
      source_tab_contents_->controller().GoBack();
      break;

    case IDS_CONTENT_CONTEXT_FORWARD:
      source_tab_contents_->controller().GoForward();
      break;

    case IDS_CONTENT_CONTEXT_SAVEPAGEAS:
      source_tab_contents_->OnSavePage();
      break;

    case IDS_CONTENT_CONTEXT_RELOAD:
      // Prevent the modal "Resubmit form post" dialog from appearing in the
      // context of an external context menu.
      source_tab_contents_->controller().Reload(!external_);
      break;

    case IDS_CONTENT_CONTEXT_PRINT:
      source_tab_contents_->PrintPreview();
      break;

    case IDS_CONTENT_CONTEXT_VIEWPAGESOURCE:
      OpenURL(GURL("view-source:" + params_.page_url.spec()),
              NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_INSPECTELEMENT:
      Inspect(params_.x, params_.y);
      break;

    case IDS_CONTENT_CONTEXT_VIEWPAGEINFO: {
      NavigationEntry* nav_entry =
          source_tab_contents_->controller().GetActiveEntry();
      source_tab_contents_->ShowPageInfo(nav_entry->url(), nav_entry->ssl(),
                                         true);
      break;
    }

    case IDS_CONTENT_CONTEXT_TRANSLATE: {
      // A translation might have been triggered by the time the menu got
      // selected, do nothing in that case.
      if (source_tab_contents_->language_state().IsPageTranslated() ||
          source_tab_contents_->language_state().translation_pending()) {
        return;
      }
      std::string original_lang =
          source_tab_contents_->language_state().original_language();
      std::string target_lang = g_browser_process->GetApplicationLocale();
      target_lang = TranslateManager::GetLanguageCode(target_lang);
      // Since the user decided to translate for that language and site, clears
      // any preferences for not translating them.
      TranslatePrefs prefs(profile_->GetPrefs());
      prefs.RemoveLanguageFromBlacklist(original_lang);
      prefs.RemoveSiteFromBlacklist(params_.page_url.HostNoBrackets());
      Singleton<TranslateManager>::get()->TranslatePage(
          source_tab_contents_, original_lang, target_lang);
      break;
    }

    case IDS_CONTENT_CONTEXT_RELOADFRAME:
      source_tab_contents_->render_view_host()->ReloadFrame();
      break;

    case IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB:
      OpenURL(params_.frame_url, NEW_BACKGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW:
      OpenURL(params_.frame_url, NEW_WINDOW, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD:
      OpenURL(params_.frame_url, OFF_THE_RECORD, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_SAVEFRAMEAS:
      // http://code.google.com/p/chromium/issues/detail?id=11827
      NOTIMPLEMENTED() << "IDS_CONTENT_CONTEXT_SAVEFRAMEAS";
      break;

    case IDS_CONTENT_CONTEXT_PRINTFRAME:
      // http://code.google.com/p/chromium/issues/detail?id=11827
      NOTIMPLEMENTED() << "IDS_CONTENT_CONTEXT_PRINTFRAME";
      break;

    case IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      OpenURL(GURL("view-source:" + params_.frame_url.spec()),
              NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_VIEWFRAMEINFO: {
      // Deserialize the SSL info.
      NavigationEntry::SSLStatus ssl;
      if (!params_.security_info.empty()) {
        int cert_id, cert_status, security_bits;
        SSLManager::DeserializeSecurityInfo(params_.security_info,
                                            &cert_id,
                                            &cert_status,
                                            &security_bits);
        ssl.set_cert_id(cert_id);
        ssl.set_cert_status(cert_status);
        ssl.set_security_bits(security_bits);
      }
      source_tab_contents_->ShowPageInfo(params_.frame_url, ssl,
                                         false);  // Don't show the history.
      break;
    }

    case IDS_CONTENT_CONTEXT_UNDO:
      source_tab_contents_->render_view_host()->Undo();
      break;

    case IDS_CONTENT_CONTEXT_REDO:
      source_tab_contents_->render_view_host()->Redo();
      break;

    case IDS_CONTENT_CONTEXT_CUT:
      source_tab_contents_->render_view_host()->Cut();
      break;

    case IDS_CONTENT_CONTEXT_COPY:
      source_tab_contents_->render_view_host()->Copy();
      break;

    case IDS_CONTENT_CONTEXT_PASTE:
      source_tab_contents_->render_view_host()->Paste();
      break;

    case IDS_CONTENT_CONTEXT_DELETE:
      source_tab_contents_->render_view_host()->Delete();
      break;

    case IDS_CONTENT_CONTEXT_SELECTALL:
      source_tab_contents_->render_view_host()->SelectAll();
      break;

    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDS_CONTENT_CONTEXT_GOTOURL: {
      OpenURL(selection_navigation_url_, NEW_FOREGROUND_TAB,
              PageTransition::LINK);
      break;
    }

    case IDC_SPELLCHECK_SUGGESTION_0:
    case IDC_SPELLCHECK_SUGGESTION_1:
    case IDC_SPELLCHECK_SUGGESTION_2:
    case IDC_SPELLCHECK_SUGGESTION_3:
    case IDC_SPELLCHECK_SUGGESTION_4:
      source_tab_contents_->render_view_host()->Replace(
          params_.dictionary_suggestions[id - IDC_SPELLCHECK_SUGGESTION_0]);
      break;

    case IDC_CHECK_SPELLING_OF_THIS_FIELD:
      source_tab_contents_->render_view_host()->ToggleSpellCheck();
      break;
    case IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY: {
      SpellCheckHost* spellcheck_host = profile_->GetSpellCheckHost();
      if (!spellcheck_host) {
        NOTREACHED();
        break;
      }
      spellcheck_host->AddWord(UTF16ToUTF8(params_.misspelled_word));
      SpellCheckerPlatform::AddWord(params_.misspelled_word);
      break;
    }

    case IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      ShowFontsLanguagesWindow(
          platform_util::GetTopLevel(
              source_tab_contents_->GetContentNativeView()),
          LANGUAGES_PAGE, profile_);
      break;

    case IDC_SPELLPANEL_TOGGLE:
      source_tab_contents_->render_view_host()->ToggleSpellPanel(
          SpellCheckerPlatform::SpellingPanelVisible());
      break;

#if defined(OS_MACOSX)
    case IDC_WRITING_DIRECTION_DEFAULT:
      // WebKit's current behavior is for this menu item to always be disabled.
      NOTREACHED();
      break;
    case IDC_WRITING_DIRECTION_RTL:
    case IDC_WRITING_DIRECTION_LTR: {
      WebKit::WebTextDirection dir = WebKit::WebTextDirectionLeftToRight;
      if (id == IDC_WRITING_DIRECTION_RTL)
        dir = WebKit::WebTextDirectionRightToLeft;
      source_tab_contents_->render_view_host()->UpdateTextDirection(dir);
      source_tab_contents_->render_view_host()->NotifyTextDirection();
      break;
    }
#endif  // OS_MACOSX

    case IDS_CONTENT_CONTEXT_ADDSEARCHENGINE:  // Not implemented.
    default:
      break;
  }
}

bool RenderViewContextMenu::IsDevCommandEnabled(int id) const {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAlwaysEnableDevTools))
    return true;

  NavigationEntry *active_entry =
      source_tab_contents_->controller().GetActiveEntry();
  if (!active_entry)
    return false;

  // Don't inspect view source.
  if (active_entry->IsViewSourceMode())
    return false;

  // Don't inspect HTML dialogs (doesn't work anyway).
  if (active_entry->url().SchemeIs(chrome::kGearsScheme))
    return false;

#if defined NDEBUG
  bool debug_mode = false;
#else
  bool debug_mode = true;
#endif
  // Don't inspect new tab UI, etc.
  if (active_entry->url().SchemeIs(chrome::kChromeUIScheme) && !debug_mode &&
      active_entry->url().host() != chrome::kChromeUIDevToolsHost)
    return false;

  // Don't inspect about:network, about:memory, etc.
  // However, we do want to inspect about:blank, which is often
  // used by ordinary web pages.
  if (active_entry->virtual_url().SchemeIs(chrome::kAboutScheme) &&
      !LowerCaseEqualsASCII(active_entry->virtual_url().path(), "blank"))
    return false;

  if (id == IDS_CONTENT_CONTEXT_INSPECTELEMENT) {
    // Don't enable the web inspector if JavaScript is disabled.
    if (!profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled) ||
        command_line.HasSwitch(switches::kDisableJavaScript))
      return false;
    // Don't enable the web inspector on web inspector if there is no process
    // per tab flag set.
    if (IsDevToolsURL(active_entry->url()) &&
        !command_line.HasSwitch(switches::kProcessPerTab))
      return false;
  }

  return true;
}

string16 RenderViewContextMenu::PrintableSelectionText() {
  return WideToUTF16(l10n_util::TruncateString(params_.selection_text, 50));
}

// Controller functions --------------------------------------------------------

void RenderViewContextMenu::OpenURL(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition) {
  source_tab_contents_->OpenURL(url, GURL(), disposition, transition);
}

void RenderViewContextMenu::CopyImageAt(int x, int y) {
  source_tab_contents_->render_view_host()->CopyImageAt(x, y);
}

void RenderViewContextMenu::Inspect(int x, int y) {
  UserMetrics::RecordAction(UserMetricsAction("DevTools_InspectElement"),
                            profile_);
  DevToolsManager::GetInstance()->InspectElement(
      source_tab_contents_->render_view_host(), x, y);
}

void RenderViewContextMenu::WriteURLToClipboard(const GURL& url) {
  chrome_browser_net::WriteURLToClipboard(
      url,
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
      g_browser_process->clipboard());
}

void RenderViewContextMenu::MediaPlayerActionAt(
    const gfx::Point& location,
    const WebMediaPlayerAction& action) {
  source_tab_contents_->render_view_host()->MediaPlayerActionAt(
      location, action);
}
