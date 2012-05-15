// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit.h"

#include <string>

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/proto/omnibox_event.pb.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

using content::UserMetricsAction;
using predictors::AutocompleteActionPredictor;
using predictors::AutocompleteActionPredictorFactory;

///////////////////////////////////////////////////////////////////////////////
// AutocompleteEditController

AutocompleteEditController::~AutocompleteEditController() {
}

///////////////////////////////////////////////////////////////////////////////
// AutocompleteEditModel::State

AutocompleteEditModel::State::State(bool user_input_in_progress,
                                    const string16& user_text,
                                    const string16& keyword,
                                    bool is_keyword_hint)
    : user_input_in_progress(user_input_in_progress),
      user_text(user_text),
      keyword(keyword),
      is_keyword_hint(is_keyword_hint) {
}

AutocompleteEditModel::State::~State() {
}

///////////////////////////////////////////////////////////////////////////////
// AutocompleteEditModel

AutocompleteEditModel::AutocompleteEditModel(
    OmniboxView* view,
    AutocompleteEditController* controller,
    Profile* profile)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        autocomplete_controller_(new AutocompleteController(profile, this))),
      view_(view),
      popup_(NULL),
      controller_(controller),
      has_focus_(false),
      user_input_in_progress_(false),
      just_deleted_text_(false),
      has_temporary_text_(false),
      paste_state_(NONE),
      control_key_state_(UP),
      is_keyword_hint_(false),
      profile_(profile),
      in_revert_(false),
      allow_exact_keyword_match_(false),
      instant_complete_behavior_(INSTANT_COMPLETE_DELAYED) {
}

AutocompleteEditModel::~AutocompleteEditModel() {
}

const AutocompleteEditModel::State
    AutocompleteEditModel::GetStateForTabSwitch() {
  // Like typing, switching tabs "accepts" the temporary text as the user
  // text, because it makes little sense to have temporary text when the
  // popup is closed.
  if (user_input_in_progress_) {
    // Weird edge case to match other browsers: if the edit is empty, revert to
    // the permanent text (so the user can get it back easily) but select it (so
    // on switching back, typing will "just work").
    const string16 user_text(UserTextFromDisplayText(view_->GetText()));
    if (user_text.empty()) {
      view_->RevertAll();
      view_->SelectAll(true);
    } else {
      InternalSetUserText(user_text);
    }
  }

  return State(user_input_in_progress_, user_text_, keyword_, is_keyword_hint_);
}

void AutocompleteEditModel::RestoreState(const State& state) {
  // Restore any user editing.
  if (state.user_input_in_progress) {
    // NOTE: Be sure and set keyword-related state BEFORE invoking
    // DisplayTextFromUserText(), as its result depends upon this state.
    keyword_ = state.keyword;
    is_keyword_hint_ = state.is_keyword_hint;
    view_->SetUserText(state.user_text,
        DisplayTextFromUserText(state.user_text), false);
  }
}

AutocompleteMatch AutocompleteEditModel::CurrentMatch() {
  AutocompleteMatch match;
  GetInfoForCurrentText(&match, NULL);
  return match;
}

bool AutocompleteEditModel::UpdatePermanentText(
    const string16& new_permanent_text) {
  // When there's a new URL, and the user is not editing anything or the edit
  // doesn't have focus, we want to revert the edit to show the new URL.  (The
  // common case where the edit doesn't have focus is when the user has started
  // an edit and then abandoned it and clicked a link on the page.)
  const bool visibly_changed_permanent_text =
      (permanent_text_ != new_permanent_text) &&
      (!user_input_in_progress_ || !has_focus_);

  permanent_text_ = new_permanent_text;
  return visibly_changed_permanent_text;
}

GURL AutocompleteEditModel::PermanentURL() {
  return URLFixerUpper::FixupURL(UTF16ToUTF8(permanent_text_), std::string());
}

void AutocompleteEditModel::SetUserText(const string16& text) {
  SetInputInProgress(true);
  InternalSetUserText(text);
  paste_state_ = NONE;
  has_temporary_text_ = false;
}

void AutocompleteEditModel::FinalizeInstantQuery(
    const string16& input_text,
    const string16& suggest_text,
    bool skip_inline_autocomplete) {
  if (skip_inline_autocomplete) {
    const string16 final_text = input_text + suggest_text;
    view_->OnBeforePossibleChange();
    view_->SetWindowTextAndCaretPos(final_text, final_text.length(), false,
        false);
    view_->OnAfterPossibleChange();
  } else if (popup_->IsOpen()) {
    SearchProvider* search_provider =
        autocomplete_controller_->search_provider();
    // There may be no providers during testing; guard against that.
    if (search_provider)
      search_provider->FinalizeInstantQuery(input_text, suggest_text);
  }
}

void AutocompleteEditModel::SetSuggestedText(
    const string16& text,
    InstantCompleteBehavior behavior) {
  instant_complete_behavior_ = behavior;
  if (instant_complete_behavior_ == INSTANT_COMPLETE_NOW) {
    if (!text.empty())
      FinalizeInstantQuery(view_->GetText(), text, false);
    else
      view_->SetInstantSuggestion(text, false);
  } else {
    DCHECK((behavior == INSTANT_COMPLETE_DELAYED) ||
           (behavior == INSTANT_COMPLETE_NEVER));
    view_->SetInstantSuggestion(text, behavior == INSTANT_COMPLETE_DELAYED);
  }
}

bool AutocompleteEditModel::CommitSuggestedText(bool skip_inline_autocomplete) {
  if (!controller_->GetInstant())
    return false;

  const string16 suggestion = view_->GetInstantSuggestion();
  if (suggestion.empty())
    return false;

  FinalizeInstantQuery(view_->GetText(), suggestion, skip_inline_autocomplete);
  return true;
}

bool AutocompleteEditModel::AcceptCurrentInstantPreview() {
  return InstantController::CommitIfCurrent(controller_->GetInstant());
}

void AutocompleteEditModel::OnChanged() {
  // Don't call CurrentMatch() when there's no editing, as in this case we'll
  // never actually use it.  This avoids running the autocomplete providers (and
  // any systems they then spin up) during startup.
  const AutocompleteMatch& current_match = user_input_in_progress_ ?
      CurrentMatch() : AutocompleteMatch();

  AutocompleteActionPredictor::Action recommended_action =
      AutocompleteActionPredictor::ACTION_NONE;
  AutocompleteActionPredictor* action_predictor =
      user_input_in_progress_ ?
      AutocompleteActionPredictorFactory::GetForProfile(profile_) : NULL;
  if (action_predictor) {
    action_predictor->RegisterTransitionalMatches(user_text_, result());
    // Confer with the AutocompleteActionPredictor to determine what action, if
    // any, we should take. Get the recommended action here even if we don't
    // need it so we can get stats for anyone who is opted in to UMA, but only
    // get it if the user has actually typed something to avoid constructing it
    // before it's needed. Note: This event is triggered as part of startup when
    // the initial tab transitions to the start page.
    recommended_action =
        action_predictor->RecommendAction(user_text_, current_match);
  }

  UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.Action",
                            recommended_action,
                            AutocompleteActionPredictor::LAST_PREDICT_ACTION);
  string16 suggested_text;

  if (DoInstant(current_match, &suggested_text)) {
    SetSuggestedText(suggested_text, instant_complete_behavior_);
  } else {
    switch (recommended_action) {
      case AutocompleteActionPredictor::ACTION_PRERENDER:
        DoPrerender(current_match);
        break;
      case AutocompleteActionPredictor::ACTION_PRECONNECT:
        DoPreconnect(current_match);
        break;
      case AutocompleteActionPredictor::ACTION_NONE:
        break;
      default:
        NOTREACHED() << "Unexpected recommended action: " << recommended_action;
        break;
    }

    // Hide any suggestions we might be showing.
    view_->SetInstantSuggestion(string16(), false);

    // No need to wait any longer for instant.
    FinalizeInstantQuery(string16(), string16(), false);
  }

  controller_->OnChanged();
}

void AutocompleteEditModel::GetDataForURLExport(GURL* url,
                                                string16* title,
                                                SkBitmap* favicon) {
  AutocompleteMatch match;
  GetInfoForCurrentText(&match, NULL);
  *url = match.destination_url;
  if (*url == URLFixerUpper::FixupURL(UTF16ToUTF8(permanent_text_),
                                      std::string())) {
    *title = controller_->GetTitle();
    *favicon = controller_->GetFavicon();
  }
}

bool AutocompleteEditModel::UseVerbatimInstant() {
#if defined(OS_MACOSX)
  // TODO(suzhe): Fix Mac port to display Instant suggest in a separated NSView,
  // so that we can display instant suggest along with composition text.
  const AutocompleteInput& input = autocomplete_controller_->input();
  if (input.prevent_inline_autocomplete())
    return true;
#endif

  // The value of input.prevent_inline_autocomplete() is determined by
  // following conditions:
  // 1. If the caret is at the end of the text (checked below).
  // 2. If it's in IME composition mode.
  // As we use a separated widget for displaying the instant suggest, it won't
  // interfere with IME composition, so we don't need to care about the value of
  // input.prevent_inline_autocomplete() here.
  if (view_->DeleteAtEndPressed() || (popup_->selected_line() != 0) ||
      just_deleted_text_)
    return true;

  size_t start, end;
  view_->GetSelectionBounds(&start, &end);
  return (start != end) || (start != view_->GetText().length());
}

string16 AutocompleteEditModel::GetDesiredTLD() const {
  // Tricky corner case: The user has typed "foo" and currently sees an inline
  // autocomplete suggestion of "foo.net".  He now presses ctrl-a (e.g. to
  // select all, on Windows).  If we treat the ctrl press as potentially for the
  // sake of ctrl-enter, then we risk "www.foo.com" being promoted as the best
  // match.  This would make the autocompleted text disappear, leaving our user
  // feeling very confused when the wrong text gets highlighted.
  //
  // Thus, we only treat the user as pressing ctrl-enter when the user presses
  // ctrl without any fragile state built up in the omnibox:
  // * the contents of the omnibox have not changed since the keypress,
  // * there is no autocompleted text visible, and
  // * the user is not typing a keyword query.
  return (control_key_state_ == DOWN_WITHOUT_CHANGE &&
          inline_autocomplete_text_.empty() && !KeywordIsSelected())?
    ASCIIToUTF16("com") : string16();
}

bool AutocompleteEditModel::CurrentTextIsURL() const {
  // If !user_input_in_progress_, the permanent text is showing, which should
  // always be a URL, so no further checking is needed.  By avoiding checking in
  // this case, we avoid calling into the autocomplete providers, and thus
  // initializing the history system, as long as possible, which speeds startup.
  if (!user_input_in_progress_)
    return true;

  AutocompleteMatch match;
  GetInfoForCurrentText(&match, NULL);
  return match.transition == content::PAGE_TRANSITION_TYPED;
}

AutocompleteMatch::Type AutocompleteEditModel::CurrentTextType() const {
  AutocompleteMatch match;
  GetInfoForCurrentText(&match, NULL);
  return match.type;
}

void AutocompleteEditModel::AdjustTextForCopy(int sel_min,
                                              bool is_all_selected,
                                              string16* text,
                                              GURL* url,
                                              bool* write_url) {
  *write_url = false;

  if (sel_min != 0)
    return;

  if (!user_input_in_progress_ && is_all_selected) {
    // The user selected all the text and has not edited it. Use the url as the
    // text so that if the scheme was stripped it's added back, and the url
    // is unescaped (we escape parts of the url for display).
    *url = PermanentURL();
    *text = UTF8ToUTF16(url->spec());
    *write_url = true;
    return;
  }

  // We can't use CurrentTextIsURL() or GetDataForURLExport() because right now
  // the user is probably holding down control to cause the copy, which will
  // screw up our calculation of the desired_tld.
  AutocompleteMatch match;
  profile_->GetAutocompleteClassifier()->Classify(*text, string16(),
        KeywordIsSelected(), true, &match, NULL);
  if (match.transition != content::PAGE_TRANSITION_TYPED)
    return;
  *url = match.destination_url;

  // Prefix the text with 'http://' if the text doesn't start with 'http://',
  // the text parses as a url with a scheme of http, the user selected the
  // entire host, and the user hasn't edited the host or manually removed the
  // scheme.
  GURL perm_url(PermanentURL());
  if (perm_url.SchemeIs(chrome::kHttpScheme) &&
      url->SchemeIs(chrome::kHttpScheme) && perm_url.host() == url->host()) {
    *write_url = true;
    string16 http = ASCIIToUTF16(chrome::kHttpScheme) +
        ASCIIToUTF16(content::kStandardSchemeSeparator);
    if (text->compare(0, http.length(), http) != 0)
      *text = http + *text;
  }
}

void AutocompleteEditModel::SetInputInProgress(bool in_progress) {
  if (user_input_in_progress_ == in_progress)
    return;

  user_input_in_progress_ = in_progress;
  if (user_input_in_progress_)
    time_user_first_modified_omnibox_ = base::TimeTicks::Now();
  controller_->OnInputInProgress(in_progress);
}

void AutocompleteEditModel::Revert() {
  SetInputInProgress(false);
  paste_state_ = NONE;
  InternalSetUserText(string16());
  keyword_.clear();
  is_keyword_hint_ = false;
  has_temporary_text_ = false;
  view_->SetWindowTextAndCaretPos(permanent_text_,
                                  has_focus_ ? permanent_text_.length() : 0,
                                  false, true);
  AutocompleteActionPredictor* action_predictor =
      AutocompleteActionPredictorFactory::GetForProfile(profile_);
  if (action_predictor)
    action_predictor->ClearTransitionalMatches();
}

void AutocompleteEditModel::StartAutocomplete(
    bool has_selected_text,
    bool prevent_inline_autocomplete) const {
  ClearPopupKeywordMode();

  bool keyword_is_selected = KeywordIsSelected();
  popup_->SetHoveredLine(AutocompletePopupModel::kNoMatch);
  // We don't explicitly clear AutocompletePopupModel::manually_selected_match,
  // as Start ends up invoking AutocompletePopupModel::OnResultChanged which
  // clears it.
  autocomplete_controller_->Start(
      user_text_, GetDesiredTLD(),
      prevent_inline_autocomplete || just_deleted_text_ ||
      (has_selected_text && inline_autocomplete_text_.empty()) ||
      (paste_state_ != NONE), keyword_is_selected,
      keyword_is_selected || allow_exact_keyword_match_,
      AutocompleteInput::ALL_MATCHES);
}

void AutocompleteEditModel::StopAutocomplete() {
  if (popup_->IsOpen() && !in_revert_) {
    InstantController* instant = controller_->GetInstant();
    if (instant && !instant->commit_on_mouse_up())
      instant->DestroyPreviewContents();
  }

  autocomplete_controller_->Stop(true);
}

bool AutocompleteEditModel::CanPasteAndGo(const string16& text) const {
  if (!view_->GetCommandUpdater()->IsCommandEnabled(IDC_OPEN_CURRENT_URL))
    return false;

  profile_->GetAutocompleteClassifier()->Classify(text, string16(),
      false, false, &paste_and_go_match_, &paste_and_go_alternate_nav_url_);
  return paste_and_go_match_.destination_url.is_valid();
}

void AutocompleteEditModel::PasteAndGo() {
  view_->RevertAll();
  view_->OpenMatch(paste_and_go_match_, CURRENT_TAB,
      paste_and_go_alternate_nav_url_, AutocompletePopupModel::kNoMatch);
}

void AutocompleteEditModel::AcceptInput(WindowOpenDisposition disposition,
                                        bool for_drop) {
  // Get the URL and transition type for the selected entry.
  AutocompleteMatch match;
  GURL alternate_nav_url;
  GetInfoForCurrentText(&match, &alternate_nav_url);

  if (!match.destination_url.is_valid())
    return;

  if ((match.transition == content::PAGE_TRANSITION_TYPED) &&
      (match.destination_url ==
       URLFixerUpper::FixupURL(UTF16ToUTF8(permanent_text_), std::string()))) {
    // When the user hit enter on the existing permanent URL, treat it like a
    // reload for scoring purposes.  We could detect this by just checking
    // user_input_in_progress_, but it seems better to treat "edits" that end
    // up leaving the URL unchanged (e.g. deleting the last character and then
    // retyping it) as reloads too.  We exclude non-TYPED transitions because if
    // the transition is GENERATED, the user input something that looked
    // different from the current URL, even if it wound up at the same place
    // (e.g. manually retyping the same search query), and it seems wrong to
    // treat this as a reload.
    match.transition = content::PAGE_TRANSITION_RELOAD;
  } else if (for_drop || ((paste_state_ != NONE) &&
                          match.is_history_what_you_typed_match)) {
    // When the user pasted in a URL and hit enter, score it like a link click
    // rather than a normal typed URL, so it doesn't get inline autocompleted
    // as aggressively later.
    match.transition = content::PAGE_TRANSITION_LINK;
  }

  const TemplateURL* template_url = match.GetTemplateURL(profile_);
  if (template_url && template_url->url_ref().HasGoogleBaseURLs())
    GoogleURLTracker::GoogleURLSearchCommitted(profile_);

  view_->OpenMatch(match, disposition, alternate_nav_url,
                   AutocompletePopupModel::kNoMatch);
}

void AutocompleteEditModel::OpenMatch(const AutocompleteMatch& match,
                                      WindowOpenDisposition disposition,
                                      const GURL& alternate_nav_url,
                                      size_t index) {
  // We only care about cases where there is a selection (i.e. the popup is
  // open).
  if (popup_->IsOpen()) {
    AutocompleteLog log(
        autocomplete_controller_->input().text(),
        just_deleted_text_,
        autocomplete_controller_->input().type(),
        popup_->selected_line(),
        -1,  // don't yet know tab ID; set later if appropriate
        ClassifyPage(controller_->GetTabContentsWrapper()->
                     web_contents()->GetURL()),
        base::TimeTicks::Now() - time_user_first_modified_omnibox_,
        0,  // inline autocomplete length; possibly set later
        result());
    DCHECK(user_input_in_progress_) << "We didn't get here through the "
        "expected series of calls.  time_user_first_modified_omnibox_ is "
        "not set correctly and other things may be wrong.";
    if (index != AutocompletePopupModel::kNoMatch)
      log.selected_index = index;
    else if (!has_temporary_text_)
      log.inline_autocompleted_length = inline_autocomplete_text_.length();
    if (disposition == CURRENT_TAB) {
      // If we know the destination is being opened in the current tab,
      // we can easily get the tab ID.  (If it's being opened in a new
      // tab, we don't know the tab ID yet.)
      log.tab_id = controller_->GetTabContentsWrapper()->
          restore_tab_helper()->session_id().id();
    }
    autocomplete_controller_->AddProvidersInfo(&log.providers_info);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
        content::Source<Profile>(profile_),
        content::Details<AutocompleteLog>(&log));
  }

  TemplateURL* template_url = match.GetTemplateURL(profile_);
  if (template_url) {
    if (match.transition == content::PAGE_TRANSITION_KEYWORD) {
      // The user is using a non-substituting keyword or is explicitly in
      // keyword mode.

      // Special case for extension keywords. Don't increment usage count for
      // these.
      if (template_url->IsExtensionKeyword()) {
        AutocompleteMatch current_match;
        GetInfoForCurrentText(&current_match, NULL);

        const AutocompleteMatch& match =
            (index == AutocompletePopupModel::kNoMatch) ?
                current_match : result().match_at(index);

        // Strip the keyword + leading space off the input.
        size_t prefix_length = match.keyword.length() + 1;
        extensions::ExtensionOmniboxEventRouter::OnInputEntered(profile_,
            template_url->GetExtensionId(),
            UTF16ToUTF8(match.fill_into_edit.substr(prefix_length)));
        view_->RevertAll();
        return;
      }

      content::RecordAction(UserMetricsAction("AcceptedKeyword"));
      TemplateURLServiceFactory::GetForProfile(profile_)->IncrementUsageCount(
          template_url);
    } else {
      DCHECK_EQ(content::PAGE_TRANSITION_GENERATED, match.transition);
      // NOTE: We purposefully don't increment the usage count of the default
      // search engine here like we do for explicit keywords above; see comments
      // in template_url.h.
    }

    // NOTE: Non-prepopulated engines will all have ID 0, which is fine as
    // the prepopulate IDs start at 1.  Distribution-specific engines will
    // all have IDs above the maximum, and will be automatically lumped
    // together in an "overflow" bucket in the histogram.
    UMA_HISTOGRAM_ENUMERATION("Omnibox.SearchEngine",
        template_url->prepopulate_id(),
        TemplateURLPrepopulateData::kMaxPrepopulatedEngineID);
  }

  if (disposition != NEW_BACKGROUND_TAB) {
    in_revert_ = true;
    view_->RevertAll();  // Revert the box to its unedited state
  }

  if (match.type == AutocompleteMatch::EXTENSION_APP) {
    extensions::LaunchAppFromOmnibox(match, profile_, disposition);
  } else {
    controller_->OnAutocompleteAccept(match.destination_url, disposition,
                                      match.transition, alternate_nav_url);
  }

  if (match.starred)
    bookmark_utils::RecordBookmarkLaunch(bookmark_utils::LAUNCH_OMNIBOX);

  InstantController* instant = controller_->GetInstant();
  if (instant && !popup_->IsOpen())
    instant->DestroyPreviewContents();
  in_revert_ = false;
}

bool AutocompleteEditModel::AcceptKeyword() {
  DCHECK(is_keyword_hint_ && !keyword_.empty());

  autocomplete_controller_->Stop(false);
  is_keyword_hint_ = false;

  if (popup_->IsOpen())
    popup_->SetSelectedLineState(AutocompletePopupModel::KEYWORD);
  else
    StartAutocomplete(false, true);

  // Ensure the current selection is saved before showing keyword mode
  // so that moving to another line and then reverting the text will restore
  // the current state properly.
  view_->OnTemporaryTextMaybeChanged(
      DisplayTextFromUserText(CurrentMatch().fill_into_edit),
      !has_temporary_text_);
  has_temporary_text_ = true;

  content::RecordAction(UserMetricsAction("AcceptedKeywordHint"));
  return true;
}

void AutocompleteEditModel::ClearKeyword(const string16& visible_text) {
  autocomplete_controller_->Stop(false);
  ClearPopupKeywordMode();

  const string16 window_text(keyword_ + visible_text);

  // Only reset the result if the edit text has changed since the
  // keyword was accepted, or if the popup is closed.
  if (just_deleted_text_ || !visible_text.empty() || !popup_->IsOpen()) {
    view_->OnBeforePossibleChange();
    view_->SetWindowTextAndCaretPos(window_text.c_str(), keyword_.length(),
        false, false);
    keyword_.clear();
    is_keyword_hint_ = false;
    view_->OnAfterPossibleChange();
    just_deleted_text_ = true;  // OnAfterPossibleChange() fails to clear this
                                // since the edit contents have actually grown
                                // longer.
  } else {
    is_keyword_hint_ = true;
    view_->SetWindowTextAndCaretPos(window_text.c_str(), keyword_.length(),
        false, true);
  }
}

const AutocompleteResult& AutocompleteEditModel::result() const {
  return autocomplete_controller_->result();
}

void AutocompleteEditModel::OnSetFocus(bool control_down) {
  has_focus_ = true;
  control_key_state_ = control_down ? DOWN_WITHOUT_CHANGE : UP;

  InstantController* instant = controller_->GetInstant();
  TabContentsWrapper* tab = controller_->GetTabContentsWrapper();
  if (instant && tab)
    instant->OnAutocompleteGotFocus(tab);
}

void AutocompleteEditModel::OnWillKillFocus(
    gfx::NativeView view_gaining_focus) {
  SetSuggestedText(string16(), INSTANT_COMPLETE_NOW);

  InstantController* instant = controller_->GetInstant();
  if (instant)
    instant->OnAutocompleteLostFocus(view_gaining_focus);
}

void AutocompleteEditModel::OnKillFocus() {
  has_focus_ = false;
  control_key_state_ = UP;
  paste_state_ = NONE;
}

bool AutocompleteEditModel::OnEscapeKeyPressed() {
  if (has_temporary_text_) {
    AutocompleteMatch match;
    InfoForCurrentSelection(&match, NULL);
    if (match.destination_url != original_url_) {
      RevertTemporaryText(true);
      return true;
    }
  }

  // If the user wasn't editing, but merely had focus in the edit, allow <esc>
  // to be processed as an accelerator, so it can still be used to stop a load.
  // When the permanent text isn't all selected we still fall through to the
  // SelectAll() call below so users can arrow around in the text and then hit
  // <esc> to quickly replace all the text; this matches IE.
  if (!user_input_in_progress_ && view_->IsSelectAll())
    return false;

  view_->RevertAll();
  view_->SelectAll(true);
  return true;
}

void AutocompleteEditModel::OnControlKeyChanged(bool pressed) {
  // Don't change anything unless the key state is actually toggling.
  if (pressed == (control_key_state_ == UP)) {
    ControlKeyState old_state = control_key_state_;
    control_key_state_ = pressed ? DOWN_WITHOUT_CHANGE : UP;
    if ((control_key_state_ == DOWN_WITHOUT_CHANGE) && has_temporary_text_) {
      // Arrowing down and then hitting control accepts the temporary text as
      // the input text.
      InternalSetUserText(UserTextFromDisplayText(view_->GetText()));
      has_temporary_text_ = false;
      if (KeywordIsSelected())
        AcceptKeyword();
    }
    if ((old_state != DOWN_WITH_CHANGE) && popup_->IsOpen()) {
      // Autocomplete history provider results may change, so refresh the
      // popup.  This will force user_input_in_progress_ to true, but if the
      // popup is open, that should have already been the case.
      view_->UpdatePopup();
    }
  }
}

void AutocompleteEditModel::OnUpOrDownKeyPressed(int count) {
  // NOTE: This purposefully don't trigger any code that resets paste_state_.

  if (!popup_->IsOpen()) {
    if (!query_in_progress()) {
      // The popup is neither open nor working on a query already.  So, start an
      // autocomplete query for the current text.  This also sets
      // user_input_in_progress_ to true, which we want: if the user has started
      // to interact with the popup, changing the permanent_text_ shouldn't
      // change the displayed text.
      // Note: This does not force the popup to open immediately.
      // TODO(pkasting): We should, in fact, force this particular query to open
      // the popup immediately.
      if (!user_input_in_progress_)
        InternalSetUserText(permanent_text_);
      view_->UpdatePopup();
    } else {
      // TODO(pkasting): The popup is working on a query but is not open.  We
      // should force it to open immediately.
    }
  } else {
    // The popup is open, so the user should be able to interact with it
    // normally.
    popup_->Move(count);
  }
}

void AutocompleteEditModel::OnPopupDataChanged(
    const string16& text,
    GURL* destination_for_temporary_text_change,
    const string16& keyword,
    bool is_keyword_hint) {
  // Update keyword/hint-related local state.
  bool keyword_state_changed = (keyword_ != keyword) ||
      ((is_keyword_hint_ != is_keyword_hint) && !keyword.empty());
  if (keyword_state_changed) {
    keyword_ = keyword;
    is_keyword_hint_ = is_keyword_hint;

    // |is_keyword_hint_| should always be false if |keyword_| is empty.
    DCHECK(!keyword_.empty() || !is_keyword_hint_);
  }

  // Handle changes to temporary text.
  if (destination_for_temporary_text_change != NULL) {
    const bool save_original_selection = !has_temporary_text_;
    if (save_original_selection) {
      // Save the original selection and URL so it can be reverted later.
      has_temporary_text_ = true;
      original_url_ = *destination_for_temporary_text_change;
      inline_autocomplete_text_.clear();
    }
    if (control_key_state_ == DOWN_WITHOUT_CHANGE) {
      // Arrowing around the popup cancels control-enter.
      control_key_state_ = DOWN_WITH_CHANGE;
      // Now things are a bit screwy: the desired_tld has changed, but if we
      // update the popup, the new order of entries won't match the old, so the
      // user's selection gets screwy; and if we don't update the popup, and the
      // user reverts, then the selected item will be as if control is still
      // pressed, even though maybe it isn't any more.  There is no obvious
      // right answer here :(
    }
    view_->OnTemporaryTextMaybeChanged(DisplayTextFromUserText(text),
                                       save_original_selection);
    return;
  }

  bool call_controller_onchanged = true;
  inline_autocomplete_text_ = text;
  if (view_->OnInlineAutocompleteTextMaybeChanged(
      DisplayTextFromUserText(user_text_ + inline_autocomplete_text_),
      DisplayTextFromUserText(user_text_).length()))
    call_controller_onchanged = false;

  // If |has_temporary_text_| is true, then we previously had a manual selection
  // but now don't (or |destination_for_temporary_text_change| would have been
  // non-NULL). This can happen when deleting the selected item in the popup.
  // In this case, we've already reverted the popup to the default match, so we
  // need to revert ourselves as well.
  if (has_temporary_text_) {
    RevertTemporaryText(false);
    call_controller_onchanged = false;
  }

  // We need to invoke OnChanged in case the destination url changed (as could
  // happen when control is toggled).
  if (call_controller_onchanged)
    OnChanged();
}

bool AutocompleteEditModel::OnAfterPossibleChange(
    const string16& old_text,
    const string16& new_text,
    size_t selection_start,
    size_t selection_end,
    bool selection_differs,
    bool text_differs,
    bool just_deleted_text,
    bool allow_keyword_ui_change) {
  // Update the paste state as appropriate: if we're just finishing a paste
  // that replaced all the text, preserve that information; otherwise, if we've
  // made some other edit, clear paste tracking.
  if (paste_state_ == PASTING)
    paste_state_ = PASTED;
  else if (text_differs)
    paste_state_ = NONE;

  // Modifying the selection counts as accepting the autocompleted text.
  const bool user_text_changed =
      text_differs || (selection_differs && !inline_autocomplete_text_.empty());

  // If something has changed while the control key is down, prevent
  // "ctrl-enter" until the control key is released.  When we do this, we need
  // to update the popup if it's open, since the desired_tld will have changed.
  if ((text_differs || selection_differs) &&
      (control_key_state_ == DOWN_WITHOUT_CHANGE)) {
    control_key_state_ = DOWN_WITH_CHANGE;
    if (!text_differs && !popup_->IsOpen())
      return false;  // Don't open the popup for no reason.
  } else if (!user_text_changed) {
    return false;
  }

  // If the user text has not changed, we do not want to change the model's
  // state associated with the text.  Otherwise, we can get surprising behavior
  // where the autocompleted text unexpectedly reappears, e.g. crbug.com/55983
  if (user_text_changed) {
    InternalSetUserText(UserTextFromDisplayText(new_text));
    has_temporary_text_ = false;

    // Track when the user has deleted text so we won't allow inline
    // autocomplete.
    just_deleted_text_ = just_deleted_text;
  }

  const bool no_selection = selection_start == selection_end;

  // Update the popup for the change, in the process changing to keyword mode
  // if the user hit space in mid-string after a keyword.
  // |allow_exact_keyword_match_| will be used by StartAutocomplete() method,
  // which will be called by |view_->UpdatePopup()|; so after that returns we
  // can safely reset this flag.
  allow_exact_keyword_match_ = text_differs && allow_keyword_ui_change &&
      !just_deleted_text && no_selection &&
      CreatedKeywordSearchByInsertingSpaceInMiddle(old_text, user_text_,
                                                   selection_start);
  view_->UpdatePopup();
  allow_exact_keyword_match_ = false;

  // Change to keyword mode if the user is now pressing space after a keyword
  // name.  Note that if this is the case, then even if there was no keyword
  // hint when we entered this function (e.g. if the user has used space to
  // replace some selected text that was adjoined to this keyword), there will
  // be one now because of the call to UpdatePopup() above; so it's safe for
  // MaybeAcceptKeywordBySpace() to look at |keyword_| and |is_keyword_hint_| to
  // determine what keyword, if any, is applicable.
  //
  // If MaybeAcceptKeywordBySpace() accepts the keyword and returns true, that
  // will have updated our state already, so in that case we don't also return
  // true from this function.
  return !(text_differs && allow_keyword_ui_change && !just_deleted_text &&
           no_selection && (selection_start == user_text_.length()) &&
           MaybeAcceptKeywordBySpace(user_text_));
}

void AutocompleteEditModel::PopupBoundsChangedTo(const gfx::Rect& bounds) {
  InstantController* instant = controller_->GetInstant();
  if (instant)
    instant->SetOmniboxBounds(bounds);
}

void AutocompleteEditModel::OnResultChanged(bool default_match_changed) {
  const bool was_open = popup_->IsOpen();
  if (default_match_changed) {
    string16 inline_autocomplete_text;
    string16 keyword;
    bool is_keyword_hint = false;
    const AutocompleteResult& result = this->result();
    const AutocompleteResult::const_iterator match(result.default_match());
    if (match != result.end()) {
      if ((match->inline_autocomplete_offset != string16::npos) &&
          (match->inline_autocomplete_offset <
           match->fill_into_edit.length())) {
        inline_autocomplete_text =
            match->fill_into_edit.substr(match->inline_autocomplete_offset);
      }

      if (!prerender::IsOmniboxEnabled(profile_))
        DoPreconnect(*match);

      // We could prefetch the alternate nav URL, if any, but because there
      // can be many of these as a user types an initial series of characters,
      // the OS DNS cache could suffer eviction problems for minimal gain.

      match->GetKeywordUIState(profile_, &keyword, &is_keyword_hint);
    }

    popup_->OnResultChanged();
    OnPopupDataChanged(inline_autocomplete_text, NULL, keyword,
                       is_keyword_hint);
  } else {
    popup_->OnResultChanged();
  }

  if (popup_->IsOpen()) {
    PopupBoundsChangedTo(popup_->view()->GetTargetBounds());
  } else if (was_open) {
    // Accepts the temporary text as the user text, because it makes little
    // sense to have temporary text when the popup is closed.
    InternalSetUserText(UserTextFromDisplayText(view_->GetText()));
    has_temporary_text_ = false;
    PopupBoundsChangedTo(gfx::Rect());
  }
}

bool AutocompleteEditModel::query_in_progress() const {
  return !autocomplete_controller_->done();
}

void AutocompleteEditModel::InternalSetUserText(const string16& text) {
  user_text_ = text;
  just_deleted_text_ = false;
  inline_autocomplete_text_.clear();
}

bool AutocompleteEditModel::KeywordIsSelected() const {
  return !is_keyword_hint_ && !keyword_.empty();
}

void AutocompleteEditModel::ClearPopupKeywordMode() const {
  if (popup_->IsOpen() &&
      popup_->selected_line_state() == AutocompletePopupModel::KEYWORD)
    popup_->SetSelectedLineState(AutocompletePopupModel::NORMAL);
}

string16 AutocompleteEditModel::DisplayTextFromUserText(
    const string16& text) const {
  return KeywordIsSelected() ?
      KeywordProvider::SplitReplacementStringFromInput(text, false) : text;
}

string16 AutocompleteEditModel::UserTextFromDisplayText(
    const string16& text) const {
  return KeywordIsSelected() ? (keyword_ + char16(' ') + text) : text;
}

void AutocompleteEditModel::InfoForCurrentSelection(
    AutocompleteMatch* match,
    GURL* alternate_nav_url) const {
  DCHECK(match != NULL);
  const AutocompleteResult& result = this->result();
  if (!autocomplete_controller_->done()) {
    // It's technically possible for |result| to be empty if no provider returns
    // a synchronous result but the query has not completed synchronously;
    // pratically, however, that should never actually happen.
    if (result.empty())
      return;
    // The user cannot have manually selected a match, or the query would have
    // stopped.  So the default match must be the desired selection.
    *match = *result.default_match();
  } else {
    CHECK(popup_->IsOpen());
    // If there are no results, the popup should be closed (so we should have
    // failed the CHECK above), and URLsForDefaultMatch() should have been
    // called instead.
    CHECK(!result.empty());
    CHECK(popup_->selected_line() < result.size());
    *match = result.match_at(popup_->selected_line());
  }
  if (alternate_nav_url && popup_->manually_selected_match().empty())
    *alternate_nav_url = result.alternate_nav_url();
}

void AutocompleteEditModel::GetInfoForCurrentText(
    AutocompleteMatch* match,
    GURL* alternate_nav_url) const {
  if (popup_->IsOpen() || query_in_progress()) {
    InfoForCurrentSelection(match, alternate_nav_url);
  } else {
    profile_->GetAutocompleteClassifier()->Classify(
        UserTextFromDisplayText(view_->GetText()), GetDesiredTLD(),
        KeywordIsSelected(), true, match, alternate_nav_url);
  }
}

void AutocompleteEditModel::RevertTemporaryText(bool revert_popup) {
  // The user typed something, then selected a different item.  Restore the
  // text they typed and change back to the default item.
  // NOTE: This purposefully does not reset paste_state_.
  just_deleted_text_ = false;
  has_temporary_text_ = false;
  if (revert_popup)
    popup_->ResetToDefaultMatch();
  view_->OnRevertTemporaryText();
}

bool AutocompleteEditModel::MaybeAcceptKeywordBySpace(
    const string16& new_text) {
  size_t keyword_length = new_text.length() - 1;
  return (paste_state_ == NONE) && is_keyword_hint_ && !keyword_.empty() &&
      inline_autocomplete_text_.empty() &&
      (keyword_.length() == keyword_length) &&
      IsSpaceCharForAcceptingKeyword(new_text[keyword_length]) &&
      !new_text.compare(0, keyword_length, keyword_, 0, keyword_length) &&
      AcceptKeyword();
}

bool AutocompleteEditModel::CreatedKeywordSearchByInsertingSpaceInMiddle(
    const string16& old_text,
    const string16& new_text,
    size_t caret_position) const {
  DCHECK_GE(new_text.length(), caret_position);

  // Check simple conditions first.
  if ((paste_state_ != NONE) || (caret_position < 2) ||
      (old_text.length() < caret_position) ||
      (new_text.length() == caret_position))
    return false;
  size_t space_position = caret_position - 1;
  if (!IsSpaceCharForAcceptingKeyword(new_text[space_position]) ||
      IsWhitespace(new_text[space_position - 1]) ||
      new_text.compare(0, space_position, old_text, 0, space_position) ||
      !new_text.compare(space_position, new_text.length() - space_position,
                        old_text, space_position,
                        old_text.length() - space_position)) {
    return false;
  }

  // Then check if the text before the inserted space matches a keyword.
  string16 keyword;
  TrimWhitespace(new_text.substr(0, space_position), TRIM_LEADING, &keyword);
  return !keyword.empty() &&
      !autocomplete_controller_->keyword_provider()->
          GetKeywordForText(keyword).empty();
}

bool AutocompleteEditModel::DoInstant(const AutocompleteMatch& match,
                                      string16* suggested_text) {
  DCHECK(suggested_text);

  if (in_revert_)
    return false;

  InstantController* instant = controller_->GetInstant();

  if (!instant)
    return false;

  // It's possible the tab strip does not have an active tab contents, for
  // instance if the tab has been closed or on return from a sleep state
  // (http://crbug.com/105689)
  TabContentsWrapper* tab = controller_->GetTabContentsWrapper();
  if (!tab)
    return false;

  if (user_input_in_progress_ && popup_->IsOpen()) {
    return instant->Update(tab, match, view_->GetText(), UseVerbatimInstant(),
                           suggested_text);
  }

  instant->Hide();
  return false;
}

void AutocompleteEditModel::DoPrerender(const AutocompleteMatch& match) {
  // Do not prerender if the destination URL is the same as the current URL.
  if (match.destination_url == PermanentURL())
    return;
  // It's possible the tab strip does not have an active tab contents, for
  // instance if the tab has been closed or on return from a sleep state
  // (http://crbug.com/105689)
  TabContentsWrapper* tab = controller_->GetTabContentsWrapper();
  if (!tab)
    return;
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(tab->profile());
  if (prerender_manager) {
    content::RenderViewHost* current_host =
        tab->web_contents()->GetRenderViewHost();
    prerender_manager->AddPrerenderFromOmnibox(
        match.destination_url, current_host->GetSessionStorageNamespace());
  }
}

void AutocompleteEditModel::DoPreconnect(const AutocompleteMatch& match) {
  if (!match.destination_url.SchemeIs(chrome::kExtensionScheme)) {
    // Warm up DNS Prefetch cache, or preconnect to a search service.
    UMA_HISTOGRAM_ENUMERATION("Autocomplete.MatchType", match.type,
                              AutocompleteMatch::NUM_TYPES);
    if (profile_->GetNetworkPredictor()) {
      profile_->GetNetworkPredictor()->AnticipateOmniboxUrl(
          match.destination_url,
          AutocompleteActionPredictor::IsPreconnectable(match));
    }
    // We could prefetch the alternate nav URL, if any, but because there
    // can be many of these as a user types an initial series of characters,
    // the OS DNS cache could suffer eviction problems for minimal gain.
  }
}

//  static
bool AutocompleteEditModel::IsSpaceCharForAcceptingKeyword(wchar_t c) {
  switch (c) {
    case 0x0020:  // Space
    case 0x3000:  // Ideographic Space
      return true;
    default:
      return false;
  }
}

metrics::OmniboxEventProto::PageClassification
    AutocompleteEditModel::ClassifyPage(const GURL& gurl) const {
  if (!gurl.is_valid())
    return metrics::OmniboxEventProto_PageClassification_INVALID_SPEC;
  const std::string& url = gurl.spec();
  if (url == chrome::kChromeUINewTabURL)
    return metrics::OmniboxEventProto_PageClassification_NEW_TAB_PAGE;
  if (url == chrome::kAboutBlankURL)
    return metrics::OmniboxEventProto_PageClassification_BLANK;
  if (url == profile()->GetPrefs()->GetString(prefs::kHomePage))
    return metrics::OmniboxEventProto_PageClassification_HOMEPAGE;
  return metrics::OmniboxEventProto_PageClassification_OTHER;
}
