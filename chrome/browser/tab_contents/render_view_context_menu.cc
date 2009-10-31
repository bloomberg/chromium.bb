// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu.h"

#include "app/clipboard/clipboard.h"
#include "app/clipboard/scoped_clipboard_writer.h"
#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/fonts_languages_window.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_versus_navigate_classifier.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/platform_util.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "webkit/api/public/WebMediaPlayerAction.h"
#include "webkit/api/public/WebContextMenuData.h"

using WebKit::WebContextMenuData;
using WebKit::WebMediaPlayerAction;

namespace {

string16 EscapeAmpersands(const string16& text) {
  string16 ret;
  ret.reserve(text.length() * 2);
  for (string16::const_iterator i = text.begin();
       i != text.end(); ++i) {
    // The escape for an ampersand is two ampersands.
    if ('&' == *i)
      ret.push_back(*i);

    ret.push_back(*i);
  }

  return ret;
}

}  // namespace

RenderViewContextMenu::RenderViewContextMenu(
    TabContents* tab_contents,
    const ContextMenuParams& params)
    : params_(params),
      source_tab_contents_(tab_contents),
      profile_(tab_contents->profile()) {
}

RenderViewContextMenu::~RenderViewContextMenu() {
}

// Menu construction functions -------------------------------------------------

void RenderViewContextMenu::Init() {
  InitMenu();
  DoInit();
}

void RenderViewContextMenu::InitMenu() {
  bool has_link = !params_.link_url.is_empty();
  bool has_selection = !params_.selection_text.empty();

  // When no special node or text is selected and selection has no link,
  // show page items.
  if (params_.media_type == WebContextMenuData::MediaTypeNone &&
      !has_link &&
      !params_.is_editable &&
      !has_selection) {
    // If context is in subframe, show subframe options instead.
    if (!params_.frame_url.is_empty()) {
      AppendFrameItems();
    } else if (!params_.page_url.is_empty()) {
      AppendPageItems();
    }
  }

  if (has_link) {
    AppendLinkItems();
    if (params_.media_type != WebContextMenuData::MediaTypeNone)
      AppendSeparator();
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
  else if (has_selection || has_link)
    AppendCopyItem();

  if (has_selection)
    AppendSearchProvider();

  AppendDeveloperItems();
}

void RenderViewContextMenu::AppendDeveloperItems() {
  if (g_browser_process->have_inspector_files()) {
    AppendSeparator();
    AppendMenuItem(IDS_CONTENT_CONTEXT_INSPECTELEMENT);
  }
}

void RenderViewContextMenu::AppendLinkItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVELINKAS);

  if (params_.link_url.SchemeIs(chrome::kMailToScheme)) {
    AppendMenuItem(IDS_CONTENT_CONTEXT_COPYLINKLOCATION,
                   l10n_util::GetStringUTF16(
                       IDS_CONTENT_CONTEXT_COPYEMAILADDRESS));
  } else {
    AppendMenuItem(IDS_CONTENT_CONTEXT_COPYLINKLOCATION);
  }
}

void RenderViewContextMenu::AppendImageItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYIMAGE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

void RenderViewContextMenu::AppendAudioItems() {
  AppendMediaItems();
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEAUDIOAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB);
}

void RenderViewContextMenu::AppendVideoItems() {
  AppendMediaItems();
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEVIDEOAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB);
}

void RenderViewContextMenu::AppendMediaItems() {
  int media_flags = params_.media_flags;
  if (media_flags & WebContextMenuData::MediaPaused) {
    AppendMenuItem(IDS_CONTENT_CONTEXT_PLAY);
  } else {
    AppendMenuItem(IDS_CONTENT_CONTEXT_PAUSE);
  }

  if (media_flags & WebContextMenuData::MediaMuted) {
    AppendMenuItem(IDS_CONTENT_CONTEXT_UNMUTE);
  } else {
    AppendMenuItem(IDS_CONTENT_CONTEXT_MUTE);
  }

  AppendCheckboxMenuItem(IDS_CONTENT_CONTEXT_LOOP,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_LOOP));
}

void RenderViewContextMenu::AppendPageItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_BACK);
  AppendMenuItem(IDS_CONTENT_CONTEXT_FORWARD);
  AppendMenuItem(IDS_CONTENT_CONTEXT_RELOAD);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEPAGEAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_PRINT);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWPAGEINFO);
}

void RenderViewContextMenu::AppendFrameItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_BACK);
  AppendMenuItem(IDS_CONTENT_CONTEXT_FORWARD);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD);
  AppendSeparator();
  // These two menu items have yet to be implemented.
  // http://code.google.com/p/chromium/issues/detail?id=11827
  // AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEFRAMEAS);
  // AppendMenuItem(IDS_CONTENT_CONTEXT_PRINTFRAME);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWFRAMEINFO);
}

void RenderViewContextMenu::AppendCopyItem() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendSearchProvider() {
  DCHECK(profile_);
  const TemplateURL* const default_provider =
      profile_->GetTemplateURLModel()->GetDefaultSearchProvider();
  if (!default_provider)
    return;

  std::wstring selection_text = EscapeAmpersands(
      l10n_util::TruncateString(params_.selection_text, 50));
  if (selection_text.empty())
    return;

  bool is_search;
  profile_->GetSearchVersusNavigateClassifier()->Classify(selection_text,
      std::wstring(), &is_search, &selection_navigation_url_, &transition_,
      NULL, NULL);
  if (!selection_navigation_url_.is_valid())
    return;

  if (is_search) {
    string16 label(l10n_util::GetStringFUTF16(
        IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
        WideToUTF16(default_provider->short_name()),
        selection_text));
    AppendMenuItem(IDS_CONTENT_CONTEXT_SEARCHWEBFOR, label);
  } else {
    string16 label(l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_GOTOURL,
                                              selection_text));
    AppendMenuItem(IDS_CONTENT_CONTEXT_GOTOURL, label);
  }
}

void RenderViewContextMenu::AppendEditableItems() {
  // Append Dictionary spell check suggestions.
  for (size_t i = 0; i < params_.dictionary_suggestions.size() &&
       IDC_SPELLCHECK_SUGGESTION_0 + i <= IDC_SPELLCHECK_SUGGESTION_LAST;
       ++i) {
    AppendMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + static_cast<int>(i),
                   params_.dictionary_suggestions[i]);
  }
  if (params_.dictionary_suggestions.size() > 0)
    AppendSeparator();

  // If word is misspelled, give option for "Add to dictionary"
  if (!params_.misspelled_word.empty()) {
    if (params_.dictionary_suggestions.size() == 0) {
      AppendMenuItem(0,
          l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS));
    }
    AppendMenuItem(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY);
    AppendSeparator();
  }

  AppendMenuItem(IDS_CONTENT_CONTEXT_UNDO);
  AppendMenuItem(IDS_CONTENT_CONTEXT_REDO);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_CUT);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPY);
  AppendMenuItem(IDS_CONTENT_CONTEXT_PASTE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_DELETE);
  AppendSeparator();

  // Add Spell Check options sub menu.
  StartSubMenu(IDC_SPELLCHECK_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLCHECK_MENU));

  // Add Spell Check languages to sub menu.
  std::vector<std::string> spellcheck_languages;
  SpellChecker::GetSpellCheckLanguages(profile_,
      &spellcheck_languages);
  DCHECK(spellcheck_languages.size() <
         IDC_SPELLCHECK_LANGUAGES_LAST - IDC_SPELLCHECK_LANGUAGES_FIRST);
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < spellcheck_languages.size(); ++i) {
    string16 display_name(l10n_util::GetDisplayNameForLocale(
        spellcheck_languages[i], app_locale, true));
    AppendRadioMenuItem(IDC_SPELLCHECK_LANGUAGES_FIRST + i, display_name);
  }

  // Add item in the sub menu to pop up the fonts and languages options menu.
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS);

  // Add 'Check the spelling of this field' item in the sub menu.
  AppendCheckboxMenuItem(
      IDC_CHECK_SPELLING_OF_THIS_FIELD,
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_CHECK_SPELLING_OF_THIS_FIELD));

  // Add option for showing the spelling panel if the platfrom spellchecker
  // supports it.
  if (SpellCheckerPlatform::SpellCheckerAvailable() &&
      SpellCheckerPlatform::SpellCheckerProvidesPanel()) {
    AppendCheckboxMenuItem(IDC_SPELLPANEL_TOGGLE,  l10n_util::GetStringUTF16(
              SpellCheckerPlatform::SpellingPanelVisible() ?
              IDS_CONTENT_CONTEXT_HIDE_SPELLING_PANEL :
              IDS_CONTENT_CONTEXT_SHOW_SPELLING_PANEL));
  }
  FinishSubMenu();

  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SELECTALL);
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenu::IsItemCommandEnabled(int id) const {
  // Allow Spell Check language items on sub menu for text area context menu.
  if ((id >= IDC_SPELLCHECK_LANGUAGES_FIRST) &&
      (id < IDC_SPELLCHECK_LANGUAGES_LAST)) {
    return profile_->GetPrefs()->GetBoolean(prefs::kEnableSpellCheck);
  }

  switch (id) {
    case IDS_CONTENT_CONTEXT_BACK:
      return source_tab_contents_->controller().CanGoBack();

    case IDS_CONTENT_CONTEXT_FORWARD:
      return source_tab_contents_->controller().CanGoForward();

    case IDS_CONTENT_CONTEXT_VIEWPAGESOURCE:
    case IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE:
    case IDS_CONTENT_CONTEXT_INSPECTELEMENT:
    // Viewing page info is not a delveloper command but is meaningful for the
    // same set of pages which developer commands are meaningful for.
    case IDS_CONTENT_CONTEXT_VIEWPAGEINFO:
      return IsDevCommandEnabled(id);

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
      GURL savable_url = (active_entry) ? active_entry->url() :
                                          GURL::EmptyGURL();
      return SavePackage::IsSavableURL(savable_url);
    }

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

    case IDS_CONTENT_CONTEXT_RELOAD:
    case IDS_CONTENT_CONTEXT_COPYIMAGE:
    case IDS_CONTENT_CONTEXT_PRINT:
    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDS_CONTENT_CONTEXT_GOTOURL:
    case IDC_SPELLCHECK_SUGGESTION_0:
    case IDC_SPELLCHECK_SUGGESTION_1:
    case IDC_SPELLCHECK_SUGGESTION_2:
    case IDC_SPELLCHECK_SUGGESTION_3:
    case IDC_SPELLCHECK_SUGGESTION_4:
    case IDC_SPELLCHECK_MENU:
    case IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
    case IDS_CONTENT_CONTEXT_VIEWFRAMEINFO:
      return true;

    case IDC_CHECK_SPELLING_OF_THIS_FIELD:
      return profile_->GetPrefs()->GetBoolean(prefs::kEnableSpellCheck);

    case IDS_CONTENT_CONTEXT_SAVEFRAMEAS:
    case IDS_CONTENT_CONTEXT_PRINTFRAME:
    case IDS_CONTENT_CONTEXT_ADDSEARCHENGINE:  // Not implemented.
    default:
      return false;
  }
}

bool RenderViewContextMenu::ItemIsChecked(int id) const {
  // See if the video is set to looping.
  if (id == IDS_CONTENT_CONTEXT_LOOP) {
    return (params_.media_flags &
            WebContextMenuData::MediaLoop) != 0;
  }

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
  return SpellChecker::GetSpellCheckLanguages(profile_, &languages) ==
      (id - IDC_SPELLCHECK_LANGUAGES_FIRST);
}

void RenderViewContextMenu::ExecuteItemCommand(int id) {
  // Check to see if one of the spell check language ids have been clicked.
  if (id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    const size_t language_number = id - IDC_SPELLCHECK_LANGUAGES_FIRST;
    std::vector<std::string> languages;
    SpellChecker::GetSpellCheckLanguages(profile_, &languages);
    if (language_number < languages.size()) {
      StringPrefMember dictionary_language;
      dictionary_language.Init(prefs::kSpellCheckDictionary,
          profile_->GetPrefs(), NULL);
      dictionary_language.SetValue(ASCIIToWide(languages[language_number]));
    }

    return;
  }

  switch (id) {
    case IDS_CONTENT_CONTEXT_OPENLINKNEWTAB:
      OpenURL(params_.link_url, NEW_BACKGROUND_TAB, PageTransition::LINK);
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
      UserMetrics::RecordAction(L"MediaContextMenu_Play", profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Play, true));
      break;

    case IDS_CONTENT_CONTEXT_PAUSE:
      UserMetrics::RecordAction(L"MediaContextMenu_Pause", profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Play, false));
      break;

    case IDS_CONTENT_CONTEXT_MUTE:
      UserMetrics::RecordAction(L"MediaContextMenu_Mute", profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Mute, true));
      break;

    case IDS_CONTENT_CONTEXT_UNMUTE:
      UserMetrics::RecordAction(L"MediaContextMenu_Unmute", profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Mute, false));
      break;

    case IDS_CONTENT_CONTEXT_LOOP:
      UserMetrics::RecordAction(L"MediaContextMenu_Loop", profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Loop,
                              !ItemIsChecked(IDS_CONTENT_CONTEXT_LOOP)));
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
      source_tab_contents_->controller().Reload(true);
      break;

    case IDS_CONTENT_CONTEXT_PRINT:
      source_tab_contents_->PrintPreview();
      break;

    case IDS_CONTENT_CONTEXT_VIEWPAGESOURCE:
      OpenURL(GURL("view-source:" + params_.page_url.spec()),
              NEW_FOREGROUND_TAB, PageTransition::GENERATED);
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
              NEW_FOREGROUND_TAB, PageTransition::GENERATED);
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
      OpenURL(selection_navigation_url_, NEW_FOREGROUND_TAB, transition_);
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
    case IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY:
      source_tab_contents_->render_view_host()->AddToDictionary(
          params_.misspelled_word);
      break;

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
  // Don't inspect inspector, new tab UI, etc.
  if (active_entry->url().SchemeIs(chrome::kChromeUIScheme) && !debug_mode)
    return false;

  // Don't inspect about:network, about:memory, etc.
  // However, we do want to inspect about:blank, which is often
  // used by ordinary web pages.
  if (active_entry->virtual_url().SchemeIs(chrome::kAboutScheme) &&
      !LowerCaseEqualsASCII(active_entry->virtual_url().path(), "blank"))
    return false;

  // Don't enable the web inspector if JavaScript is disabled
  if (id == IDS_CONTENT_CONTEXT_INSPECTELEMENT) {
    if (!profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled) ||
        command_line.HasSwitch(switches::kDisableJavaScript))
      return false;
  }

  return true;
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
