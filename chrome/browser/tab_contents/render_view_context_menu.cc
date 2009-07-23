// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu.h"

#include "app/l10n_util.h"
#include "base/clipboard.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_clipboard_writer.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/fonts_languages_window.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/platform_util.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

// Constants for the standard playback rates provided by the context
// menu. If another rate is reported, it will be considered unknown and
// no rate will be selected in the submenu.
static const double kSlowPlaybackRate = 0.5f;
static const double kNormalPlaybackRate = 1.0f;
static const double kFastPlaybackRate = 1.25f;
static const double kFasterPlaybackRate = 1.50f;
static const double kDoubleTimePlaybackRate = 2.0f;

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
  InitMenu(params_.node_type, params_.media_params);
  DoInit();
}

void RenderViewContextMenu::InitMenu(ContextNodeType node_type,
                                     ContextMenuMediaParams media_params) {
  if (node_type.type & ContextNodeType::PAGE)
    AppendPageItems();
  if (node_type.type & ContextNodeType::FRAME)
    AppendFrameItems();
  if (node_type.type & ContextNodeType::LINK)
    AppendLinkItems();

  if (node_type.type & ContextNodeType::IMAGE) {
    if (node_type.type & ContextNodeType::LINK)
      AppendSeparator();
    AppendImageItems();
  }

  if (node_type.type & ContextNodeType::VIDEO) {
    if (node_type.type & ContextNodeType::LINK)
      AppendSeparator();
    AppendVideoItems(media_params);
  }

  if (node_type.type & ContextNodeType::AUDIO) {
    if (node_type.type & ContextNodeType::LINK)
      AppendSeparator();
    AppendAudioItems(media_params);
  }

  if (node_type.type & ContextNodeType::EDITABLE)
    AppendEditableItems();
  else if (node_type.type & ContextNodeType::SELECTION ||
           node_type.type & ContextNodeType::LINK)
    AppendCopyItem();

  if (node_type.type & ContextNodeType::SELECTION)
    AppendSearchProvider();
  AppendSeparator();
  AppendDeveloperItems();
}

void RenderViewContextMenu::AppendDeveloperItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_INSPECTELEMENT);
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

void RenderViewContextMenu::AppendAudioItems(
    ContextMenuMediaParams media_params) {
  AppendMediaItems(media_params);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEAUDIOAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB);
}

void RenderViewContextMenu::AppendVideoItems(
    ContextMenuMediaParams media_params) {
  AppendMediaItems(media_params);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEVIDEOAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB);
}

void RenderViewContextMenu::AppendMediaItems(
    ContextMenuMediaParams media_params) {
  if (media_params.player_state & ContextMenuMediaParams::PAUSED) {
    AppendMenuItem(IDS_CONTENT_CONTEXT_PLAY);
  } else {
    AppendMenuItem(IDS_CONTENT_CONTEXT_PAUSE);
  }

  if (media_params.player_state & ContextMenuMediaParams::MUTED) {
    AppendMenuItem(IDS_CONTENT_CONTEXT_UNMUTE);
  } else {
    AppendMenuItem(IDS_CONTENT_CONTEXT_MUTE);
  }

  AppendCheckboxMenuItem(IDS_CONTENT_CONTEXT_LOOP,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_LOOP));

  StartSubMenu(IDS_CONTENT_CONTEXT_PLAYBACKRATE_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLAYBACKRATE_MENU));
  AppendRadioMenuItem(IDS_CONTENT_CONTEXT_PLAYBACKRATE_SLOW,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLAYBACKRATE_SLOW));
  AppendRadioMenuItem(IDS_CONTENT_CONTEXT_PLAYBACKRATE_NORMAL,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLAYBACKRATE_NORMAL));
  AppendRadioMenuItem(IDS_CONTENT_CONTEXT_PLAYBACKRATE_FAST,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLAYBACKRATE_FAST));
  AppendRadioMenuItem(IDS_CONTENT_CONTEXT_PLAYBACKRATE_FASTER,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLAYBACKRATE_FASTER));
  AppendRadioMenuItem(IDS_CONTENT_CONTEXT_PLAYBACKRATE_DOUBLETIME,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLAYBACKRATE_DOUBLETIME));
  FinishSubMenu();
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
  //AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEFRAMEAS);
  //AppendMenuItem(IDS_CONTENT_CONTEXT_PRINTFRAME);
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
  if (default_provider != NULL) {
    std::wstring selection_text =
        l10n_util::TruncateString(params_.selection_text, 50);
    if (!selection_text.empty()) {
      string16 label(WideToUTF16(
          l10n_util::GetStringF(IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
                                default_provider->short_name(),
                                selection_text)));
      AppendMenuItem(IDS_CONTENT_CONTEXT_SEARCHWEBFOR, label);
    }
  }
}

void RenderViewContextMenu::AppendEditableItems() {
  // Append Dictionary spell check suggestions.
  for (size_t i = 0; i < params_.dictionary_suggestions.size() &&
       IDC_SPELLCHECK_SUGGESTION_0 + i <= IDC_SPELLCHECK_SUGGESTION_LAST;
       ++i) {
    AppendMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + static_cast<int>(i),
                   WideToUTF16(params_.dictionary_suggestions[i]));
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
      // TODO (sidchat): Enable this option once this issue is resolved.
      if (params_.src_url.scheme() == chrome::kChromeUIScheme)
        return false;
      return true;

    case IDS_CONTENT_CONTEXT_FULLSCREEN:
      // TODO(ajwong): Enable fullsceren after we actually implement this.
      return false;

    // Media control commands should all be disabled if the player is in an
    // error state.
    case IDS_CONTENT_CONTEXT_PLAY:
    case IDS_CONTENT_CONTEXT_PAUSE:
    case IDS_CONTENT_CONTEXT_MUTE:
    case IDS_CONTENT_CONTEXT_UNMUTE:
    case IDS_CONTENT_CONTEXT_LOOP:
    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_MENU:
    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_SLOW:
    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_NORMAL:
    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_FAST:
    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_FASTER:
    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_DOUBLETIME:
      return (params_.media_params.player_state &
              ContextMenuMediaParams::IN_ERROR) == 0;

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
      return (params_.media_params.player_state &
              ContextMenuMediaParams::CAN_SAVE) &&
             params_.src_url.is_valid() &&
             URLRequest::IsHandledURL(params_.src_url);

    case IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB:
    case IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB:
      return true;

    case IDS_CONTENT_CONTEXT_SAVEPAGEAS:
      return SavePackage::IsSavableURL(source_tab_contents_->GetURL());

    case IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB:
    case IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW:
      return params_.frame_url.is_valid();

    case IDS_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & ContextNodeType::CAN_UNDO);

    case IDS_CONTENT_CONTEXT_REDO:
      return !!(params_.edit_flags & ContextNodeType::CAN_REDO);

    case IDS_CONTENT_CONTEXT_CUT:
      return !!(params_.edit_flags & ContextNodeType::CAN_CUT);

    case IDS_CONTENT_CONTEXT_COPY:
      return !!(params_.edit_flags & ContextNodeType::CAN_COPY);

    case IDS_CONTENT_CONTEXT_PASTE:
      return !!(params_.edit_flags & ContextNodeType::CAN_PASTE);

    case IDS_CONTENT_CONTEXT_DELETE:
      return !!(params_.edit_flags & ContextNodeType::CAN_DELETE);

    case IDS_CONTENT_CONTEXT_SELECTALL:
      return !!(params_.edit_flags & ContextNodeType::CAN_SELECT_ALL);

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
  // Select the correct playback rate.
  if (id == IDS_CONTENT_CONTEXT_PLAYBACKRATE_SLOW) {
    return params_.media_params.playback_rate == kSlowPlaybackRate;
  }
  if (id == IDS_CONTENT_CONTEXT_PLAYBACKRATE_NORMAL) {
    return params_.media_params.playback_rate == kNormalPlaybackRate;
  }
  if (id == IDS_CONTENT_CONTEXT_PLAYBACKRATE_FAST) {
    return params_.media_params.playback_rate == kFastPlaybackRate;
  }
  if (id == IDS_CONTENT_CONTEXT_PLAYBACKRATE_FASTER) {
    return params_.media_params.playback_rate == kFasterPlaybackRate;
  }
  if (id == IDS_CONTENT_CONTEXT_PLAYBACKRATE_DOUBLETIME) {
    return params_.media_params.playback_rate == kDoubleTimePlaybackRate;
  }

  // See if the video is set to looping.
  if (id == IDS_CONTENT_CONTEXT_LOOP) {
    return (params_.media_params.player_state &
            ContextMenuMediaParams::LOOP) != 0;
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
      OpenURL(params_.link_url, NEW_FOREGROUND_TAB, PageTransition::LINK);
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
      MediaPlayerActionAt(params_.x,
                          params_.y,
                          MediaPlayerAction(MediaPlayerAction::PLAY));
      break;

    case IDS_CONTENT_CONTEXT_PAUSE:
      MediaPlayerActionAt(params_.x,
                          params_.y,
                          MediaPlayerAction(MediaPlayerAction::PAUSE));
      break;

    case IDS_CONTENT_CONTEXT_MUTE:
      MediaPlayerActionAt(params_.x,
                        params_.y,
                        MediaPlayerAction(MediaPlayerAction::MUTE));
      break;

    case IDS_CONTENT_CONTEXT_UNMUTE:
      MediaPlayerActionAt(params_.x,
                          params_.y,
                          MediaPlayerAction(MediaPlayerAction::UNMUTE));
      break;

    case IDS_CONTENT_CONTEXT_LOOP:
      if (ItemIsChecked(IDS_CONTENT_CONTEXT_LOOP)) {
        MediaPlayerActionAt(params_.x,
                            params_.y,
                            MediaPlayerAction(MediaPlayerAction::NO_LOOP));
      } else {
        MediaPlayerActionAt(params_.x,
                            params_.y,
                            MediaPlayerAction(MediaPlayerAction::LOOP));
      }
      break;

    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_SLOW:
      MediaPlayerActionAt(
          params_.x,
          params_.y,
          MediaPlayerAction(MediaPlayerAction::SET_PLAYBACK_RATE,
                            kSlowPlaybackRate));
      break;

    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_NORMAL:
      MediaPlayerActionAt(
          params_.x,
          params_.y,
          MediaPlayerAction(MediaPlayerAction::SET_PLAYBACK_RATE,
                            kNormalPlaybackRate));
      break;

    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_FAST:
      MediaPlayerActionAt(
          params_.x,
          params_.y,
          MediaPlayerAction(MediaPlayerAction::SET_PLAYBACK_RATE,
                            kFastPlaybackRate));
      break;

    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_FASTER:
      MediaPlayerActionAt(
          params_.x,
          params_.y,
          MediaPlayerAction(MediaPlayerAction::SET_PLAYBACK_RATE,
                            kFasterPlaybackRate));
      break;

    case IDS_CONTENT_CONTEXT_PLAYBACKRATE_DOUBLETIME:
      MediaPlayerActionAt(
          params_.x,
          params_.y,
          MediaPlayerAction(MediaPlayerAction::SET_PLAYBACK_RATE,
                            kDoubleTimePlaybackRate));
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

    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR: {
      const TemplateURL* const default_provider =
          profile_->GetTemplateURLModel()->GetDefaultSearchProvider();
      DCHECK(default_provider);  // The context menu should not contain this
                                 // item when there is no provider.
      const TemplateURLRef* const search_url = default_provider->url();
      DCHECK(search_url->SupportsReplacement());
      OpenURL(GURL(WideToUTF8(search_url->ReplaceSearchTerms(*default_provider,
          params_.selection_text, TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
          std::wstring()))), NEW_FOREGROUND_TAB, PageTransition::GENERATED);
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
  if (active_entry->display_url().SchemeIs(chrome::kAboutScheme) &&
      !LowerCaseEqualsASCII(active_entry->display_url().path(), "blank"))
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

void RenderViewContextMenu::WriteTextToClipboard(const string16& text) {
  Clipboard* clipboard = g_browser_process->clipboard();

  if (!clipboard)
    return;

  ScopedClipboardWriter scw(clipboard);
  scw.WriteText(text);
}

void RenderViewContextMenu::WriteURLToClipboard(const GURL& url) {
  std::string utf8_text = url.SchemeIs(chrome::kMailToScheme) ? url.path() :
      // Unescaping path and query is not a good idea because other
      // applications may not enocode non-ASCII characters in UTF-8.
      // So the 4th parameter of net::FormatUrl() should be false.
      // See crbug.com/2820.
      WideToUTF8(net::FormatUrl(
                 url, profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
                 false, UnescapeRule::NONE, NULL, NULL));

  WriteTextToClipboard(UTF8ToUTF16(utf8_text));
  DidWriteURLToClipboard(utf8_text);
}

void RenderViewContextMenu::MediaPlayerActionAt(
    int x,
    int y,
    const MediaPlayerAction& action) {
  source_tab_contents_->render_view_host()->MediaPlayerActionAt(x, y, action);
}
