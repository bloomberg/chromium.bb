// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_controller.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "net/base/escape.h"
#include "unicode/normalizer2.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

namespace {

// An artificial delay (in milliseconds) we introduce before telling the Instant
// page about the new omnibox bounds, in cases where the bounds shrink. This is
// to avoid the page jumping up/down very fast in response to bounds changes.
const int kUpdateBoundsDelayMS = 1000;

// The maximum number of times we'll load a non-Instant-supporting search engine
// before we give up and blacklist it for the rest of the browsing session.
const int kMaxInstantSupportFailures = 10;

// If an Instant page has not been used in these many milliseconds, it is
// reloaded so that the page does not become stale.
const int kStaleLoaderTimeoutMS = 3 * 3600 * 1000;

void AddSessionStorageHistogram(bool extended_enabled,
                                const TabContents* tab1,
                                const TabContents* tab2) {
  base::Histogram* histogram = base::BooleanHistogram::FactoryGet(
      std::string("Instant.SessionStorageNamespace") +
          (extended_enabled ? "_Extended" : "_Instant"),
      base::Histogram::kUmaTargetedHistogramFlag);
  const content::SessionStorageNamespaceMap& session_storage_map1 =
      tab1->web_contents()->GetController().GetSessionStorageNamespaceMap();
  const content::SessionStorageNamespaceMap& session_storage_map2 =
      tab2->web_contents()->GetController().GetSessionStorageNamespaceMap();
  bool is_session_storage_the_same =
      session_storage_map1.size() == session_storage_map2.size();
  if (is_session_storage_the_same) {
    // The size is the same, so let's check that all entries match.
    for (content::SessionStorageNamespaceMap::const_iterator
             it1 = session_storage_map1.begin(),
             it2 = session_storage_map2.begin();
         it1 != session_storage_map1.end() && it2 != session_storage_map2.end();
         ++it1, ++it2) {
      if (it1->first != it2->first || it1->second != it2->second) {
        is_session_storage_the_same = false;
        break;
      }
    }
  }
  histogram->AddBoolean(is_session_storage_the_same);
}

string16 Normalize(const string16& str) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* normalizer =
      icu::Normalizer2::getInstance(NULL, "nfkc_cf", UNORM2_COMPOSE, status);
  if (normalizer == NULL || U_FAILURE(status))
    return str;
  icu::UnicodeString norm_str(normalizer->normalize(
      icu::UnicodeString(FALSE, str.c_str(), str.size()), status));
  if (U_FAILURE(status))
    return str;
  return string16(norm_str.getBuffer(), norm_str.length());
}

bool NormalizeAndStripPrefix(string16* text, const string16& prefix) {
  const string16 norm_prefix = Normalize(prefix);
  string16 norm_text = Normalize(*text);
  if (norm_prefix.size() <= norm_text.size() &&
      norm_text.compare(0, norm_prefix.size(), norm_prefix) == 0) {
    *text = norm_text.erase(0, norm_prefix.size());
    return true;
  }
  return false;
}

// For TOOLKIT_VIEWS, the top level widget is always focused. If the focus
// change originated in views determine the child Widget from the view that is
// being focused.
gfx::NativeView GetViewGainingFocus(gfx::NativeView view_gaining_focus) {
#if defined(TOOLKIT_VIEWS)
  views::Widget* widget = view_gaining_focus ?
      views::Widget::GetWidgetForNativeView(view_gaining_focus) : NULL;
  if (widget) {
    views::FocusManager* focus_manager = widget->GetFocusManager();
    if (focus_manager && focus_manager->is_changing_focus() &&
        focus_manager->GetFocusedView() &&
        focus_manager->GetFocusedView()->GetWidget()) {
      return focus_manager->GetFocusedView()->GetWidget()->GetNativeView();
    }
  }
#endif
  return view_gaining_focus;
}

// Returns true if |view| is the top-level contents view or a child view in the
// view hierarchy of |contents|.
bool IsViewInContents(gfx::NativeView view, content::WebContents* contents) {
  content::RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView();
  if (!view || !rwhv)
    return false;

  gfx::NativeView tab_view = contents->GetNativeView();
  if (view == rwhv->GetNativeView() || view == tab_view)
    return true;

  // Walk up the view hierarchy to determine if the view is a subview of the
  // WebContents view (such as a windowed plugin or http auth dialog).
  while (view) {
    view = platform_util::GetParent(view);
    if (view == tab_view)
      return true;
  }

  return false;
}

}  // namespace

InstantController::InstantController(chrome::BrowserInstantController* browser,
                                     bool extended_enabled)
    : browser_(browser),
      extended_enabled_(extended_enabled),
      instant_enabled_(false),
      model_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      last_verbatim_(false),
      last_transition_type_(content::PAGE_TRANSITION_LINK),
      last_match_was_search_(false),
      is_omnibox_focused_(false) {
}

InstantController::~InstantController() {
}

bool InstantController::Update(const AutocompleteMatch& match,
                               const string16& user_text,
                               const string16& full_text,
                               const bool verbatim,
                               const bool user_input_in_progress,
                               const bool omnibox_popup_is_open) {
  if (!extended_enabled_ && !instant_enabled_)
    return false;

  DVLOG(1) << "Update: " << AutocompleteMatch::TypeToString(match.type)
           << " user_text='" << user_text << "' full_text='" << full_text << "'"
           << " verbatim=" << verbatim << " typing=" << user_input_in_progress
           << " popup=" << omnibox_popup_is_open;

  // If the popup is open, the user has to be typing.
  DCHECK(!omnibox_popup_is_open || user_input_in_progress);

  // If the popup is closed, there should be no inline autocompletion.
  DCHECK(omnibox_popup_is_open || user_text == full_text) << user_text << "|"
                                                          << full_text;

  // If there's inline autocompletion, the query has to be verbatim.
  DCHECK(user_text == full_text || verbatim) << user_text << "|" << full_text;

  // If there's no text in the omnibox, the user can't have typed any.
  DCHECK(!full_text.empty() || user_text.empty()) << user_text;

  // If the user isn't typing, and the popup is closed, there can't be any
  // user-typed text.
  DCHECK(user_input_in_progress || omnibox_popup_is_open || user_text.empty())
      << user_text;

  // In non-extended mode, SearchModeChanged() is never called, so fake it. The
  // mode is set to "disallow suggestions" here, so that if one of the early
  // "return false" conditions is hit, suggestions will be disallowed. If the
  // query is sent to the loader, the mode is set to "allow" further below.
  if (!extended_enabled_)
    search_mode_.mode = chrome::search::Mode::MODE_DEFAULT;

  // If there's no active tab, the browser is closing.
  const TabContents* const active_tab = browser_->GetActiveTabContents();
  if (!active_tab) {
    Hide(true);
    return false;
  }

  // Legend: OPIO == |omnibox_popup_is_open|, UIIP = |user_input_in_progress|.
  //
  //  #  OPIO UIIP full_text  Notes
  //  -  ---- ---- ---------  -----
  //  1   no   no    blank    } Navigation, or user hit Escape. |full_text| is
  //  2   no   no  non-blank  } blank if the page is NTP, non-blank otherwise.
  //
  //  3   no  yes    blank    User backspaced away all omnibox text.
  //
  //  4   no  yes  non-blank  User switched to a tab with a partial query.
  //
  //  5  yes   no    blank    } Impossible. DCHECK()ed above.
  //  6  yes   no  non-blank  }
  //
  //  7  yes  yes    blank    User typed a "?" into the omnibox.
  //
  //  8  yes  yes  non-blank  User typed text into the omnibox.
  //
  //  In non-extended mode, #1 to #7 call Hide(). #8 calls loader_->Update().
  //
  //  In extended mode, #2 and #4 call Hide(). #1 doesn't Hide() as the preview
  //  may be showing custom NTP content, but doesn't Update() either. #3 and #7
  //  don't Hide(), but send a blank query to Update(). #8 calls Update().

  if (extended_enabled_) {
    if (!omnibox_popup_is_open) {
      if (!full_text.empty()) {
        Hide(true);
        return false;
      }
      if (!user_input_in_progress && full_text.empty())
        return false;
    }
  } else if (!omnibox_popup_is_open || full_text.empty()) {
    // Update() can be called if the user clicks the preview while composing
    // text with an IME. If so, we should commit on mouse up, so don't Hide().
    if (!GetPreviewContents() || !loader_->IsPointerDownFromActivate())
      Hide(true);
    return false;
  }

  // Ensure we have a loader that can process this match. First, try to use the
  // TemplateURL of the |match|. If that's invalid, in non-extended mode, stop.
  // In extended mode, try using the default search engine, but only when the
  // match is for a URL (i.e., not some other kind of non-Instant search).
  // A completely blank query shows up as a search, and we do want to allow
  // that, hence the "!full_text.empty()" clause.
  Profile* const profile = active_tab->profile();
  const bool match_is_search = AutocompleteMatch::IsSearchType(match.type);
  if (!ResetLoader(match.GetTemplateURL(profile, false), active_tab) &&
      (!extended_enabled_ || (match_is_search && !full_text.empty()) ||
       !CreateDefaultLoader())) {
    Hide(true);
    return false;
  }

  // If the user continues typing the same query as the suggested text is
  // showing, reuse the suggestion (but only for INSTANT_COMPLETE_NEVER).
  bool reused_suggestion = false;
  if (last_suggestion_.behavior == INSTANT_COMPLETE_NEVER) {
    if (StartsWith(last_user_text_, user_text, false) && !user_text.empty()) {
      // The user is backspacing away characters.
      last_suggestion_.text.insert(0, last_user_text_, user_text.size(),
                                   last_user_text_.size() - user_text.size());
      reused_suggestion = true;
    } else if (StartsWith(user_text, last_user_text_, false)) {
      // The user is typing forward. Normalize any added characters.
      reused_suggestion = NormalizeAndStripPrefix(&last_suggestion_.text,
          string16(user_text, last_user_text_.size()));
    }
  }

  last_user_text_ = user_text;
  last_full_text_ = full_text;
  last_verbatim_ = verbatim;

  if (!reused_suggestion)
    last_suggestion_ = InstantSuggestion();

  last_transition_type_ = match.transition;
  last_match_was_search_ = match_is_search;
  url_for_history_ = match.destination_url;

  // Store the first interaction time for use with latency histograms.
  if (first_interaction_time_.is_null())
    first_interaction_time_ = base::Time::Now();

  // Allow search suggestions. In extended mode, SearchModeChanged() will set
  // this, but it's not called in non-extended mode, so fake it.
  if (!extended_enabled_)
    search_mode_.mode = chrome::search::Mode::MODE_SEARCH_SUGGESTIONS;

  loader_->Update(extended_enabled_ ? user_text : full_text, verbatim);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());

  // We don't have new suggestions yet, but we can either reuse the existing
  // suggestion or reset the existing "gray text".
  browser_->SetInstantSuggestion(last_suggestion_);

  // Though we may have handled a URL match above, we return false here, so that
  // omnibox prerendering can kick in. TODO(sreeram): Remove this (and always
  // return true) once we are able to commit URLs as well.
  return match_is_search;
}

// TODO(tonyg): This method only fires when the omnibox bounds change. It also
// needs to fire when the preview bounds change (e.g.: open/close info bar).
void InstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (!extended_enabled_ && !instant_enabled_)
    return;

  if (omnibox_bounds_ == bounds)
    return;

  omnibox_bounds_ = bounds;
  if (omnibox_bounds_.height() > last_omnibox_bounds_.height()) {
    update_bounds_timer_.Stop();
    SendBoundsToPage();
  } else if (!update_bounds_timer_.IsRunning()) {
    update_bounds_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kUpdateBoundsDelayMS), this,
        &InstantController::SendBoundsToPage);
  }
}

void InstantController::HandleAutocompleteResults(
    const std::vector<AutocompleteProvider*>& providers) {
  if (!extended_enabled_)
    return;

  if (!GetPreviewContents())
    return;

  DVLOG(1) << "AutocompleteResults:";
  std::vector<InstantAutocompleteResult> results;
  for (ACProviders::const_iterator provider = providers.begin();
       provider != providers.end(); ++provider) {
    for (ACMatches::const_iterator match = (*provider)->matches().begin();
         match != (*provider)->matches().end(); ++match) {
      InstantAutocompleteResult result;
      result.provider = UTF8ToUTF16((*provider)->GetName());
      result.type = UTF8ToUTF16(AutocompleteMatch::TypeToString(match->type));
      result.description = match->description;
      result.destination_url = UTF8ToUTF16(match->destination_url.spec());
      result.relevance = match->relevance;
      DVLOG(1) << "    " << result.relevance << " " << result.type << " "
               << result.provider << " " << result.destination_url << " '"
               << result.description << "'";
      results.push_back(result);
    }
  }

  loader_->SendAutocompleteResults(results);
}

bool InstantController::OnUpOrDownKeyPressed(int count) {
  if (!extended_enabled_)
    return false;

  if (!GetPreviewContents())
    return false;

  loader_->OnUpOrDownKeyPressed(count);
  return true;
}

TabContents* InstantController::GetPreviewContents() const {
  return loader_ ? loader_->preview_contents() : NULL;
}

bool InstantController::IsCurrent() const {
  return model_.mode().is_search_suggestions() && last_match_was_search_;
}

bool InstantController::CommitIfCurrent(InstantCommitType type) {
  if (!extended_enabled_ && !instant_enabled_)
    return false;

  if (!IsCurrent())
    return false;

  DVLOG(1) << "CommitIfCurrent";
  TabContents* preview = loader_->ReleasePreviewContents(type, last_full_text_);

  if (extended_enabled_) {
    // Consider what's happening:
    //   1. The user has typed a query in the omnibox and committed it (either
    //      by pressing Enter or clicking on the preview).
    //   2. We commit the preview to the tab strip, and tell the page.
    //   3. The page will update the URL hash fragment with the query terms.
    // After steps 1 and 3, the omnibox will show the query terms. However, if
    // the URL we are committing at step 2 doesn't already have query terms, it
    // will flash for a brief moment as a plain URL. So, avoid that flicker by
    // pretending that the plain URL is actually the typed query terms.
    // TODO(samarth,beaudoin): Instead of this hack, we should add a new field
    // to NavigationEntry to keep track of what the correct query, if any, is.
    content::NavigationEntry* entry =
        preview->web_contents()->GetController().GetVisibleEntry();
    std::string url = entry->GetVirtualURL().spec();
    if (!google_util::IsInstantExtendedAPIGoogleSearchUrl(url) &&
        google_util::IsGoogleDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                       google_util::ALLOW_NON_STANDARD_PORTS)) {
      entry->SetVirtualURL(GURL(
          url + "#q=" +
          net::EscapeQueryParamValue(UTF16ToUTF8(last_full_text_), true)));
      chrome::search::SearchTabHelper::FromWebContents(
          preview->web_contents())->NavigationEntryUpdated();
    }
  }

  // If the preview page has navigated since the last Update(), we need to add
  // the navigation to history ourselves. Else, the page will navigate after
  // commit, and it will be added to history in the usual manner.
  const history::HistoryAddPageArgs& last_navigation =
      loader_->last_navigation();
  if (!last_navigation.url.is_empty()) {
    content::NavigationEntry* entry =
        preview->web_contents()->GetController().GetActiveEntry();
    DCHECK_EQ(last_navigation.url, entry->GetURL());

    // Add the page to history.
    HistoryTabHelper* history_tab_helper =
        HistoryTabHelper::FromWebContents(preview->web_contents());
    history_tab_helper->UpdateHistoryForNavigation(last_navigation);

    // Update the page title.
    history_tab_helper->UpdateHistoryPageTitle(*entry);
  }

  // Add a fake history entry with a non-Instant search URL, so that search
  // terms extraction (for autocomplete history matches) works.
  if (HistoryService* history = HistoryServiceFactory::GetForProfile(
          preview->profile(), Profile::EXPLICIT_ACCESS)) {
    history->AddPage(url_for_history_, base::Time::Now(), NULL, 0, GURL(),
                     history::RedirectList(), last_transition_type_,
                     history::SOURCE_BROWSED, false);
  }

  preview->web_contents()->GetController().PruneAllButActive();

  if (type != INSTANT_COMMIT_PRESSED_ALT_ENTER) {
    const TabContents* active_tab = browser_->GetActiveTabContents();
    AddSessionStorageHistogram(extended_enabled_, active_tab, preview);
    preview->web_contents()->GetController().CopyStateFromAndPrune(
        &active_tab->web_contents()->GetController());
  }

  DeleteLoader();

  // Browser takes ownership of the preview.
  browser_->CommitInstant(preview, type == INSTANT_COMMIT_PRESSED_ALT_ENTER);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_COMMITTED,
      content::Source<content::WebContents>(preview->web_contents()),
      content::NotificationService::NoDetails());

  model_.SetPreviewState(chrome::search::Mode(), 0, INSTANT_SIZE_PERCENT);

  // Try to create another loader immediately so that it is ready for the next
  // user interaction.
  CreateDefaultLoader();

  return true;
}

void InstantController::OmniboxLostFocus(gfx::NativeView view_gaining_focus) {
  DVLOG(1) << "OmniboxLostFocus";
  is_omnibox_focused_ = false;

  if (!extended_enabled_ && !instant_enabled_)
    return;

  // If the preview isn't showing search suggestions, nothing to do. The check
  // for GetPreviewContents() (which normally is redundant, given IsCurrent())
  // is to handle the case when we get here during a commit.
  if (!IsCurrent() || !GetPreviewContents()) {
    OnStaleLoader();
    return;
  }

#if defined(OS_MACOSX)
  if (!loader_->IsPointerDownFromActivate())
    Hide(true);
#else
  if (IsViewInContents(GetViewGainingFocus(view_gaining_focus),
                       GetPreviewContents()->web_contents()))
    CommitIfCurrent(INSTANT_COMMIT_FOCUS_LOST);
  else
    Hide(true);
#endif
}

void InstantController::OmniboxGotFocus() {
  DVLOG(1) << "OmniboxGotFocus";
  is_omnibox_focused_ = true;

  if (!extended_enabled_ && !instant_enabled_)
    return;

  if (!GetPreviewContents())
    CreateDefaultLoader();
}

void InstantController::SearchModeChanged(
    const chrome::search::Mode& old_mode,
    const chrome::search::Mode& new_mode) {
  if (!extended_enabled_)
    return;

  DVLOG(1) << "SearchModeChanged: [origin:mode] " << old_mode.origin << ":"
           << old_mode.mode << " to " << new_mode.origin << ":"
           << new_mode.mode;
  search_mode_ = new_mode;

  if (new_mode.is_search_suggestions()) {
    // The preview is showing NTP content, but it's not appropriate anymore.
    if (model_.mode().is_ntp() && !new_mode.is_origin_ntp())
      Hide(false);
  } else {
    Hide(true);
  }

  if (GetPreviewContents())
    loader_->SearchModeChanged(new_mode);
}

void InstantController::ActiveTabChanged() {
  if (!extended_enabled_ && !instant_enabled_)
    return;

  DVLOG(1) << "ActiveTabChanged";

  // By this time, SearchModeChanged() should've been called, so we only need to
  // handle the case when the search mode does NOT change, as in the case of
  // going from search_suggestions to search_suggestions (i.e., partial queries
  // on both old and new tabs).
  if (search_mode_.is_search_suggestions() &&
      model_.mode().is_search_suggestions())
    Hide(false);
}

void InstantController::SetInstantEnabled(bool instant_enabled) {
  instant_enabled_ = instant_enabled;
  if (!extended_enabled_ && !instant_enabled_)
    DeleteLoader();
}

void InstantController::ThemeChanged(const ThemeBackgroundInfo& theme_info) {
  if (!extended_enabled_)
    return;

  if (GetPreviewContents())
    loader_->SendThemeBackgroundInfo(theme_info);
}

void InstantController::ThemeAreaHeightChanged(int height) {
  if (!extended_enabled_)
    return;

  if (GetPreviewContents())
    loader_->SendThemeAreaHeight(height);
}

void InstantController::SetSuggestions(
    InstantLoader* loader,
    const std::vector<InstantSuggestion>& suggestions) {
  DVLOG(1) << "SetSuggestions";
  if (loader_ != loader || !search_mode_.is_search_suggestions())
    return;

  InstantSuggestion suggestion;
  if (!suggestions.empty())
    suggestion = suggestions[0];

  if (suggestion.behavior == INSTANT_COMPLETE_REPLACE) {
    // We don't get an Update() when changing the omnibox due to a REPLACE
    // suggestion (so that we don't inadvertently cause the preview to change
    // what it's showing, as the user arrows up/down through the page-provided
    // suggestions). So, update these state variables here.
    last_full_text_ = suggestion.text;
    last_user_text_.clear();
    last_verbatim_ = true;
    last_suggestion_ = InstantSuggestion();
    last_match_was_search_ = suggestion.type == INSTANT_SUGGESTION_SEARCH;
    DVLOG(1) << "SetReplaceSuggestion: text='" << suggestion.text << "'"
             << " type=" << suggestion.type;
    browser_->SetInstantSuggestion(suggestion);
  } else {
    // Suggestion text should be a full URL for URL suggestions, or the
    // completion of a query for query suggestions.
    if (suggestion.type == INSTANT_SUGGESTION_URL) {
      // If the suggestion is not a valid URL, perhaps it's something like
      // "foo.com". Try prefixing "http://". If it still isn't valid, drop it.
      if (!GURL(suggestion.text).is_valid()) {
        suggestion.text.insert(0, ASCIIToUTF16("http://"));
        if (!GURL(suggestion.text).is_valid())
          suggestion = InstantSuggestion();
      }
    } else if (StartsWith(suggestion.text, last_user_text_, true)) {
      // The user typed an exact prefix of the suggestion.
      suggestion.text.erase(0, last_user_text_.size());
    } else if (!NormalizeAndStripPrefix(&suggestion.text, last_user_text_)) {
      // Unicode normalize and case-fold the user text and suggestion. If the
      // user text is a prefix, suggest the normalized, case-folded completion;
      // for instance, if the user types 'i' and the suggestion is 'INSTANT',
      // suggest 'nstant'. Otherwise, the user text really isn't a prefix, so
      // suggest nothing.
      suggestion = InstantSuggestion();
    }

    // If the omnibox is blank, this suggestion is for an older query. Ignore.
    if (last_user_text_.empty())
      suggestion = InstantSuggestion();

    // Don't suggest gray text if there already was inline autocompletion.
    // http://crbug.com/162303
    if (suggestion.behavior == INSTANT_COMPLETE_NEVER &&
        last_user_text_ != last_full_text_)
      suggestion = InstantSuggestion();

    // Don't allow inline autocompletion if the query was verbatim.
    if (suggestion.behavior == INSTANT_COMPLETE_NOW && last_verbatim_)
      suggestion = InstantSuggestion();

    last_suggestion_ = suggestion;

    if (!suggestion.text.empty()) {
      DVLOG(1) << "SetInstantSuggestion: text='" << suggestion.text << "'"
               << " behavior=" << suggestion.behavior << " type="
               << suggestion.type;
      browser_->SetInstantSuggestion(suggestion);
    }
  }

  Show(INSTANT_SHOWN_QUERY_SUGGESTIONS, 100, INSTANT_SIZE_PERCENT);
}

void InstantController::CommitInstantLoader(InstantLoader* loader) {
  if (loader_ == loader)
    CommitIfCurrent(INSTANT_COMMIT_FOCUS_LOST);
}

void InstantController::ShowInstantPreview(InstantLoader* loader,
                                           InstantShownReason reason,
                                           int height,
                                           InstantSizeUnits units) {
  if (loader_ == loader && extended_enabled_)
    Show(reason, height, units);
}

void InstantController::InstantSupportDetermined(InstantLoader* loader,
                                                 bool supports_instant) {
  if (supports_instant) {
    blacklisted_urls_.erase(loader->instant_url());
  } else {
    ++blacklisted_urls_[loader->instant_url()];
    if (loader_ == loader)
      DeleteLoader();
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());
}

void InstantController::SwappedTabContents(InstantLoader* loader) {
  if (loader_ == loader)
    model_.SetPreviewContents(GetPreviewContents());
}

void InstantController::InstantLoaderContentsFocused(InstantLoader* loader) {
#if defined(USE_AURA)
  // On aura the omnibox only receives a focus lost if we initiate the focus
  // change. This does that.
  if (loader_ == loader && !model_.mode().is_default())
    browser_->InstantPreviewFocused();
#endif
}

bool InstantController::ResetLoader(const TemplateURL* template_url,
                                    const TabContents* active_tab) {
  std::string instant_url;
  if (!GetInstantURL(template_url, &instant_url))
    return false;

  if (GetPreviewContents() && loader_->instant_url() != instant_url)
    DeleteLoader();

  if (!GetPreviewContents()) {
    loader_.reset(new InstantLoader(this, instant_url, active_tab));
    loader_->Init();

    // Ensure the searchbox API has the correct theme-related info and context.
    if (extended_enabled_) {
      browser_->UpdateThemeInfoForPreview();
      loader_->SearchModeChanged(search_mode_);
    }

    // Reset the loader timer.
    stale_loader_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kStaleLoaderTimeoutMS), this,
        &InstantController::OnStaleLoader);
  }

  return true;
}

bool InstantController::CreateDefaultLoader() {
  // If there's no active tab, the browser is closing.
  const TabContents* active_tab = browser_->GetActiveTabContents();
  if (!active_tab)
    return false;

  const TemplateURL* template_url =
      TemplateURLServiceFactory::GetForProfile(active_tab->profile())->
                                 GetDefaultSearchProvider();

  return ResetLoader(template_url, active_tab);
}

void InstantController::OnStaleLoader() {
  // If the preview is showing or the omnibox has focus, don't delete the
  // loader. It will get refreshed the next time the preview is hidden or the
  // omnibox loses focus.
  if (!stale_loader_timer_.IsRunning() && !is_omnibox_focused_ &&
      model_.mode().is_default()) {
    DeleteLoader();
    CreateDefaultLoader();
  }
}

void InstantController::DeleteLoader() {
  // Clear all state, except |last_transition_type_| as it's used during commit.
  last_user_text_.clear();
  last_full_text_.clear();
  last_verbatim_ = false;
  last_suggestion_ = InstantSuggestion();
  last_match_was_search_ = false;
  if (!extended_enabled_)
    search_mode_.mode = chrome::search::Mode::MODE_DEFAULT;
  omnibox_bounds_ = gfx::Rect();
  last_omnibox_bounds_ = gfx::Rect();
  update_bounds_timer_.Stop();
  stale_loader_timer_.Stop();
  url_for_history_ = GURL();
  first_interaction_time_ = base::Time();
  if (GetPreviewContents()) {
    model_.SetPreviewState(chrome::search::Mode(), 0, INSTANT_SIZE_PERCENT);
    loader_->CleanupPreviewContents();
  }

  // Schedule the deletion for later, since we may have gotten here from a call
  // within a |loader_| method (i.e., it's still on the stack). If we deleted
  // the loader immediately, things would still be fine so long as the caller
  // doesn't access any instance members after we return, but why rely on that?
  MessageLoop::current()->DeleteSoon(FROM_HERE, loader_.release());
}

void InstantController::Hide(bool clear_query) {
  DVLOG(1) << "Hide: clear_query=" << clear_query;

  // The only time when the preview is not already in the desired MODE_DEFAULT
  // state and GetPreviewContents() returns NULL is when we are in the commit
  // path. In that case, don't change the state just yet; otherwise we may
  // cause the preview to hide unnecessarily. Instead, the state will be set
  // correctly after the commit is done.
  if (GetPreviewContents())
    model_.SetPreviewState(chrome::search::Mode(), 0, INSTANT_SIZE_PERCENT);

  // Clear the first interaction timestamp for later use.
  first_interaction_time_ = base::Time();

  if (clear_query) {
    if (GetPreviewContents() && !last_full_text_.empty())
      loader_->Update(string16(), true);
    last_user_text_.clear();
    last_full_text_.clear();
  }

  OnStaleLoader();
}

void InstantController::Show(InstantShownReason reason,
                             int height,
                             InstantSizeUnits units) {
  DVLOG(1) << "Show: reason=" << reason << " height=" << height << " units="
           << units;

  // Must be on NTP to show NTP content.
  if (reason == INSTANT_SHOWN_CUSTOM_NTP_CONTENT && !search_mode_.is_ntp())
    return;

  // Must have updated omnibox after most recent Hide() to show suggestions.
  if (reason == INSTANT_SHOWN_QUERY_SUGGESTIONS &&
      !search_mode_.is_search_suggestions())
    return;

  // If the preview is being shown because of the first set of suggestions to
  // arrive for this query editing session, record a histogram value.
  if (!first_interaction_time_.is_null() && model_.mode().is_default()) {
    base::TimeDelta delta = base::Time::Now() - first_interaction_time_;
    UMA_HISTOGRAM_TIMES("Instant.TimeToFirstShow", delta);
  }

  // Show at 100% height except in the following cases:
  // - The page wants to hide (height=0).
  // - The page wants to show custom NTP content.
  // - The page is over a website other than search or an NTP, and is not
  //   already showing at 100% height.
  const bool is_full_height =
      model_.height() == 100 && model_.height_units() == INSTANT_SIZE_PERCENT;
  if (height == 0 ||
      reason == INSTANT_SHOWN_CUSTOM_NTP_CONTENT ||
      (search_mode_.is_origin_default() && !is_full_height))
    model_.SetPreviewState(search_mode_, height, units);
  else
    model_.SetPreviewState(search_mode_, 100, INSTANT_SIZE_PERCENT);
}

void InstantController::SendBoundsToPage() {
  if (last_omnibox_bounds_ == omnibox_bounds_ ||
      !GetPreviewContents() || loader_->IsPointerDownFromActivate())
    return;

  last_omnibox_bounds_ = omnibox_bounds_;
  gfx::Rect preview_bounds = browser_->GetInstantBounds();
  gfx::Rect intersection = gfx::IntersectRects(omnibox_bounds_, preview_bounds);

  // Translate into window coordinates.
  if (!intersection.IsEmpty()) {
    intersection.Offset(-preview_bounds.origin().x(),
                        -preview_bounds.origin().y());
  }

  // In the current Chrome UI, these must always be true so they sanity check
  // the above operations. In a future UI, these may be removed or adjusted.
  // There is no point in sanity-checking |intersection.y()| because the omnibox
  // can be placed anywhere vertically relative to the preview (for example, in
  // Mac fullscreen mode, the omnibox is fully enclosed by the preview bounds).
  DCHECK_LE(0, intersection.x());
  DCHECK_LE(0, intersection.width());
  DCHECK_LE(0, intersection.height());

  loader_->SetOmniboxBounds(intersection);
}

bool InstantController::GetInstantURL(const TemplateURL* template_url,
                                      std::string* instant_url) const {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInstantURL)) {
    *instant_url = command_line->GetSwitchValueASCII(switches::kInstantURL);
    return template_url != NULL;
  }

  if (!template_url)
    return false;

  const TemplateURLRef& instant_url_ref = template_url->instant_url_ref();
  if (!instant_url_ref.IsValid())
    return false;

  // Even if the URL template doesn't have search terms, it may have other
  // components (such as {google:baseURL}) that need to be replaced.
  *instant_url = instant_url_ref.ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(string16()));

  // Extended mode should always use HTTPS. TODO(sreeram): This section can be
  // removed if TemplateURLs supported "https://{google:host}/..." instead of
  // only supporting "{google:baseURL}...".
  if (extended_enabled_) {
    GURL url_obj(*instant_url);
    if (!url_obj.is_valid())
      return false;

    if (!url_obj.SchemeIsSecure()) {
      const std::string new_scheme = "https";
      const std::string new_port = "443";
      GURL::Replacements secure;
      secure.SetSchemeStr(new_scheme);
      secure.SetPortStr(new_port);
      url_obj = url_obj.ReplaceComponents(secure);

      if (!url_obj.is_valid())
        return false;

      *instant_url = url_obj.spec();
    }
  }

  std::map<std::string, int>::const_iterator iter =
      blacklisted_urls_.find(*instant_url);
  if (iter != blacklisted_urls_.end() &&
      iter->second > kMaxInstantSupportFailures)
    return false;

  return true;
}
