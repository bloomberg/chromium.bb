// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_controller.h"

#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/instant/instant_controller_delegate.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/codec/png_codec.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

namespace {

enum PreviewUsageType {
  PREVIEW_CREATED = 0,
  PREVIEW_DELETED,
  PREVIEW_LOADED,
  PREVIEW_SHOWED,
  PREVIEW_COMMITTED,
  PREVIEW_NUM_TYPES,
};

// An artificial delay (in milliseconds) we introduce before telling the Instant
// page about the new omnibox bounds, in cases where the bounds shrink. This is
// to avoid the page jumping up/down very fast in response to bounds changes.
const int kUpdateBoundsDelayMS = 1000;

// The maximum number of times we'll load a non-Instant-supporting search engine
// before we give up and blacklist it for the rest of the browsing session.
const int kMaxInstantSupportFailures = 10;

std::string ModeToString(InstantController::Mode mode) {
  switch (mode) {
    case InstantController::INSTANT:  return "_Instant";
    case InstantController::SUGGEST:  return "_Suggest";
    case InstantController::HIDDEN:   return "_Hidden";
    case InstantController::SILENT:   return "_Silent";
  }

  NOTREACHED();
  return std::string();
}

void AddPreviewUsageForHistogram(InstantController::Mode mode,
                                 PreviewUsageType usage) {
  DCHECK(0 <= usage && usage < PREVIEW_NUM_TYPES) << usage;
  base::Histogram* histogram = base::LinearHistogram::FactoryGet(
      "Instant.Previews" + ModeToString(mode), 1, PREVIEW_NUM_TYPES,
      PREVIEW_NUM_TYPES + 1, base::Histogram::kUmaTargetedHistogramFlag);
  histogram->Add(usage);
}

void AddSessionStorageHistogram(InstantController::Mode mode,
                                const TabContents* tab1,
                                const TabContents* tab2) {
  base::Histogram* histogram = base::BooleanHistogram::FactoryGet(
      "Instant.SessionStorageNamespace" + ModeToString(mode),
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
         it1 != session_storage_map1.end() &&
             it2 != session_storage_map2.end();
         ++it1, ++it2) {
      if (it1->first != it2->first || it1->second != it2->second) {
        is_session_storage_the_same = false;
        break;
      }
    }
  }
  histogram->AddBoolean(is_session_storage_the_same);
}

}  // namespace

InstantController::InstantController(InstantControllerDelegate* delegate,
                                     Mode mode)
    : delegate_(delegate),
      mode_(mode),
      last_active_tab_(NULL),
      last_verbatim_(false),
      last_transition_type_(content::PAGE_TRANSITION_LINK),
      is_showing_(false),
      loader_processed_last_update_(false) {
}

InstantController::~InstantController() {
  if (GetPreviewContents())
    AddPreviewUsageForHistogram(mode_, PREVIEW_DELETED);
}

// static
void InstantController::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kInstantConfirmDialogShown, false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kInstantEnabled, false,
                             PrefService::SYNCABLE_PREF);

  // TODO(jamescook): Move this to search controller.
  prefs->RegisterDoublePref(prefs::kInstantAnimationScaleFactor,
                            1.0,
                            PrefService::UNSYNCABLE_PREF);
}

// static
bool InstantController::IsEnabled(Profile* profile) {
  const PrefService* prefs = profile ? profile->GetPrefs() : NULL;
  return prefs && prefs->GetBoolean(prefs::kInstantEnabled);
}

bool InstantController::Update(const AutocompleteMatch& match,
                               const string16& user_text,
                               bool verbatim,
                               string16* suggested_text,
                               InstantCompleteBehavior* complete_behavior) {
  const TabContents* active_tab = delegate_->GetActiveTabContents();

  // We could get here with no active tab if the Browser is closing.
  if (!active_tab) {
    Hide();
    return false;
  }

  std::string instant_url;
  Profile* profile = active_tab->profile();

  // If the match's TemplateURL isn't valid, it is likely not a query.
  if (!GetInstantURL(match.GetTemplateURL(profile), &instant_url)) {
    Hide();
    return false;
  }

  string16 full_text = user_text + *suggested_text;

  if (full_text.empty()) {
    Hide();
    return false;
  }

  // The presence of any suggested_text implies verbatim.
  DCHECK(suggested_text->empty() || verbatim)
      << user_text << "|" << *suggested_text;

  ResetLoader(instant_url, active_tab);
  last_active_tab_ = active_tab;

  // Track the non-Instant search URL for this query.
  url_for_history_ = match.destination_url;
  last_transition_type_ = match.transition;

  last_user_text_ = user_text;

  // Don't send an update to the loader if the query text hasn't changed.
  if (full_text == last_full_text_ && verbatim == last_verbatim_) {
    // Since we are updating |suggested_text|, shouldn't we also update
    // |last_full_text_|? No. There's no guarantee that our suggestion will
    // actually be inline autocompleted. For example, it may get trumped by
    // a history suggestion. If our suggestion does make it, the omnibox will
    // call Update() again, at which time we'll update |last_full_text_|.
    *suggested_text = last_suggestion_.text;
    *complete_behavior = last_suggestion_.behavior;

    // We need to call Show() here because of this:
    // 1. User has typed a query (say Q). Instant overlay is showing results.
    // 2. User arrows-down to a URL entry or erases all omnibox text. Both of
    //    these cause the overlay to Hide().
    // 3. User arrows-up to Q or types Q again. The last text we processed is
    //    still Q, so we don't Update() the loader, but we do need to Show().
    if (loader_processed_last_update_ && mode_ == INSTANT)
      Show();
    return true;
  }

  last_full_text_ = full_text;
  last_verbatim_ = verbatim;
  loader_processed_last_update_ = false;

  // Reset the last suggestion, as it's no longer valid.
  suggested_text->clear();
  last_suggestion_ = InstantSuggestion();
  *complete_behavior = INSTANT_COMPLETE_NOW;

  if (mode_ != SILENT) {
    loader_->Update(last_full_text_, last_verbatim_);

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
        content::Source<InstantController>(this),
        content::NotificationService::NoDetails());
  }

  return true;
}

// TODO(tonyg): This method only fires when the omnibox bounds change. It also
// needs to fire when the preview bounds change (e.g.: open/close info bar).
void InstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (omnibox_bounds_ == bounds || mode_ != INSTANT)
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

TabContents* InstantController::GetPreviewContents() const {
  return loader_.get() ? loader_->preview_contents() : NULL;
}

void InstantController::Hide() {
  last_active_tab_ = NULL;
  if (is_showing_) {
    is_showing_ = false;
    delegate_->HideInstant();
  }
}

bool InstantController::IsCurrent() const {
  DCHECK(IsOutOfDate() || GetPreviewContents());
  return !IsOutOfDate() && GetPreviewContents() && loader_->supports_instant();
}

TabContents* InstantController::CommitCurrentPreview(InstantCommitType type) {
  const TabContents* active_tab = delegate_->GetActiveTabContents();
  TabContents* preview = ReleasePreviewContents(type);
  AddSessionStorageHistogram(mode_, active_tab, preview);
  preview->web_contents()->GetController().CopyStateFromAndPrune(
      &active_tab->web_contents()->GetController());
  delegate_->CommitInstant(preview);
  return preview;
}

TabContents* InstantController::ReleasePreviewContents(InstantCommitType type) {
  TabContents* preview = loader_->ReleasePreviewContents(type, last_full_text_);

  // If the preview page has navigated since the last Update(), we need to add
  // the navigation to history ourselves. Else, the page will navigate after
  // commit, and it will be added to history in the usual manner.
  scoped_refptr<history::HistoryAddPageArgs> last_navigation =
      loader_->last_navigation();
  if (last_navigation != NULL) {
    content::NavigationEntry* entry =
        preview->web_contents()->GetController().GetActiveEntry();
    DCHECK_EQ(last_navigation->url, entry->GetURL());

    // Add the page to history.
    preview->history_tab_helper()->UpdateHistoryForNavigation(last_navigation);

    // Update the page title.
    preview->history_tab_helper()->UpdateHistoryPageTitle(*entry);

    // Update the favicon.
    FaviconService* favicon_service =
        preview->profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);
    if (favicon_service && entry->GetFavicon().valid &&
        entry->GetFavicon().image.IsEmpty()) {
      std::vector<unsigned char> image_data;
      // TODO: Add all variants once the history service supports it.
      gfx::PNGCodec::EncodeBGRASkBitmap(
          entry->GetFavicon().image.AsBitmap(), false, &image_data);
      favicon_service->SetFavicon(entry->GetURL(),
                                  entry->GetFavicon().url,
                                  image_data,
                                  history::FAVICON);
    }
  }

  // Add a fake history entry with a non-Instant search URL, so that search
  // terms extraction (for autocomplete history matches) works.
  HistoryService* history = HistoryServiceFactory::GetForProfile(
      preview->profile(), Profile::EXPLICIT_ACCESS);
  if (history) {
    history->AddPage(url_for_history_, NULL, 0, GURL(), last_transition_type_,
                     history::RedirectList(), history::SOURCE_BROWSED, false);
  }

  AddPreviewUsageForHistogram(mode_, PREVIEW_COMMITTED);

  // We may have gotten here from CommitInstant(), which means the loader may
  // still be on the stack. So, schedule a destruction for later.
  MessageLoop::current()->DeleteSoon(FROM_HERE, loader_.release());

  // This call is here to hide the preview and reset view state. It won't
  // actually delete |loader_| because it was just released to DeleteSoon().
  DeleteLoader();

  return preview;
}

// TODO(sreeram): Since we never delete the loader except when committing
// Instant, the loader may have a very stale page. Reload it when stale.
void InstantController::OnAutocompleteLostFocus(
    gfx::NativeView view_gaining_focus) {
  DCHECK(!is_showing_ || GetPreviewContents());

  // If the preview is not showing, nothing to do.
  if (!is_showing_ || !GetPreviewContents())
    return;

#if defined(OS_MACOSX)
  if (!loader_->IsPointerDownFromActivate())
    Hide();
#else
  content::RenderWidgetHostView* rwhv =
      GetPreviewContents()->web_contents()->GetRenderWidgetHostView();
  if (!view_gaining_focus || !rwhv) {
    Hide();
    return;
  }

#if defined(TOOLKIT_VIEWS)
  // For views the top level widget is always focused. If the focus change
  // originated in views determine the child Widget from the view that is being
  // focused.
  views::Widget* widget =
      views::Widget::GetWidgetForNativeView(view_gaining_focus);
  if (widget) {
    views::FocusManager* focus_manager = widget->GetFocusManager();
    if (focus_manager && focus_manager->is_changing_focus() &&
        focus_manager->GetFocusedView() &&
        focus_manager->GetFocusedView()->GetWidget()) {
      view_gaining_focus =
          focus_manager->GetFocusedView()->GetWidget()->GetNativeView();
    }
  }
#endif

  gfx::NativeView tab_view =
      GetPreviewContents()->web_contents()->GetNativeView();

  // Focus is going to the renderer.
  if (rwhv->GetNativeView() == view_gaining_focus ||
      tab_view == view_gaining_focus) {

    // If the mouse is not down, focus is not going to the renderer. Someone
    // else moved focus and we shouldn't commit.
    if (!loader_->IsPointerDownFromActivate())
      Hide();

    return;
  }

  // Walk up the view hierarchy. If the view gaining focus is a subview of the
  // WebContents view (such as a windowed plugin or http auth dialog), we want
  // to keep the preview contents. Otherwise, focus has gone somewhere else,
  // such as the JS inspector, and we want to cancel the preview.
  gfx::NativeView view_gaining_focus_ancestor = view_gaining_focus;
  while (view_gaining_focus_ancestor &&
         view_gaining_focus_ancestor != tab_view) {
    view_gaining_focus_ancestor =
        platform_util::GetParent(view_gaining_focus_ancestor);
  }

  if (view_gaining_focus_ancestor) {
    CommitCurrentPreview(INSTANT_COMMIT_FOCUS_LOST);
    return;
  }

  Hide();
#endif
}

void InstantController::OnAutocompleteGotFocus() {
  const TabContents* active_tab = delegate_->GetActiveTabContents();

  // We could get here with no active tab if the Browser is closing.
  if (!active_tab)
    return;

  // Since we don't have any autocomplete match to work with, we'll just use
  // the default search provider's Instant URL.
  const TemplateURL* template_url =
      TemplateURLServiceFactory::GetForProfile(active_tab->profile())->
                                 GetDefaultSearchProvider();

  std::string instant_url;
  if (!GetInstantURL(template_url, &instant_url))
    return;

  ResetLoader(instant_url, active_tab);
}

bool InstantController::commit_on_pointer_release() const {
  return GetPreviewContents() && loader_->IsPointerDownFromActivate();
}

void InstantController::SetSuggestions(
    InstantLoader* loader,
    const std::vector<InstantSuggestion>& suggestions) {
  DCHECK_EQ(loader_.get(), loader);
  if (loader_ != loader || IsOutOfDate() || mode_ == SILENT || mode_ == HIDDEN)
    return;

  loader_processed_last_update_ = true;

  InstantSuggestion suggestion;
  if (!suggestions.empty())
    suggestion = suggestions[0];

  string16 suggestion_lower = base::i18n::ToLower(suggestion.text);
  string16 user_text_lower = base::i18n::ToLower(last_user_text_);
  if (user_text_lower.size() >= suggestion_lower.size() ||
      suggestion_lower.compare(0, user_text_lower.size(), user_text_lower)) {
    suggestion.text.clear();
  } else {
    suggestion.text.erase(0, last_user_text_.size());
  }

  last_suggestion_ = suggestion;
  if (!last_verbatim_)
    delegate_->SetSuggestedText(suggestion.text, suggestion.behavior);

  if (mode_ != SUGGEST)
    Show();
}

void InstantController::CommitInstantLoader(InstantLoader* loader) {
  DCHECK_EQ(loader_.get(), loader);
  DCHECK(is_showing_ && !IsOutOfDate()) << is_showing_;
  if (loader_ != loader || !is_showing_ || IsOutOfDate())
    return;

  CommitCurrentPreview(INSTANT_COMMIT_FOCUS_LOST);
}

void InstantController::InstantLoaderPreviewLoaded(InstantLoader* loader) {
  DCHECK_EQ(loader_.get(), loader);
  AddPreviewUsageForHistogram(mode_, PREVIEW_LOADED);
}

void InstantController::InstantSupportDetermined(InstantLoader* loader,
                                                 bool supports_instant) {
  DCHECK_EQ(loader_.get(), loader);
  if (supports_instant) {
    blacklisted_urls_.erase(loader->instant_url());
  } else {
    ++blacklisted_urls_[loader->instant_url()];
    if (loader_ == loader) {
      if (GetPreviewContents())
        AddPreviewUsageForHistogram(mode_, PREVIEW_DELETED);

      // Because of the state of the stack, we can't destroy the loader now.
      MessageLoop::current()->DeleteSoon(FROM_HERE, loader_.release());
      DeleteLoader();
    }
  }

  content::Details<const bool> details(&supports_instant);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      content::NotificationService::AllSources(),
      details);
}

void InstantController::SwappedTabContents(InstantLoader* loader) {
  DCHECK_EQ(loader_.get(), loader);
  if (loader_ == loader && is_showing_)
    delegate_->ShowInstant();
}

void InstantController::InstantLoaderContentsFocused(InstantLoader* loader) {
  DCHECK_EQ(loader_.get(), loader);
  DCHECK(is_showing_ && !IsOutOfDate()) << is_showing_;
#if defined(USE_AURA)
  // On aura the omnibox only receives a focus lost if we initiate the focus
  // change. This does that.
  if (is_showing_ && !IsOutOfDate())
    delegate_->InstantPreviewFocused();
#endif
}

void InstantController::ResetLoader(const std::string& instant_url,
                                    const TabContents* active_tab) {
  if (GetPreviewContents() && loader_->instant_url() != instant_url)
    DeleteLoader();

  if (!GetPreviewContents()) {
    loader_.reset(new InstantLoader(this, instant_url, active_tab));
    loader_->Init();
    AddPreviewUsageForHistogram(mode_, PREVIEW_CREATED);
  }
}

void InstantController::DeleteLoader() {
  Hide();
  last_full_text_.clear();
  last_user_text_.clear();
  last_verbatim_ = false;
  last_suggestion_ = InstantSuggestion();
  last_transition_type_ = content::PAGE_TRANSITION_LINK;
  last_omnibox_bounds_ = gfx::Rect();
  url_for_history_ = GURL();
  if (GetPreviewContents())
    AddPreviewUsageForHistogram(mode_, PREVIEW_DELETED);
  loader_.reset();
}

void InstantController::Show() {
  if (!is_showing_) {
    is_showing_ = true;
    delegate_->ShowInstant();
    AddPreviewUsageForHistogram(mode_, PREVIEW_SHOWED);
  }
}

void InstantController::SendBoundsToPage() {
  if (last_omnibox_bounds_ == omnibox_bounds_ || IsOutOfDate() ||
      !GetPreviewContents() || loader_->IsPointerDownFromActivate()) {
    return;
  }

  last_omnibox_bounds_ = omnibox_bounds_;
  gfx::Rect preview_bounds = delegate_->GetInstantBounds();
  gfx::Rect intersection = omnibox_bounds_.Intersect(preview_bounds);

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
    return true;
  }

  if (!template_url)
    return false;

  const TemplateURLRef& instant_url_ref = template_url->instant_url_ref();
  if (!instant_url_ref.IsValid() || !instant_url_ref.SupportsReplacement())
    return false;

  *instant_url = instant_url_ref.ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(string16()));

  std::map<std::string, int>::const_iterator iter =
      blacklisted_urls_.find(*instant_url);
  if (iter != blacklisted_urls_.end() &&
      iter->second > kMaxInstantSupportFailures) {
    instant_url->clear();
    return false;
  }

  return true;
}

bool InstantController::IsOutOfDate() const {
  return !last_active_tab_ ||
         last_active_tab_ != delegate_->GetActiveTabContents();
}
