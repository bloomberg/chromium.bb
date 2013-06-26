// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_controller.h"

#include <iterator>

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/instant_overlay.h"
#include "chrome/browser/ui/search/instant_tab.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/serialized_navigation_entry.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"
#include "third_party/icu/public/common/unicode/normalizer2.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

namespace {

// An artificial delay (in milliseconds) we introduce before telling the Instant
// page about the new omnibox bounds, in cases where the bounds shrink. This is
// to avoid the page jumping up/down very fast in response to bounds changes.
const int kUpdateBoundsDelayMS = 1000;

// For reporting Instant navigations.
enum InstantNavigation {
  INSTANT_NAVIGATION_LOCAL_CLICK = 0,
  INSTANT_NAVIGATION_LOCAL_SUBMIT = 1,
  INSTANT_NAVIGATION_ONLINE_CLICK = 2,
  INSTANT_NAVIGATION_ONLINE_SUBMIT = 3,
  INSTANT_NAVIGATION_NONEXTENDED = 4,
  INSTANT_NAVIGATION_MAX = 5
};

void RecordNavigationHistogram(bool is_local, bool is_click, bool is_extended) {
  InstantNavigation navigation;
  if (!is_extended) {
    navigation = INSTANT_NAVIGATION_NONEXTENDED;
  } else if (is_local) {
    navigation = is_click ? INSTANT_NAVIGATION_LOCAL_CLICK :
                            INSTANT_NAVIGATION_LOCAL_SUBMIT;
  } else {
    navigation = is_click ? INSTANT_NAVIGATION_ONLINE_CLICK :
                            INSTANT_NAVIGATION_ONLINE_SUBMIT;
  }
  UMA_HISTOGRAM_ENUMERATION("InstantExtended.InstantNavigation",
                            navigation,
                            INSTANT_NAVIGATION_MAX);
}

void RecordFallbackReasonHistogram(
    const InstantController::InstantFallbackReason fallback_reason) {
  UMA_HISTOGRAM_ENUMERATION("InstantExtended.FallbackToLocalOverlay",
                            fallback_reason,
                            InstantController::INSTANT_FALLBACK_MAX);
}

InstantController::InstantFallbackReason DetermineFallbackReason(
    const InstantPage* page, std::string instant_url) {
  InstantController::InstantFallbackReason fallback_reason;
  if (!page) {
    fallback_reason = InstantController::INSTANT_FALLBACK_NO_OVERLAY;
  } else if (instant_url.empty()) {
    fallback_reason = InstantController::INSTANT_FALLBACK_INSTANT_URL_EMPTY;
  } else if (!chrome::MatchesOriginAndPath(GURL(page->instant_url()),
                                           GURL(instant_url))) {
    fallback_reason = InstantController::INSTANT_FALLBACK_ORIGIN_PATH_MISMATCH;
  } else if (!page->supports_instant()) {
    fallback_reason = InstantController::INSTANT_FALLBACK_INSTANT_NOT_SUPPORTED;
  } else {
    fallback_reason = InstantController::INSTANT_FALLBACK_UNKNOWN;
  }
  return fallback_reason;
}

void AddSessionStorageHistogram(bool extended_enabled,
                                const content::WebContents* tab1,
                                const content::WebContents* tab2) {
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      std::string("Instant.SessionStorageNamespace") +
          (extended_enabled ? "_Extended" : "_Instant"),
      base::HistogramBase::kUmaTargetedHistogramFlag);
  const content::SessionStorageNamespaceMap& session_storage_map1 =
      tab1->GetController().GetSessionStorageNamespaceMap();
  const content::SessionStorageNamespaceMap& session_storage_map2 =
      tab2->GetController().GetSessionStorageNamespaceMap();
  bool is_session_storage_the_same =
      session_storage_map1.size() == session_storage_map2.size();
  if (is_session_storage_the_same) {
    // The size is the same, so let's check that all entries match.
    for (content::SessionStorageNamespaceMap::const_iterator
             it1 = session_storage_map1.begin(),
             it2 = session_storage_map2.begin();
         it1 != session_storage_map1.end() && it2 != session_storage_map2.end();
         ++it1, ++it2) {
      if (it1->first != it2->first || it1->second.get() != it2->second.get()) {
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
  string16 norm_prefix = Normalize(prefix);
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
        focus_manager->GetFocusedView()->GetWidget())
      return focus_manager->GetFocusedView()->GetWidget()->GetNativeView();
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

  gfx::NativeView tab_view = contents->GetView()->GetNativeView();
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

bool IsFullHeight(const InstantOverlayModel& model) {
  return model.height() == 100 && model.height_units() == INSTANT_SIZE_PERCENT;
}

bool IsContentsFrom(const InstantPage* page,
                    const content::WebContents* contents) {
  return page && (page->contents() == contents);
}

// Adds a transient NavigationEntry to the supplied |contents|'s
// NavigationController if the page's URL has not already been updated with the
// supplied |search_terms|. Sets the |search_terms| on the transient entry for
// search terms extraction to work correctly.
void EnsureSearchTermsAreSet(content::WebContents* contents,
                             const string16& search_terms) {
  content::NavigationController* controller = &contents->GetController();

  // If search terms are already correct or there is already a transient entry
  // (there shouldn't be), bail out early.
  if (chrome::GetSearchTerms(contents) == search_terms ||
      controller->GetTransientEntry())
    return;

  const content::NavigationEntry* active_entry = controller->GetActiveEntry();
  content::NavigationEntry* transient = controller->CreateNavigationEntry(
      active_entry->GetURL(),
      active_entry->GetReferrer(),
      active_entry->GetTransitionType(),
      false,
      std::string(),
      contents->GetBrowserContext());
  transient->SetExtraData(sessions::kSearchTermsKey, search_terms);
  controller->SetTransientEntry(transient);

  SearchTabHelper::FromWebContents(contents)->NavigationEntryUpdated();
}

template <typename T>
void DeletePageSoon(scoped_ptr<T> page) {
  if (page->contents()) {
    base::MessageLoop::current()->DeleteSoon(
        FROM_HERE, page->ReleaseContents().release());
  }

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, page.release());
}

}  // namespace

InstantController::InstantController(BrowserInstantController* browser,
                                     bool extended_enabled)
    : browser_(browser),
      extended_enabled_(extended_enabled),
      instant_enabled_(false),
      use_local_page_only_(true),
      preload_ntp_(true),
      model_(this),
      use_tab_for_suggestions_(false),
      last_omnibox_text_has_inline_autocompletion_(false),
      last_verbatim_(false),
      last_transition_type_(content::PAGE_TRANSITION_LINK),
      last_match_was_search_(false),
      omnibox_focus_state_(OMNIBOX_FOCUS_NONE),
      omnibox_focus_change_reason_(OMNIBOX_FOCUS_CHANGE_EXPLICIT),
      omnibox_bounds_(-1, -1, 0, 0),
      allow_overlay_to_show_search_suggestions_(false) {

  // When the InstantController lives, the InstantService should live.
  // InstantService sets up profile-level facilities such as the ThemeSource for
  // the NTP.
  // However, in some tests, browser_ may be null.
  if (browser_) {
    InstantService* instant_service = GetInstantService();
    instant_service->AddObserver(this);
  }
}

InstantController::~InstantController() {
  if (browser_) {
    InstantService* instant_service = GetInstantService();
    instant_service->RemoveObserver(this);
  }
}

void InstantController::OnAutocompleteStart() {
  if (UseTabForSuggestions() && instant_tab_->supports_instant()) {
    LOG_INSTANT_DEBUG_EVENT(
        this, "OnAutocompleteStart: using InstantTab");
    return;
  }

  // Not using |instant_tab_|. Check if overlay is OK to use.
  InstantFallbackReason fallback_reason = ShouldSwitchToLocalOverlay();
  if (fallback_reason != INSTANT_FALLBACK_NONE) {
    ResetOverlay(GetLocalInstantURL());
    RecordFallbackReasonHistogram(fallback_reason);
    LOG_INSTANT_DEBUG_EVENT(
        this, "OnAutocompleteStart: switching to local overlay");
  } else {
    LOG_INSTANT_DEBUG_EVENT(
        this, "OnAutocompleteStart: using existing overlay");
  }
  use_tab_for_suggestions_ = false;
}

bool InstantController::Update(const AutocompleteMatch& match,
                               const string16& user_text,
                               const string16& full_text,
                               size_t selection_start,
                               size_t selection_end,
                               bool verbatim,
                               bool user_input_in_progress,
                               bool omnibox_popup_is_open,
                               bool escape_pressed,
                               bool is_keyword_search) {
  if (!extended_enabled() && !instant_enabled_)
    return false;

  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "Update: %s user_text='%s' full_text='%s' selection_start=%d "
      "selection_end=%d verbatim=%d typing=%d popup=%d escape_pressed=%d "
      "is_keyword_search=%d",
      AutocompleteMatchType::ToString(match.type).c_str(),
      UTF16ToUTF8(user_text).c_str(), UTF16ToUTF8(full_text).c_str(),
      static_cast<int>(selection_start), static_cast<int>(selection_end),
      verbatim, user_input_in_progress, omnibox_popup_is_open, escape_pressed,
      is_keyword_search));

  // Store the current |last_omnibox_text_| and update |last_omnibox_text_|
  // upfront with the contents of |full_text|. Even if we do an early return,
  // |last_omnibox_text_| will be updated.
  string16 previous_omnibox_text = last_omnibox_text_;
  last_omnibox_text_ = full_text;
  last_match_was_search_ = AutocompleteMatch::IsSearchType(match.type) &&
                           !user_text.empty();

  // TODO(dhollowa): Complete keyword match UI.  For now just hide suggestions.
  // http://crbug.com/153932.  Note, this early escape is happens prior to the
  // DCHECKs below because |user_text| and |full_text| have different semantics
  // when keyword search is in effect.
  if (is_keyword_search) {
    if (UseTabForSuggestions())
      instant_tab_->sender()->Update(string16(), 0, 0, true);
    else
      HideOverlay();
    last_match_was_search_ = false;
    last_suggestion_ = InstantSuggestion();
    return false;
  }

  // Ignore spurious updates when the omnibox is blurred; otherwise click
  // targets on the page may vanish before a click event arrives.
  if (omnibox_focus_state_ == OMNIBOX_FOCUS_NONE)
    return false;

  // If the popup is open, the user has to be typing.
  DCHECK(!omnibox_popup_is_open || user_input_in_progress);

  // If the popup is closed, there should be no inline autocompletion.
  DCHECK(omnibox_popup_is_open || user_text.empty() || user_text == full_text)
      << user_text << "|" << full_text;

  // If there's no text in the omnibox, the user can't have typed any.
  DCHECK(!full_text.empty() || user_text.empty()) << user_text;

  // If the user isn't typing, and the popup is closed, there can't be any
  // user-typed text.
  DCHECK(user_input_in_progress || omnibox_popup_is_open || user_text.empty())
      << user_text;

  // The overlay is being clicked and will commit soon. Don't change anything.
  // TODO(sreeram): Add a browser test for this.
  if (overlay_ && overlay_->is_pointer_down_from_activate())
    return false;

  // In non-extended mode, SearchModeChanged() is never called, so fake it. The
  // mode is set to "disallow suggestions" here, so that if one of the early
  // "return false" conditions is hit, suggestions will be disallowed. If the
  // query is sent to the overlay, the mode is set to "allow" further below.
  if (!extended_enabled())
    search_mode_.mode = SearchMode::MODE_DEFAULT;

  // In non extended mode, Instant is disabled for URLs and keyword mode.
  if (!extended_enabled() &&
      (!last_match_was_search_ ||
       match.type == AutocompleteMatchType::SEARCH_OTHER_ENGINE)) {
    HideOverlay();
    return false;
  }

  if (!UseTabForSuggestions() && !overlay_) {
    HideOverlay();
    return false;
  }

  if (extended_enabled()) {
    if (!omnibox_popup_is_open) {
      if (!user_input_in_progress) {
        // If the user isn't typing and the omnibox popup is closed, it means a
        // regular navigation, tab-switch or the user hitting Escape.
        if (UseTabForSuggestions()) {
          // The user is on a search results page. It may be showing results for
          // a partial query the user typed before they hit Escape. Send the
          // omnibox text to the page to restore the original results.
          //
          // In a tab switch, |instant_tab_| won't have updated yet, so it may
          // be pointing to the previous tab (which was a search results page).
          // Ensure we don't send the omnibox text to a random webpage (the new
          // tab), by comparing the old and new WebContents.
          if (escape_pressed &&
              instant_tab_->contents() == browser_->GetActiveWebContents()) {
            // TODO(kmadhusu): If the |full_text| is not empty, send an
            // onkeypress(esc) to the Instant page. Do not call
            // onsubmit(full_text). Fix.
            if (full_text.empty()) {
              // Call onchange("") to clear the query for the page.
              instant_tab_->sender()->Update(string16(), 0, 0, true);
              instant_tab_->sender()->EscKeyPressed();
             } else {
              instant_tab_->sender()->Submit(full_text);
            }
          }
        } else if (!full_text.empty()) {
          // If |full_text| is empty, the user is on the NTP. The overlay may
          // be showing custom NTP content; hide only if that's not the case.
          HideOverlay();
        }
      } else if (full_text.empty()) {
        // The user is typing, and backspaced away all omnibox text. Clear
        // |last_omnibox_text_| so that we don't attempt to set suggestions.
        last_omnibox_text_.clear();
        last_user_text_.clear();
        last_suggestion_ = InstantSuggestion();
        if (UseTabForSuggestions()) {
          // On a search results page, tell it to clear old results.
          instant_tab_->sender()->Update(string16(), 0, 0, true);
        } else if (overlay_ && search_mode_.is_origin_ntp()) {
          // On the NTP, tell the overlay to clear old results. Don't hide the
          // overlay so it can show a blank page or logo if it wants.
          overlay_->Update(string16(), 0, 0, true);
        } else {
          HideOverlay();
        }
      } else {
        // The user switched to a tab with partial text already in the omnibox.
        HideOverlay();

        // The new tab may or may not be a search results page; we don't know
        // since SearchModeChanged() hasn't been called yet. If it later turns
        // out to be, we should store |full_text| now, so that if the user hits
        // Enter, we'll send the correct query to
        // instant_tab_->sender()->Submit(). If the partial text is not a query
        // (|last_match_was_search_| is false), we won't Submit(), so no need to
        // worry about that.
        last_user_text_ = user_text;
        last_suggestion_ = InstantSuggestion();
      }
      return false;
    } else if (full_text.empty()) {
      // The user typed a solitary "?". Same as the backspace case above.
      last_omnibox_text_.clear();
      last_user_text_.clear();
      last_suggestion_ = InstantSuggestion();
      if (UseTabForSuggestions())
        instant_tab_->sender()->Update(string16(), 0, 0, true);
      else if (overlay_ && search_mode_.is_origin_ntp())
        overlay_->Update(string16(), 0, 0, true);
      else
        HideOverlay();
      return false;
    }
  } else if (!omnibox_popup_is_open || full_text.empty()) {
    // In the non-extended case, hide the overlay as long as the user isn't
    // actively typing a non-empty query.
    HideOverlay();
    return false;
  }

  last_omnibox_text_has_inline_autocompletion_ = user_text != full_text;

  // If the user continues typing the same query as the suggested text is
  // showing, reuse the suggestion (but only for INSTANT_COMPLETE_NEVER).
  bool reused_suggestion = false;
  if (last_suggestion_.behavior == INSTANT_COMPLETE_NEVER &&
      !last_omnibox_text_has_inline_autocompletion_) {
    if (StartsWith(previous_omnibox_text, full_text, false))  {
      // The user is backspacing away characters.
      last_suggestion_.text.insert(0, previous_omnibox_text, full_text.size(),
          previous_omnibox_text.size() - full_text.size());
      reused_suggestion = true;
    } else if (StartsWith(full_text, previous_omnibox_text, false)) {
      // The user is typing forward. Normalize any added characters.
      reused_suggestion = NormalizeAndStripPrefix(&last_suggestion_.text,
          string16(full_text, previous_omnibox_text.size()));
    }
  }
  if (!reused_suggestion)
    last_suggestion_ = InstantSuggestion();

  // TODO(kmadhusu): Investigate whether it's possible to update
  // |last_user_text_| at the beginning of this function.
  last_user_text_ = user_text;

  if (!extended_enabled()) {
    // In non-extended mode, the query is verbatim if there's any selection
    // (including inline autocompletion) or if the cursor is not at the end.
    verbatim = verbatim || selection_start != selection_end ||
                           selection_start != full_text.size();
  }
  last_verbatim_ = verbatim;

  last_transition_type_ = match.transition;
  url_for_history_ = match.destination_url;

  // Allow search suggestions. In extended mode, SearchModeChanged() will set
  // this, but it's not called in non-extended mode, so fake it.
  if (!extended_enabled())
    search_mode_.mode = SearchMode::MODE_SEARCH_SUGGESTIONS;

  if (UseTabForSuggestions()) {
    instant_tab_->sender()->Update(user_text, selection_start,
                                   selection_end, verbatim);
  } else if (overlay_) {
    allow_overlay_to_show_search_suggestions_ = true;

    overlay_->Update(extended_enabled() ? user_text : full_text,
                     selection_start, selection_end, verbatim);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());

  // We don't have new suggestions yet, but we can either reuse the existing
  // suggestion or reset the existing "gray text".
  browser_->SetInstantSuggestion(last_suggestion_);

  // Record the time of the first keypress for logging histograms.
  if (!first_interaction_time_recorded_ && first_interaction_time_.is_null())
    first_interaction_time_ = base::Time::Now();

  return true;
}

bool InstantController::WillFetchCompletions() const {
  if (!extended_enabled())
    return false;

  return !UsingLocalPage();
}

scoped_ptr<content::WebContents> InstantController::ReleaseNTPContents() {
  if (!extended_enabled() || !browser_->profile() ||
      browser_->profile()->IsOffTheRecord() ||
      !chrome::ShouldShowInstantNTP())
    return scoped_ptr<content::WebContents>();

  LOG_INSTANT_DEBUG_EVENT(this, "ReleaseNTPContents");

  if (ShouldSwitchToLocalNTP())
    ResetNTP(GetLocalInstantURL());

  scoped_ptr<content::WebContents> ntp_contents = ntp_->ReleaseContents();

  // Preload a new Instant NTP.
  if (preload_ntp_)
    ResetNTP(GetInstantURL());
  else
    ntp_.reset();

  return ntp_contents.Pass();
}

// TODO(tonyg): This method only fires when the omnibox bounds change. It also
// needs to fire when the overlay bounds change (e.g.: open/close info bar).
void InstantController::SetPopupBounds(const gfx::Rect& bounds) {
  if (!extended_enabled() && !instant_enabled_)
    return;

  if (popup_bounds_ == bounds)
    return;

  popup_bounds_ = bounds;
  if (popup_bounds_.height() > last_popup_bounds_.height()) {
    update_bounds_timer_.Stop();
    SendPopupBoundsToPage();
  } else if (!update_bounds_timer_.IsRunning()) {
    update_bounds_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kUpdateBoundsDelayMS), this,
        &InstantController::SendPopupBoundsToPage);
  }
}

void InstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (!extended_enabled() || omnibox_bounds_ == bounds)
    return;

  omnibox_bounds_ = bounds;
  if (overlay_)
    overlay_->sender()->SetOmniboxBounds(omnibox_bounds_);
  if (ntp_)
    ntp_->sender()->SetOmniboxBounds(omnibox_bounds_);
  if (instant_tab_)
    instant_tab_->sender()->SetOmniboxBounds(omnibox_bounds_);
}

void InstantController::HandleAutocompleteResults(
    const std::vector<AutocompleteProvider*>& providers,
    const AutocompleteResult& autocomplete_result) {
  if (!extended_enabled())
    return;

  if (!UseTabForSuggestions() && !overlay_)
    return;

  // The omnibox sends suggestions when its possibly imaginary popup closes
  // as it stops autocomplete. Ignore these.
  if (omnibox_focus_state_ == OMNIBOX_FOCUS_NONE)
    return;

  for (ACProviders::const_iterator provider = providers.begin();
       provider != providers.end(); ++provider) {
    const bool from_search_provider =
        (*provider)->type() == AutocompleteProvider::TYPE_SEARCH;

    // TODO(jeremycho): Pass search_provider() as a parameter to this function
    // and remove the static cast.
    const bool provider_done = from_search_provider ?
        static_cast<SearchProvider*>(*provider)->IsNonInstantSearchDone() :
        (*provider)->done();
    if (!provider_done) {
      DVLOG(1) << "Waiting for " << (*provider)->GetName();
      return;
    }
  }

  DVLOG(1) << "AutocompleteResults:";
  std::vector<InstantAutocompleteResult> results;
  if (UsingLocalPage()) {
    for (AutocompleteResult::const_iterator match(autocomplete_result.begin());
         match != autocomplete_result.end(); ++match) {
      InstantAutocompleteResult result;
      PopulateInstantAutocompleteResultFromMatch(
          *match, std::distance(autocomplete_result.begin(), match), &result);
      results.push_back(result);
    }
  } else {
    for (ACProviders::const_iterator provider = providers.begin();
         provider != providers.end(); ++provider) {
      for (ACMatches::const_iterator match = (*provider)->matches().begin();
           match != (*provider)->matches().end(); ++match) {
        // When the top match is an inline history URL, the page calls
        // SetSuggestions(url) which calls FinalizeInstantQuery() in
        // SearchProvider creating a NAVSUGGEST match for the URL. If we sent
        // this NAVSUGGEST match back to the page, it would be deduped against
        // the original history match and replace it. But since the page ignores
        // SearchProvider suggestions, the match would then disappear. Yuck.
        // TODO(jered): Remove this when FinalizeInstantQuery() is ripped out.
        if ((*provider)->type() == AutocompleteProvider::TYPE_SEARCH &&
            match->type == AutocompleteMatchType::NAVSUGGEST) {
          continue;
        }
        InstantAutocompleteResult result;
        PopulateInstantAutocompleteResultFromMatch(*match, kNoMatchIndex,
                                                   &result);
        results.push_back(result);
      }
    }
  }
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "HandleAutocompleteResults: total_results=%d",
      static_cast<int>(results.size())));

  if (UseTabForSuggestions())
    instant_tab_->sender()->SendAutocompleteResults(results);
  else if (overlay_)
    overlay_->sender()->SendAutocompleteResults(results);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_SENT_AUTOCOMPLETE_RESULTS,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());
}

void InstantController::OnDefaultSearchProviderChanged() {
  if (ntp_ && extended_enabled()) {
    ntp_.reset();
    if (preload_ntp_)
      ResetNTP(GetInstantURL());
  }

  // Do not reload the overlay if it's actually the local overlay.
  if (overlay_ && !overlay_->IsLocal()) {
    overlay_.reset();
    if (extended_enabled() || instant_enabled_) {
      // Try to create another overlay immediately so that it is ready for the
      // next user interaction.
      ResetOverlay(GetInstantURL());
    }
  }
}

bool InstantController::OnUpOrDownKeyPressed(int count) {
  if (!extended_enabled())
    return false;

  if (!UseTabForSuggestions() && !overlay_)
    return false;

  if (UseTabForSuggestions())
    instant_tab_->sender()->UpOrDownKeyPressed(count);
  else if (overlay_)
    overlay_->sender()->UpOrDownKeyPressed(count);

  return true;
}

void InstantController::OnCancel(const AutocompleteMatch& match,
                                 const string16& user_text,
                                 const string16& full_text) {
  if (!extended_enabled())
    return;

  if (!UseTabForSuggestions() && !overlay_)
    return;

  // We manually reset the state here since the JS is not expected to do it.
  // TODO(sreeram): Handle the case where user_text is now a URL
  last_match_was_search_ = AutocompleteMatch::IsSearchType(match.type) &&
                           !full_text.empty();
  last_omnibox_text_ = full_text;
  last_user_text_ = user_text;
  last_suggestion_ = InstantSuggestion();

  // Say |full_text| is "amazon.com" and |user_text| is "ama". This means the
  // inline autocompletion is "zon.com"; so the selection should span from
  // user_text.size() to full_text.size(). The selection bounds are inverted
  // because the caret is at the end of |user_text|, not |full_text|.
  if (UseTabForSuggestions()) {
    instant_tab_->sender()->CancelSelection(user_text, full_text.size(),
                                            user_text.size(), last_verbatim_);
  } else if (overlay_) {
    overlay_->sender()->CancelSelection(user_text, full_text.size(),
                                        user_text.size(), last_verbatim_);
  }
}

void InstantController::OmniboxNavigateToURL() {
  RecordNavigationHistogram(UsingLocalPage(), false, extended_enabled());
  if (!extended_enabled())
    return;
  if (UseTabForSuggestions())
    instant_tab_->sender()->Submit(string16());
}

void InstantController::ToggleVoiceSearch() {
  if (instant_tab_)
    instant_tab_->sender()->ToggleVoiceSearch();
}

void InstantController::InstantPageLoadFailed(content::WebContents* contents) {
  if (!chrome::ShouldPreferRemoteNTPOnStartup() || !extended_enabled()) {
    // We only need to fall back on errors if we're showing the online page
    // at startup, as otherwise we fall back correctly when trying to show
    // a page that hasn't yet indicated that it supports the InstantExtended
    // API.
    return;
  }

  if (IsContentsFrom(instant_tab(), contents)) {
    // Verify we're not already on a local page and that the URL precisely
    // equals the instant_url (minus the query params, as those will be filled
    // in by template values).  This check is necessary to make sure we don't
    // inadvertently redirect to the local NTP if someone, say, reloads a SRP
    // while offline, as a committed results page still counts as an instant
    // url.  We also check to make sure there's no forward history, as if
    // someone hits the back button a lot when offline and returns to a NTP
    // we don't want to redirect and nuke their forward history stack.
    const GURL& current_url = contents->GetURL();
    if (instant_tab_->IsLocal() ||
        !chrome::MatchesOriginAndPath(GURL(GetInstantURL()), current_url) ||
        !current_url.ref().empty() ||
        contents->GetController().CanGoForward())
      return;
    LOG_INSTANT_DEBUG_EVENT(this, "InstantPageLoadFailed: instant_tab");
    RedirectToLocalNTP(contents);
  } else if (IsContentsFrom(ntp(), contents)) {
    LOG_INSTANT_DEBUG_EVENT(this, "InstantPageLoadFailed: ntp");
    bool is_local = ntp_->IsLocal();
    DeletePageSoon(ntp_.Pass());
    if (!is_local)
      ResetNTP(GetLocalInstantURL());
  } else if (IsContentsFrom(overlay(), contents)) {
    LOG_INSTANT_DEBUG_EVENT(this, "InstantPageLoadFailed: overlay");
    bool is_local = overlay_->IsLocal();
    DeletePageSoon(overlay_.Pass());
    if (!is_local)
      ResetOverlay(GetLocalInstantURL());
  }
}

content::WebContents* InstantController::GetOverlayContents() const {
  return overlay_ ? overlay_->contents() : NULL;
}

content::WebContents* InstantController::GetNTPContents() const {
  return ntp_ ? ntp_->contents() : NULL;
}

bool InstantController::IsOverlayingSearchResults() const {
  return model_.mode().is_search_suggestions() && IsFullHeight(model_) &&
         (last_match_was_search_ ||
          last_suggestion_.behavior == INSTANT_COMPLETE_NEVER);
}

bool InstantController::SubmitQuery(const string16& search_terms) {
  if (extended_enabled() && instant_tab_ && instant_tab_->supports_instant() &&
      search_mode_.is_origin_search()) {
    // Use |instant_tab_| to run the query if we're already on a search results
    // page. (NOTE: in particular, we do not send the query to NTPs.)
    instant_tab_->sender()->Submit(search_terms);
    instant_tab_->contents()->GetView()->Focus();
    EnsureSearchTermsAreSet(instant_tab_->contents(), search_terms);
    return true;
  }
  return false;
}

bool InstantController::CommitIfPossible(InstantCommitType type) {
  if (!extended_enabled() && !instant_enabled_)
    return false;

  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "CommitIfPossible: type=%d last_omnibox_text_='%s' "
      "last_match_was_search_=%d use_tab_for_suggestions=%d", type,
      UTF16ToUTF8(last_omnibox_text_).c_str(), last_match_was_search_,
      UseTabForSuggestions()));

  // If we are on an already committed search results page, send a submit event
  // to the page, but otherwise, nothing else to do.
  if (UseTabForSuggestions()) {
    if (type == INSTANT_COMMIT_PRESSED_ENTER &&
        !instant_tab_->IsLocal() &&
        (last_match_was_search_ ||
         last_suggestion_.behavior == INSTANT_COMPLETE_NEVER)) {
      last_suggestion_.text.clear();
      instant_tab_->sender()->Submit(last_omnibox_text_);
      instant_tab_->contents()->GetView()->Focus();
      EnsureSearchTermsAreSet(instant_tab_->contents(), last_omnibox_text_);
      return true;
    }
    return false;
  }

  if (!overlay_)
    return false;

  // If the overlay is not showing at all, don't commit it.
  if (!model_.mode().is_search_suggestions())
    return false;

  // If the overlay is showing at full height (with results), commit it.
  // If it's showing at parial height, commit if it's navigating.
  if (!IsOverlayingSearchResults() && type != INSTANT_COMMIT_NAVIGATED)
    return false;

  // There may re-entrance here, from the call to browser_->CommitInstant below,
  // which can cause a TabDeactivated notification which gets back here.
  // In this case, overlay_->ReleaseContents() was called already.
  if (!GetOverlayContents())
    return false;

  // Never commit the local overlay.
  if (overlay_->IsLocal())
    return false;

  if (type == INSTANT_COMMIT_FOCUS_LOST) {
    // Extended mode doesn't need or use the Cancel message.
    if (!extended_enabled())
      overlay_->sender()->Cancel(last_omnibox_text_);
  } else if (type != INSTANT_COMMIT_NAVIGATED) {
    overlay_->sender()->Submit(last_omnibox_text_);
  }

  // We expect the WebContents to be in a valid state (i.e., has a last
  // committed entry, no transient entry, and no existing pending entry).
  scoped_ptr<content::WebContents> overlay = overlay_->ReleaseContents();
  CHECK(overlay->GetController().CanPruneAllButVisible());

  // If the overlay page has navigated since the last Update(), we need to add
  // the navigation to history ourselves. Else, the page will navigate after
  // commit, and it will be added to history in the usual manner.
  const history::HistoryAddPageArgs& last_navigation =
      overlay_->last_navigation();
  if (!last_navigation.url.is_empty()) {
    content::NavigationEntry* entry = overlay->GetController().GetActiveEntry();

    // The last navigation should be the same as the active entry if the overlay
    // is in search mode. During navigation, the active entry could have
    // changed since DidCommitProvisionalLoadForFrame is called after the entry
    // is changed.
    // TODO(shishir): Should we commit the last navigation for
    // INSTANT_COMMIT_NAVIGATED.
    DCHECK(type == INSTANT_COMMIT_NAVIGATED ||
           last_navigation.url == entry->GetURL());

    // Add the page to history.
    HistoryTabHelper* history_tab_helper =
        HistoryTabHelper::FromWebContents(overlay.get());
    history_tab_helper->UpdateHistoryForNavigation(last_navigation);

    // Update the page title.
    history_tab_helper->UpdateHistoryPageTitle(*entry);
  }

  // Add a fake history entry with a non-Instant search URL, so that search
  // terms extraction (for autocomplete history matches) works.
  HistoryService* history = HistoryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(overlay->GetBrowserContext()),
      Profile::EXPLICIT_ACCESS);
  if (history) {
    history->AddPage(url_for_history_, base::Time::Now(), NULL, 0, GURL(),
                     history::RedirectList(), last_transition_type_,
                     history::SOURCE_BROWSED, false);
  }

  if (type == INSTANT_COMMIT_PRESSED_ALT_ENTER) {
    overlay->GetController().PruneAllButVisible();
  } else {
    content::WebContents* active_tab = browser_->GetActiveWebContents();
    AddSessionStorageHistogram(extended_enabled(), active_tab, overlay.get());
    overlay->GetController().CopyStateFromAndPrune(
        &active_tab->GetController());
  }

  if (extended_enabled()) {
    // Adjust the search terms shown in the omnibox for this query. Hitting
    // ENTER searches for what the user typed, so use last_omnibox_text_.
    // Clicking on the overlay commits what is currently showing, so add in the
    // gray text in that case.
    if (type == INSTANT_COMMIT_FOCUS_LOST &&
        last_suggestion_.behavior == INSTANT_COMPLETE_NEVER) {
      // Update |last_omnibox_text_| so that the controller commits the proper
      // query if the user focuses the omnibox and presses Enter.
      last_omnibox_text_ += last_suggestion_.text;
    }

    EnsureSearchTermsAreSet(overlay.get(), last_omnibox_text_);
  }

  // Save notification source before we release the overlay.
  content::Source<content::WebContents> notification_source(overlay.get());

  browser_->CommitInstant(overlay.Pass(),
                          type == INSTANT_COMMIT_PRESSED_ALT_ENTER);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_COMMITTED,
      notification_source,
      content::NotificationService::NoDetails());

  // Hide explicitly. See comments in HideOverlay() for why.
  model_.SetOverlayState(SearchMode(), 0, INSTANT_SIZE_PERCENT);

  // Delay deletion as we could've gotten here from an InstantOverlay method.
  DeletePageSoon(overlay_.Pass());

  // Try to create another overlay immediately so that it is ready for the next
  // user interaction.
  ResetOverlay(GetInstantURL());

  if (instant_tab_)
    use_tab_for_suggestions_ = true;

  LOG_INSTANT_DEBUG_EVENT(this, "Committed");
  return true;
}

void InstantController::OmniboxFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason,
    gfx::NativeView view_gaining_focus) {
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "OmniboxFocusChanged: %d to %d for reason %d", omnibox_focus_state_,
      state, reason));

  OmniboxFocusState old_focus_state = omnibox_focus_state_;
  omnibox_focus_state_ = state;
  if (!extended_enabled() && !instant_enabled_)
    return;

  if (extended_enabled()) {
    if (overlay_)
      overlay_->sender()->FocusChanged(omnibox_focus_state_, reason);

    if (instant_tab_) {
      instant_tab_->sender()->FocusChanged(omnibox_focus_state_, reason);
      // Don't send oninputstart/oninputend updates in response to focus changes
      // if there's a navigation in progress. This prevents Chrome from sending
      // a spurious oninputend when the user accepts a match in the omnibox.
      if (instant_tab_->contents()->GetController().GetPendingEntry() == NULL)
        instant_tab_->sender()->SetInputInProgress(IsInputInProgress());
    }
  }

  if (state == OMNIBOX_FOCUS_VISIBLE && old_focus_state == OMNIBOX_FOCUS_NONE) {
    // If the user explicitly focused the omnibox, then create the overlay if
    // it doesn't exist. If we're using a fallback overlay, try loading the
    // remote overlay again.
    if (!overlay_ || (overlay_->IsLocal() && !use_local_page_only_))
      ResetOverlay(GetInstantURL());
  } else if (state == OMNIBOX_FOCUS_NONE &&
             old_focus_state != OMNIBOX_FOCUS_NONE) {
    // If the focus went from the omnibox to outside the omnibox, commit or
    // discard the overlay.
    OmniboxLostFocus(view_gaining_focus);
  }
}

void InstantController::SearchModeChanged(const SearchMode& old_mode,
                                          const SearchMode& new_mode) {
  if (!extended_enabled())
    return;

  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "SearchModeChanged: [origin:mode] %d:%d to %d:%d", old_mode.origin,
      old_mode.mode, new_mode.origin, new_mode.mode));

  search_mode_ = new_mode;
  if (!new_mode.is_search_suggestions())
    HideOverlay();

  ResetInstantTab();

  if (instant_tab_ && old_mode.is_ntp() != new_mode.is_ntp())
    instant_tab_->sender()->SetInputInProgress(IsInputInProgress());
}

void InstantController::ActiveTabChanged() {
  if (!extended_enabled() && !instant_enabled_)
    return;

  LOG_INSTANT_DEBUG_EVENT(this, "ActiveTabChanged");

  // When switching tabs, always hide the overlay.
  HideOverlay();

  if (extended_enabled())
    ResetInstantTab();
}

void InstantController::TabDeactivated(content::WebContents* contents) {
  LOG_INSTANT_DEBUG_EVENT(this, "TabDeactivated");
  if (extended_enabled() && !contents->IsBeingDestroyed())
    CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);

  if (GetOverlayContents())
    HideOverlay();
}

void InstantController::SetInstantEnabled(bool instant_enabled,
                                          bool use_local_page_only) {
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "SetInstantEnabled: instant_enabled=%d, use_local_page_only=%d",
      instant_enabled, use_local_page_only));

  // Non extended mode does not care about |use_local_page_only|.
  if (instant_enabled == instant_enabled_ &&
      (!extended_enabled() ||
       use_local_page_only == use_local_page_only_)) {
    return;
  }

  instant_enabled_ = instant_enabled;
  use_local_page_only_ = use_local_page_only;
  preload_ntp_ = !use_local_page_only_ || chrome::ShouldPreloadLocalOnlyNTP();

  // Preload the overlay.
  HideInternal();
  overlay_.reset();
  if (extended_enabled() || instant_enabled_)
    ResetOverlay(GetInstantURL());

  // Preload the Instant NTP.
  ntp_.reset();
  if (extended_enabled() && preload_ntp_)
    ResetNTP(GetInstantURL());

  if (instant_tab_)
    instant_tab_->sender()->SetDisplayInstantResults(instant_enabled_);
}

void InstantController::ThemeInfoChanged(
    const ThemeBackgroundInfo& theme_info) {
  if (!extended_enabled())
    return;

  if (overlay_)
    overlay_->sender()->SendThemeBackgroundInfo(theme_info);
  if (ntp_)
    ntp_->sender()->SendThemeBackgroundInfo(theme_info);
  if (instant_tab_)
    instant_tab_->sender()->SendThemeBackgroundInfo(theme_info);
}

void InstantController::SwappedOverlayContents() {
  model_.SetOverlayContents(GetOverlayContents());
}

void InstantController::FocusedOverlayContents() {
#if defined(USE_AURA)
  // On aura the omnibox only receives a focus lost if we initiate the focus
  // change. This does that.
  if (!model_.mode().is_default())
    browser_->InstantOverlayFocused();
#endif
}

void InstantController::ReloadOverlayIfStale() {
  // The local overlay is never stale.
  if (overlay_ && (overlay_->IsLocal() || !overlay_->is_stale()))
    return;

  // If the overlay is showing or the omnibox has focus, don't refresh the
  // overlay. It will get refreshed the next time the overlay is hidden or the
  // omnibox loses focus.
  if (omnibox_focus_state_ == OMNIBOX_FOCUS_NONE && model_.mode().is_default())
    ResetOverlay(GetInstantURL());
}

void InstantController::OverlayLoadCompletedMainFrame() {
  if (!overlay_ || overlay_->supports_instant())
    return;
  InstantService* instant_service = GetInstantService();
  content::WebContents* contents = overlay_->contents();
  DCHECK(contents);
  if (instant_service->IsInstantProcess(
      contents->GetRenderProcessHost()->GetID())) {
    return;
  }
  InstantSupportDetermined(contents, false);
}

void InstantController::LogDebugEvent(const std::string& info) const {
  DVLOG(1) << info;

  debug_events_.push_front(std::make_pair(
      base::Time::Now().ToInternalValue(), info));
  static const size_t kMaxDebugEventSize = 2000;
  if (debug_events_.size() > kMaxDebugEventSize)
    debug_events_.pop_back();
}

void InstantController::ClearDebugEvents() {
  debug_events_.clear();
}

void InstantController::UpdateMostVisitedItems() {
  InstantService* instant_service = GetInstantService();
  if (!instant_service)
    return;

  std::vector<InstantMostVisitedItem> items;
  instant_service->GetCurrentMostVisitedItems(&items);

  if (overlay_)
    overlay_->sender()->SendMostVisitedItems(items);

  if (ntp_)
    ntp_->sender()->SendMostVisitedItems(items);

  if (instant_tab_)
    instant_tab_->sender()->SendMostVisitedItems(items);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_SENT_MOST_VISITED_ITEMS,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());
}

void InstantController::DeleteMostVisitedItem(const GURL& url) {
  DCHECK(!url.is_empty());
  InstantService* instant_service = GetInstantService();
  if (!instant_service)
    return;

  instant_service->DeleteMostVisitedItem(url);
}

void InstantController::UndoMostVisitedDeletion(const GURL& url) {
  DCHECK(!url.is_empty());
  InstantService* instant_service = GetInstantService();
  if (!instant_service)
    return;

  instant_service->UndoMostVisitedDeletion(url);
}

void InstantController::UndoAllMostVisitedDeletions() {
  InstantService* instant_service = GetInstantService();
  if (!instant_service)
    return;

  instant_service->UndoAllMostVisitedDeletions();
}

Profile* InstantController::profile() const {
  return browser_->profile();
}

InstantOverlay* InstantController::overlay() const {
  return overlay_.get();
}

InstantTab* InstantController::instant_tab() const {
  return instant_tab_.get();
}

InstantNTP* InstantController::ntp() const {
  return ntp_.get();
}

// TODO(shishir): We assume that the WebContent's current RenderViewHost is the
// RenderViewHost being created which is not always true. Fix this.
void InstantController::InstantPageRenderViewCreated(
    const content::WebContents* contents) {
  if (!extended_enabled())
    return;

  // Update theme info so that the page picks it up.
  InstantService* instant_service = GetInstantService();
  if (instant_service)
    instant_service->UpdateThemeInfo();

  // Ensure the searchbox API has the correct initial state.
  if (IsContentsFrom(overlay(), contents)) {
    overlay_->sender()->SetDisplayInstantResults(instant_enabled_);
    overlay_->sender()->FocusChanged(omnibox_focus_state_,
                                     omnibox_focus_change_reason_);
    overlay_->sender()->SetOmniboxBounds(omnibox_bounds_);
    overlay_->InitializeFonts();
  } else if (IsContentsFrom(ntp(), contents)) {
    ntp_->sender()->SetDisplayInstantResults(instant_enabled_);
    ntp_->sender()->SetOmniboxBounds(omnibox_bounds_);
    ntp_->InitializeFonts();
    ntp_->InitializePromos();
  } else {
    NOTREACHED();
  }
  UpdateMostVisitedItems();
}

void InstantController::InstantSupportChanged(
    InstantSupportState instant_support) {
  // Handle INSTANT_SUPPORT_YES here because InstantPage is not hooked up to the
  // active tab. Search model changed listener in InstantPage will handle other
  // cases.
  if (instant_support != INSTANT_SUPPORT_YES)
    return;

  ResetInstantTab();
}

void InstantController::InstantSupportDetermined(
    const content::WebContents* contents,
    bool supports_instant) {
  if (IsContentsFrom(instant_tab(), contents)) {
    if (!supports_instant)
      base::MessageLoop::current()->DeleteSoon(FROM_HERE,
                                               instant_tab_.release());

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
        content::Source<InstantController>(this),
        content::NotificationService::NoDetails());
  } else if (IsContentsFrom(ntp(), contents)) {
    if (!supports_instant) {
      bool is_local = ntp_->IsLocal();
      DeletePageSoon(ntp_.Pass());
      // Preload a local NTP in place of the broken online one.
      if (!is_local)
        ResetNTP(GetLocalInstantURL());
    }

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_NTP_SUPPORT_DETERMINED,
        content::Source<InstantController>(this),
        content::NotificationService::NoDetails());

  } else if (IsContentsFrom(overlay(), contents)) {
    if (!supports_instant) {
      HideInternal();
      bool is_local = overlay_->IsLocal();
      DeletePageSoon(overlay_.Pass());
      // Preload a local overlay in place of the broken online one.
      if (!is_local && extended_enabled())
        ResetOverlay(GetLocalInstantURL());
    }

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_OVERLAY_SUPPORT_DETERMINED,
        content::Source<InstantController>(this),
        content::NotificationService::NoDetails());
  }
}

void InstantController::InstantPageRenderViewGone(
    const content::WebContents* contents) {
  if (IsContentsFrom(overlay(), contents)) {
    HideInternal();
    DeletePageSoon(overlay_.Pass());
  } else if (IsContentsFrom(ntp(), contents)) {
    DeletePageSoon(ntp_.Pass());
  } else {
    NOTREACHED();
  }
}

void InstantController::InstantPageAboutToNavigateMainFrame(
    const content::WebContents* contents,
    const GURL& url) {
  if (IsContentsFrom(overlay(), contents)) {
    // If the page does not yet support Instant, we allow redirects and other
    // navigations to go through since the Instant URL can redirect - e.g. to
    // country specific pages.
    if (!overlay_->supports_instant())
      return;

    GURL instant_url(overlay_->instant_url());

    // If we are navigating to the Instant URL, do nothing.
    if (url == instant_url)
      return;

    // Commit the navigation if either:
    //  - The page is in NTP mode (so it could only navigate on a user click) or
    //  - The page is not in NTP mode and we are navigating to a URL with a
    //    different host or path than the Instant URL. This enables the instant
    //    page when it is showing search results to change the query parameters
    //    and fragments of the URL without it navigating.
    if (model_.mode().is_ntp() ||
        (url.host() != instant_url.host() ||
         url.path() != instant_url.path())) {
      CommitIfPossible(INSTANT_COMMIT_NAVIGATED);
    }
  } else if (IsContentsFrom(instant_tab(), contents)) {
    // The Instant tab navigated.  Send it the data it needs to display
    // properly.
    UpdateInfoForInstantTab();
  } else {
    NOTREACHED();
  }
}

void InstantController::SetSuggestions(
    const content::WebContents* contents,
    const std::vector<InstantSuggestion>& suggestions) {
  LOG_INSTANT_DEBUG_EVENT(this, "SetSuggestions");

  // Ignore if the message is from an unexpected source.
  if (IsContentsFrom(ntp(), contents))
    return;
  if (UseTabForSuggestions() && !IsContentsFrom(instant_tab(), contents))
    return;
  if (IsContentsFrom(overlay(), contents) &&
      !allow_overlay_to_show_search_suggestions_)
    return;

  InstantSuggestion suggestion;
  if (!suggestions.empty())
    suggestion = suggestions[0];

  // TODO(samarth): allow InstantTabs to call SetSuggestions() from the NTP once
  // that is better supported.
  bool can_use_instant_tab = UseTabForSuggestions() &&
      search_mode_.is_search();
  bool can_use_overlay = search_mode_.is_search_suggestions() &&
      !last_omnibox_text_.empty();
  if (!can_use_instant_tab && !can_use_overlay)
    return;

  if (suggestion.behavior == INSTANT_COMPLETE_REPLACE) {
    if (omnibox_focus_state_ == OMNIBOX_FOCUS_NONE) {
      // TODO(samarth,skanuj): setValue() needs to be handled differently when
      // the omnibox doesn't have focus. Instead of setting temporary text, we
      // should be setting search terms on the appropriate NavigationEntry.
      // (Among other things, this ensures that URL-shaped values will get the
      // additional security token.)
      //
      // Note that this also breaks clicking on a suggestion corresponding to
      // gray-text completion: we can't distinguish between the user
      // clicking on white space (where we don't accept the gray text) and the
      // user clicking on the suggestion (when we do accept the gray text).
      // This needs to be fixed before we can turn on Instant again.
      return;
    }

    // We don't get an Update() when changing the omnibox due to a REPLACE
    // suggestion (so that we don't inadvertently cause the overlay to change
    // what it's showing, as the user arrows up/down through the page-provided
    // suggestions). So, update these state variables here.
    last_omnibox_text_ = suggestion.text;
    last_user_text_.clear();
    last_suggestion_ = InstantSuggestion();
    last_match_was_search_ = suggestion.type == INSTANT_SUGGESTION_SEARCH;
    LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
        "ReplaceSuggestion text='%s' type=%d",
        UTF16ToUTF8(suggestion.text).c_str(), suggestion.type));
    browser_->SetInstantSuggestion(suggestion);
  } else {
    if (FixSuggestion(&suggestion)) {
      last_suggestion_ = suggestion;
      if (suggestion.type == INSTANT_SUGGESTION_SEARCH &&
          suggestion.behavior == INSTANT_COMPLETE_NEVER)
        last_omnibox_text_ = last_user_text_;
      LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
          "SetInstantSuggestion: text='%s' behavior=%d",
          UTF16ToUTF8(suggestion.text).c_str(),
          suggestion.behavior));
      browser_->SetInstantSuggestion(suggestion);
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_INSTANT_SET_SUGGESTION,
          content::Source<InstantController>(this),
          content::NotificationService::NoDetails());
    } else {
      last_suggestion_ = InstantSuggestion();
    }
  }

  // Extended mode pages will call ShowOverlay() when they are ready.
  if (!extended_enabled())
    ShowOverlay(100, INSTANT_SIZE_PERCENT);
}

void InstantController::ShowInstantOverlay(const content::WebContents* contents,
                                           int height,
                                           InstantSizeUnits units) {
  if (extended_enabled() && IsContentsFrom(overlay(), contents))
    ShowOverlay(height, units);
}

void InstantController::LogDropdownShown() {
  // If suggestions are being shown for the first time since the user started
  // typing, record a histogram value.
  if (!first_interaction_time_.is_null() && !first_interaction_time_recorded_) {
    base::TimeDelta delta = base::Time::Now() - first_interaction_time_;
    first_interaction_time_recorded_ = true;
    if (search_mode_.is_origin_ntp()) {
      UMA_HISTOGRAM_TIMES("Instant.TimeToFirstShowFromNTP", delta);
      LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
          "LogShowInstantOverlay: TimeToFirstShowFromNTP=%d",
          static_cast<int>(delta.InMilliseconds())));
    } else if (search_mode_.is_origin_search()) {
      UMA_HISTOGRAM_TIMES("Instant.TimeToFirstShowFromSERP", delta);
      LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
          "LogShowInstantOverlay: TimeToFirstShowFromSERP=%d",
          static_cast<int>(delta.InMilliseconds())));
    } else {
      UMA_HISTOGRAM_TIMES("Instant.TimeToFirstShowFromWeb", delta);
      LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
          "LogShowInstantOverlay: TimeToFirstShowFromWeb=%d",
          static_cast<int>(delta.InMilliseconds())));
    }
  }
}

void InstantController::FocusOmnibox(const content::WebContents* contents,
                                     OmniboxFocusState state) {
  if (!extended_enabled())
    return;

  DCHECK(IsContentsFrom(instant_tab(), contents));

  // Do not add a default case in the switch block for the following reasons:
  // (1) Explicitly handle the new states. If new states are added in the
  // OmniboxFocusState, the compiler will warn the developer to handle the new
  // states.
  // (2) An attacker may control the renderer and sends the browser process a
  // malformed IPC. This function responds to the invalid |state| values by
  // doing nothing instead of crashing the browser process (intentional no-op).
  switch (state) {
    case OMNIBOX_FOCUS_VISIBLE:
      // TODO(samarth): re-enable this once setValue() correctly handles
      // URL-shaped queries.
      // browser_->FocusOmnibox(true);
      break;
    case OMNIBOX_FOCUS_INVISIBLE:
      browser_->FocusOmnibox(false);
      break;
    case OMNIBOX_FOCUS_NONE:
      if (omnibox_focus_state_ != OMNIBOX_FOCUS_INVISIBLE)
        contents->GetView()->Focus();
      break;
  }
}

void InstantController::NavigateToURL(const content::WebContents* contents,
                                      const GURL& url,
                                      content::PageTransition transition,
                                      WindowOpenDisposition disposition,
                                      bool is_search_type) {
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "NavigateToURL: url='%s'", url.spec().c_str()));

  // TODO(samarth): handle case where contents are no longer "active" (e.g. user
  // has switched tabs).
  if (!extended_enabled())
    return;
  if (overlay_) {
    HideOverlay();
  }

  if (transition == content::PAGE_TRANSITION_AUTO_BOOKMARK) {
    content::RecordAction(
        content::UserMetricsAction("InstantExtended.MostVisitedClicked"));
  } else {
    // Exclude navigation by Most Visited click and searches.
    if (!is_search_type)
      RecordNavigationHistogram(UsingLocalPage(), true, true);
  }
  browser_->OpenURL(url, transition, disposition);
}

void InstantController::OmniboxLostFocus(gfx::NativeView view_gaining_focus) {
  // If the overlay is showing custom NTP content, don't hide it, commit it
  // (no matter where the user clicked) or try to recreate it.
  if (model_.mode().is_ntp())
    return;

  if (model_.mode().is_default()) {
    // If the overlay is not showing at all, recreate it if it's stale.
    ReloadOverlayIfStale();
    return;
  }

  // The overlay is showing search suggestions. If GetOverlayContents() is NULL,
  // we are in the commit path. Don't do anything.
  if (!GetOverlayContents())
    return;

#if defined(OS_MACOSX)
  // TODO(sreeram): See if Mac really needs this special treatment.
  if (!overlay_->is_pointer_down_from_activate())
    HideOverlay();
#else
  if (IsFullHeight(model_))
    CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);
  else if (!IsViewInContents(GetViewGainingFocus(view_gaining_focus),
                             overlay_->contents()))
    HideOverlay();
#endif
}

std::string InstantController::GetLocalInstantURL() const {
  return chrome::GetLocalInstantURL(profile()).spec();
}

std::string InstantController::GetInstantURL() const {
  if (extended_enabled() &&
      (use_local_page_only_ || net::NetworkChangeNotifier::IsOffline()))
    return GetLocalInstantURL();

  const GURL instant_url = chrome::GetInstantURL(profile(),
                                                 omnibox_bounds_.x());
  if (instant_url.is_valid())
    return instant_url.spec();

  // Only extended mode has a local fallback.
  return extended_enabled() ? GetLocalInstantURL() : std::string();
}

bool InstantController::extended_enabled() const {
  return extended_enabled_;
}

bool InstantController::PageIsCurrent(const InstantPage* page) const {

  const std::string& instant_url = GetInstantURL();
  if (instant_url.empty() ||
      !chrome::MatchesOriginAndPath(GURL(page->instant_url()),
                                    GURL(instant_url)))
    return false;

  return page->supports_instant();
}

void InstantController::ResetNTP(const std::string& instant_url) {
  // Never load the Instant NTP if it is disabled.
  if (!chrome::ShouldShowInstantNTP())
    return;

  // Instant NTP is only used in extended mode so we should always have a
  // non-empty URL to use.
  DCHECK(!instant_url.empty());
  ntp_.reset(new InstantNTP(this, instant_url,
                            browser_->profile()->IsOffTheRecord()));
  ntp_->InitContents(profile(), browser_->GetActiveWebContents(),
                     base::Bind(&InstantController::ReloadStaleNTP,
                                base::Unretained(this)));
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "ResetNTP: instant_url='%s'", instant_url.c_str()));
}

void InstantController::ReloadStaleNTP() {
  ResetNTP(GetInstantURL());
}

bool InstantController::ShouldSwitchToLocalNTP() const {
  if (!ntp())
    return true;

  // Assume users with Javascript disabled do not want the online experience.
  if (!IsJavascriptEnabled())
    return true;

  // Already a local page. Not calling IsLocal() because we want to distinguish
  // between the Google-specific and generic local NTP.
  if (extended_enabled() && ntp()->instant_url() == GetLocalInstantURL())
    return false;

  if (PageIsCurrent(ntp()))
    return false;

  // The preloaded NTP does not support instant yet. If we're not in startup,
  // always fall back to the local NTP. If we are in startup, use the local NTP
  // (unless the finch flag to use the remote NTP is set).
  return !(InStartup() && chrome::ShouldPreferRemoteNTPOnStartup());
}

void InstantController::ResetOverlay(const std::string& instant_url) {
  HideInternal();
  overlay_.reset();
}

InstantController::InstantFallbackReason
InstantController::ShouldSwitchToLocalOverlay() const {
  if (!extended_enabled())
    return INSTANT_FALLBACK_NONE;

  if (!overlay())
    return DetermineFallbackReason(NULL, std::string());

  // Assume users with Javascript disabled do not want the online experience.
  if (!IsJavascriptEnabled())
    return INSTANT_FALLBACK_JAVASCRIPT_DISABLED;

  if (overlay()->IsLocal())
    return INSTANT_FALLBACK_NONE;

  bool page_is_current = PageIsCurrent(overlay());
  if (!page_is_current)
    return DetermineFallbackReason(overlay(), GetInstantURL());

  return INSTANT_FALLBACK_NONE;
}

void InstantController::ResetInstantTab() {
  if (!search_mode_.is_origin_default()) {
    content::WebContents* active_tab = browser_->GetActiveWebContents();
    if (!instant_tab_ || active_tab != instant_tab_->contents()) {
      instant_tab_.reset(
          new InstantTab(this, browser_->profile()->IsOffTheRecord()));
      instant_tab_->Init(active_tab);
      UpdateInfoForInstantTab();
      use_tab_for_suggestions_ = true;
    }

    // Hide the |overlay_| since we are now using |instant_tab_| instead.
    HideOverlay();
  } else {
    instant_tab_.reset();
  }
}

void InstantController::UpdateInfoForInstantTab() {
  if (instant_tab_) {
    instant_tab_->sender()->SetDisplayInstantResults(instant_enabled_);
    instant_tab_->sender()->SetOmniboxBounds(omnibox_bounds_);

    // Update theme details.
    InstantService* instant_service = GetInstantService();
    if (instant_service)
      instant_service->UpdateThemeInfo();

    instant_tab_->InitializeFonts();
    instant_tab_->InitializePromos();
    UpdateMostVisitedItems();
    instant_tab_->sender()->FocusChanged(omnibox_focus_state_,
                                         omnibox_focus_change_reason_);
    instant_tab_->sender()->SetInputInProgress(IsInputInProgress());
  }
}

bool InstantController::IsInputInProgress() const {
  return !search_mode_.is_ntp() &&
      omnibox_focus_state_ == OMNIBOX_FOCUS_VISIBLE;
}

void InstantController::HideOverlay() {
  HideInternal();
  ReloadOverlayIfStale();
}

void InstantController::HideInternal() {
  LOG_INSTANT_DEBUG_EVENT(this, "Hide");

  // If GetOverlayContents() returns NULL, either we're already in the desired
  // MODE_DEFAULT state, or we're in the commit path. For the latter, don't
  // change the state just yet; else we may hide the overlay unnecessarily.
  // Instead, the state will be set correctly after the commit is done.
  if (GetOverlayContents()) {
    model_.SetOverlayState(SearchMode(), 0, INSTANT_SIZE_PERCENT);
    allow_overlay_to_show_search_suggestions_ = false;

    // Send a message asking the overlay to clear out old results.
    overlay_->Update(string16(), 0, 0, true);
  }

  // Clear the first interaction timestamp for later use.
  first_interaction_time_ = base::Time();
  first_interaction_time_recorded_ = false;

  if (instant_tab_)
    use_tab_for_suggestions_ = true;
}

void InstantController::ShowOverlay(int height, InstantSizeUnits units) {
  // Nothing to see here.
  if (!overlay_)
    return;

  // If we are on a committed search results page, the |overlay_| is not in use.
  if (UseTabForSuggestions())
    return;

  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "Show: height=%d units=%d", height, units));

  // Must have updated omnibox after the last HideOverlay() to show suggestions.
  if (!allow_overlay_to_show_search_suggestions_)
    return;

  // The page is trying to hide itself. Hide explicitly (i.e., don't use
  // HideOverlay()) so that it can change its mind.
  if (height == 0) {
    model_.SetOverlayState(SearchMode(), 0, INSTANT_SIZE_PERCENT);
    if (instant_tab_)
      use_tab_for_suggestions_ = true;
    return;
  }

  // Show at 100% height except in the following cases:
  // - The local overlay (omnibox popup) is being loaded.
  // - Instant is disabled. The page needs to be able to show only a dropdown.
  // - The page is over a website other than search or an NTP, and is not
  //   already showing at 100% height.
  if (overlay_->IsLocal() || !instant_enabled_ ||
      (search_mode_.is_origin_default() && !IsFullHeight(model_)))
    model_.SetOverlayState(search_mode_, height, units);
  else
    model_.SetOverlayState(search_mode_, 100, INSTANT_SIZE_PERCENT);

  // If the overlay is being shown at full height and the omnibox is not
  // focused, commit right away.
  if (IsFullHeight(model_) && omnibox_focus_state_ == OMNIBOX_FOCUS_NONE)
    CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);
}

void InstantController::SendPopupBoundsToPage() {
  if (last_popup_bounds_ == popup_bounds_ || !overlay_ ||
      overlay_->is_pointer_down_from_activate())
    return;

  last_popup_bounds_ = popup_bounds_;
  gfx::Rect overlay_bounds = browser_->GetInstantBounds();
  gfx::Rect intersection = gfx::IntersectRects(popup_bounds_, overlay_bounds);

  // Translate into window coordinates.
  if (!intersection.IsEmpty()) {
    intersection.Offset(-overlay_bounds.origin().x(),
                        -overlay_bounds.origin().y());
  }

  // In the current Chrome UI, these must always be true so they sanity check
  // the above operations. In a future UI, these may be removed or adjusted.
  // There is no point in sanity-checking |intersection.y()| because the omnibox
  // can be placed anywhere vertically relative to the overlay (for example, in
  // Mac fullscreen mode, the omnibox is fully enclosed by the overlay bounds).
  DCHECK_LE(0, intersection.x());
  DCHECK_LE(0, intersection.width());
  DCHECK_LE(0, intersection.height());

  overlay_->sender()->SetPopupBounds(intersection);
}



bool InstantController::FixSuggestion(InstantSuggestion* suggestion) const {
  // We only accept suggestions if the user has typed text. If the user is
  // arrowing up/down (|last_user_text_| is empty), we reject suggestions.
  if (last_user_text_.empty())
    return false;

  // If the page is trying to set inline autocompletion in verbatim mode,
  // instead try suggesting the exact omnibox text. This makes the omnibox
  // interpret user text as an URL if possible while preventing unwanted
  // autocompletion during backspacing.
  if (suggestion->behavior == INSTANT_COMPLETE_NOW && last_verbatim_)
    suggestion->text = last_omnibox_text_;

  // Suggestion text should be a full URL for URL suggestions, or the
  // completion of a query for query suggestions.
  if (suggestion->type == INSTANT_SUGGESTION_URL) {
    // If the suggestion is not a valid URL, perhaps it's something like
    // "foo.com". Try prefixing "http://". If it still isn't valid, drop it.
    if (!GURL(suggestion->text).is_valid()) {
      suggestion->text.insert(0, ASCIIToUTF16("http://"));
      if (!GURL(suggestion->text).is_valid())
        return false;
    }

    // URL suggestions are only accepted if the query for which the suggestion
    // was generated is the same as |last_user_text_|.
    //
    // Any other URL suggestions--in particular suggestions for old user_text
    // lagging behind a slow IPC--are ignored. See crbug.com/181589.
    //
    // TODO(samarth): Accept stale suggestions if they would be accepted by
    // SearchProvider as an inlinable suggestion. http://crbug.com/191656.
    return suggestion->query == last_user_text_;
  }

  // We use |last_user_text_| because |last_omnibox_text| may contain text from
  // a previous URL suggestion at this point.
  if (suggestion->type == INSTANT_SUGGESTION_SEARCH) {
    if (StartsWith(suggestion->text, last_user_text_, true)) {
      // The user typed an exact prefix of the suggestion.
      suggestion->text.erase(0, last_user_text_.size());
      return true;
    } else if (NormalizeAndStripPrefix(&suggestion->text, last_user_text_)) {
      // Unicode normalize and case-fold the user text and suggestion. If the
      // user text is a prefix, suggest the normalized, case-folded completion
      // for instance, if the user types 'i' and the suggestion is 'INSTANT',
      // suggest 'nstant'. Otherwise, the user text really isn't a prefix, so
      // suggest nothing.
      // TODO(samarth|jered): revisit this logic. http://crbug.com/196572.
      return true;
    }
  }

  return false;
}

bool InstantController::UsingLocalPage() const {
  return (UseTabForSuggestions() && instant_tab_->IsLocal()) ||
      (!UseTabForSuggestions() && overlay_ && overlay_->IsLocal());
}

bool InstantController::UseTabForSuggestions() const {
  return instant_tab_ && use_tab_for_suggestions_;
}

void InstantController::RedirectToLocalNTP(content::WebContents* contents) {
  contents->GetController().LoadURL(
    chrome::GetLocalInstantURL(browser_->profile()),
    content::Referrer(),
    content::PAGE_TRANSITION_SERVER_REDIRECT,
    std::string());  // No extra headers.
  // TODO(dcblack): Remove extraneous history entry caused by 404s.
  // Note that the base case of a 204 being returned doesn't push a history
  // entry.
}

void InstantController::PopulateInstantAutocompleteResultFromMatch(
    const AutocompleteMatch& match, size_t autocomplete_match_index,
    InstantAutocompleteResult* result) {
  DCHECK(result);
  result->provider = UTF8ToUTF16(match.provider->GetName());
  result->type = match.type;
  result->description = match.description;
  result->destination_url = UTF8ToUTF16(match.destination_url.spec());

  // Setting the search_query field tells the Instant page to treat the
  // suggestion as a query.
  if (AutocompleteMatch::IsSearchType(match.type))
    result->search_query = match.contents;

  result->transition = match.transition;
  result->relevance = match.relevance;
  result->autocomplete_match_index = autocomplete_match_index;

  DVLOG(1) << "    " << result->relevance << " "
      << UTF8ToUTF16(AutocompleteMatchType::ToString(result->type)) << " "
      << result->provider << " " << result->destination_url << " '"
      << result->description << "' '" << result->search_query << "' "
      << result->transition <<  " " << result->autocomplete_match_index;
}

bool InstantController::IsJavascriptEnabled() const {
  GURL instant_url(GetInstantURL());
  GURL origin(instant_url.GetOrigin());
  ContentSetting js_setting = profile()->GetHostContentSettingsMap()->
      GetContentSetting(origin, origin, CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                        NO_RESOURCE_IDENTIFIER);
  // Javascript can be disabled either in content settings or via a WebKit
  // preference, so check both. Disabling it through the Settings page affects
  // content settings. I'm not sure how to disable the WebKit preference, but
  // it's theoretically possible some users have it off.
  bool js_content_enabled =
      js_setting == CONTENT_SETTING_DEFAULT ||
      js_setting == CONTENT_SETTING_ALLOW;
  bool js_webkit_enabled = profile()->GetPrefs()->GetBoolean(
      prefs::kWebKitJavascriptEnabled);
  return js_content_enabled && js_webkit_enabled;
}

bool InstantController::InStartup() const {
  // TODO(shishir): This is not completely reliable. Find a better way to detect
  // startup time.
  return !browser_->GetActiveWebContents();
}

InstantService* InstantController::GetInstantService() const {
  return InstantServiceFactory::GetForProfile(profile());
}
