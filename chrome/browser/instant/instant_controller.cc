// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_controller.h"

#include "build/build_config.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/instant/instant_delegate.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/instant/instant_loader_manager.h"
#include "chrome/browser/instant/promo_counter.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"

// Number of ms to delay between loading urls.
static const int kUpdateDelayMS = 200;

// static
InstantController::HostBlacklist* InstantController::host_blacklist_ = NULL;

InstantController::InstantController(Profile* profile,
                                     InstantDelegate* delegate)
    : delegate_(delegate),
      tab_contents_(NULL),
      is_active_(false),
      displayable_loader_(NULL),
      commit_on_mouse_up_(false),
      last_transition_type_(PageTransition::LINK),
      ALLOW_THIS_IN_INITIALIZER_LIST(destroy_factory_(this)) {
  PrefService* service = profile->GetPrefs();
  if (service) {
    // kInstantWasEnabledOnce was added after instant, set it now to make sure
    // it is correctly set.
    service->SetBoolean(prefs::kInstantEnabledOnce, true);
  }
}

InstantController::~InstantController() {
}

// static
void InstantController::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kInstantConfirmDialogShown, false);
  prefs->RegisterBooleanPref(prefs::kInstantEnabled, false);
  prefs->RegisterBooleanPref(prefs::kInstantEnabledOnce, false);
  prefs->RegisterInt64Pref(prefs::kInstantEnabledTime, false);
  PromoCounter::RegisterUserPrefs(prefs, prefs::kInstantPromo);
}

// static
void InstantController::RecordMetrics(Profile* profile) {
  if (!IsEnabled(profile))
    return;

  PrefService* service = profile->GetPrefs();
  if (service) {
    int64 enable_time = service->GetInt64(prefs::kInstantEnabledTime);
    if (!enable_time) {
      service->SetInt64(prefs::kInstantEnabledTime,
                        base::Time::Now().ToInternalValue());
    } else {
      base::TimeDelta delta =
          base::Time::Now() - base::Time::FromInternalValue(enable_time);
      // Histogram from 1 hour to 30 days.
      UMA_HISTOGRAM_CUSTOM_COUNTS("Instant.EnabledTime.Predictive",
                                  delta.InHours(), 1, 30 * 24, 50);
    }
  }
}

// static
bool InstantController::IsEnabled(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  return prefs->GetBoolean(prefs::kInstantEnabled);
}

// static
void InstantController::Enable(Profile* profile) {
  PromoCounter* promo_counter = profile->GetInstantPromoCounter();
  if (promo_counter)
    promo_counter->Hide();

  PrefService* service = profile->GetPrefs();
  if (!service)
    return;

  service->SetBoolean(prefs::kInstantEnabled, true);
  service->SetBoolean(prefs::kInstantConfirmDialogShown, true);
  service->SetInt64(prefs::kInstantEnabledTime,
                    base::Time::Now().ToInternalValue());
  service->SetBoolean(prefs::kInstantEnabledOnce, true);
}

// static
void InstantController::Disable(Profile* profile) {
  PrefService* service = profile->GetPrefs();
  if (!service || !IsEnabled(profile))
    return;

  int64 enable_time = service->GetInt64(prefs::kInstantEnabledTime);
  if (enable_time) {
    base::TimeDelta delta =
        base::Time::Now() - base::Time::FromInternalValue(enable_time);
    // Histogram from 1 minute to 10 days.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Instant.TimeToDisable.Predictive",
                                delta.InMinutes(), 1, 60 * 24 * 10, 50);
  }

  service->SetBoolean(prefs::kInstantEnabled, false);
}

// static
bool InstantController::CommitIfCurrent(InstantController* controller) {
  if (controller && controller->IsCurrent()) {
    controller->CommitCurrentPreview(INSTANT_COMMIT_PRESSED_ENTER);
    return true;
  }
  return false;
}

void InstantController::Update(TabContentsWrapper* tab_contents,
                               const AutocompleteMatch& match,
                               const string16& user_text,
                               bool verbatim,
                               string16* suggested_text) {
  suggested_text->clear();

  if (tab_contents != tab_contents_)
    DestroyPreviewContents();

  const GURL& url = match.destination_url;
  tab_contents_ = tab_contents;
  commit_on_mouse_up_ = false;
  last_transition_type_ = match.transition;
  const TemplateURL* template_url = NULL;

  if (url.is_empty() || !url.is_valid()) {
    // Assume we were invoked with GURL() and should destroy all.
    DestroyPreviewContents();
    return;
  }

  if (!ShouldShowPreviewFor(match, &template_url)) {
    DestroyPreviewContentsAndLeaveActive();
    return;
  }

  if (!loader_manager_.get())
    loader_manager_.reset(new InstantLoaderManager(this));

  if (!is_active_) {
    is_active_ = true;
    delegate_->PrepareForInstant();
  }

  TemplateURLID template_url_id = template_url ? template_url->id() : 0;
  // Verbatim only makes sense if the search engines supports instant.
  bool real_verbatim = template_url_id ? verbatim : false;

  if (ShouldUpdateNow(template_url_id, match.destination_url)) {
    UpdateLoader(template_url, match.destination_url, match.transition,
                 user_text, real_verbatim, suggested_text);
  } else {
    ScheduleUpdate(match.destination_url);
  }

  NotificationService::current()->Notify(
      NotificationType::INSTANT_CONTROLLER_UPDATED,
      Source<InstantController>(this),
      NotificationService::NoDetails());
}

void InstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (omnibox_bounds_ == bounds)
    return;

  // Always track the omnibox bounds. That way if Update is later invoked the
  // bounds are in sync.
  omnibox_bounds_ = bounds;
  if (loader_manager_.get()) {
    if (loader_manager_->current_loader())
      loader_manager_->current_loader()->SetOmniboxBounds(bounds);
    if (loader_manager_->pending_loader())
      loader_manager_->pending_loader()->SetOmniboxBounds(bounds);
  }
}

void InstantController::DestroyPreviewContents() {
  if (!loader_manager_.get()) {
    // We're not showing anything, nothing to do.
    return;
  }

  // ReleasePreviewContents sets is_active_ to false, but we need to set it
  // before notifying the delegate, otherwise if the delegate asks for the state
  // we'll still be active.
  is_active_ = false;
  delegate_->HideInstant();
  delete ReleasePreviewContents(INSTANT_COMMIT_DESTROY);
}

void InstantController::DestroyPreviewContentsAndLeaveActive() {
  commit_on_mouse_up_ = false;
  if (displayable_loader_) {
    displayable_loader_ = NULL;
    delegate_->HideInstant();
  }

  // TODO(sky): this shouldn't nuke the loader. It should just nuke non-instant
  // loaders and hide instant loaders.
  loader_manager_.reset(new InstantLoaderManager(this));
  update_timer_.Stop();
}

bool InstantController::IsCurrent() {
  return loader_manager_.get() && loader_manager_->active_loader() &&
      loader_manager_->active_loader()->ready() && !update_timer_.IsRunning();
}

void InstantController::CommitCurrentPreview(InstantCommitType type) {
  DCHECK(loader_manager_.get());
  DCHECK(loader_manager_->current_loader());
  TabContentsWrapper* tab = ReleasePreviewContents(type);
  delegate_->CommitInstant(tab);
  CompleteRelease(tab->tab_contents());
}

void InstantController::SetCommitOnMouseUp() {
  commit_on_mouse_up_ = true;
}

bool InstantController::IsMouseDownFromActivate() {
  DCHECK(loader_manager_.get());
  DCHECK(loader_manager_->current_loader());
  return loader_manager_->current_loader()->IsMouseDownFromActivate();
}

void InstantController::OnAutocompleteLostFocus(
    gfx::NativeView view_gaining_focus) {
  if (!is_active() || !GetPreviewContents()) {
    DestroyPreviewContents();
    return;
  }

  RenderWidgetHostView* rwhv =
      GetPreviewContents()->tab_contents()->GetRenderWidgetHostView();
  if (!view_gaining_focus || !rwhv) {
    DestroyPreviewContents();
    return;
  }

  gfx::NativeView tab_view =
      GetPreviewContents()->tab_contents()->GetNativeView();
  // Focus is going to the renderer.
  if (rwhv->GetNativeView() == view_gaining_focus ||
      tab_view == view_gaining_focus) {
    if (!IsMouseDownFromActivate()) {
      // If the mouse is not down, focus is not going to the renderer. Someone
      // else moved focus and we shouldn't commit.
      DestroyPreviewContents();
      return;
    }

    if (IsShowingInstant()) {
      // We're showing instant results. As instant results may shift when
      // committing we commit on the mouse up. This way a slow click still
      // works fine.
      SetCommitOnMouseUp();
      return;
    }

    CommitCurrentPreview(INSTANT_COMMIT_FOCUS_LOST);
    return;
  }

  // Walk up the view hierarchy. If the view gaining focus is a subview of the
  // TabContents view (such as a windowed plugin or http auth dialog), we want
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

  DestroyPreviewContents();
}

TabContentsWrapper* InstantController::ReleasePreviewContents(
    InstantCommitType type) {
  if (!loader_manager_.get())
    return NULL;

  // Loader may be null if the url blacklisted instant.
  scoped_ptr<InstantLoader> loader;
  if (loader_manager_->current_loader())
    loader.reset(loader_manager_->ReleaseCurrentLoader());
  TabContentsWrapper* tab = loader.get() ?
      loader->ReleasePreviewContents(type) : NULL;

  ClearBlacklist();
  is_active_ = false;
  displayable_loader_ = NULL;
  commit_on_mouse_up_ = false;
  omnibox_bounds_ = gfx::Rect();
  loader_manager_.reset();
  update_timer_.Stop();
  return tab;
}

void InstantController::CompleteRelease(TabContents* tab) {
  tab->SetAllContentsBlocked(false);
}

TabContentsWrapper* InstantController::GetPreviewContents() {
  return loader_manager_.get() && loader_manager_->current_loader() ?
      loader_manager_->current_loader()->preview_contents() : NULL;
}

bool InstantController::IsShowingInstant() {
  return loader_manager_.get() && loader_manager_->current_loader() &&
      loader_manager_->current_loader()->is_showing_instant();
}

bool InstantController::MightSupportInstant() {
  return loader_manager_.get() && loader_manager_->active_loader() &&
      loader_manager_->active_loader()->is_showing_instant();
}

GURL InstantController::GetCurrentURL() {
  return loader_manager_.get() && loader_manager_->active_loader() ?
      loader_manager_->active_loader()->url() : GURL();
}

void InstantController::ShowInstantLoader(InstantLoader* loader) {
  DCHECK(loader_manager_.get());
  scoped_ptr<InstantLoader> old_loader;
  if (loader == loader_manager_->pending_loader()) {
    loader_manager_->MakePendingCurrent(&old_loader);
  } else if (loader != loader_manager_->current_loader()) {
    // Notification from a loader that is no longer the current (either we have
    // a pending, or its an instant loader). Ignore it.
    return;
  }

  UpdateDisplayableLoader();

  NotificationService::current()->Notify(
      NotificationType::INSTANT_CONTROLLER_SHOWN,
      Source<InstantController>(this),
      NotificationService::NoDetails());
}

void InstantController::SetSuggestedTextFor(InstantLoader* loader,
                                            const string16& text) {
  if (loader_manager_->current_loader() == loader)
    delegate_->SetSuggestedText(text);
}

gfx::Rect InstantController::GetInstantBounds() {
  return delegate_->GetInstantBounds();
}

bool InstantController::ShouldCommitInstantOnMouseUp() {
  return commit_on_mouse_up_;
}

void InstantController::CommitInstantLoader(InstantLoader* loader) {
  if (loader_manager_.get() && loader_manager_->current_loader() == loader) {
    CommitCurrentPreview(INSTANT_COMMIT_FOCUS_LOST);
  } else {
    // This can happen if the mouse was down, we swapped out the preview and
    // the mouse was released. Generally this shouldn't happen, but if it does
    // revert.
    DestroyPreviewContents();
  }
}

void InstantController::InstantLoaderDoesntSupportInstant(
    InstantLoader* loader) {
  DCHECK(!loader->ready());  // We better not be showing this loader.
  DCHECK(loader->template_url_id());

  VLOG(1) << "provider does not support instant";

  // Don't attempt to use instant for this search engine again.
  BlacklistFromInstant(loader->template_url_id());

  if (loader_manager_->active_loader() == loader) {
    // The loader is active, hide all.
    DestroyPreviewContentsAndLeaveActive();
  } else {
    scoped_ptr<InstantLoader> owned_loader(
        loader_manager_->ReleaseLoader(loader));
    UpdateDisplayableLoader();
  }
}

void InstantController::AddToBlacklist(InstantLoader* loader, const GURL& url) {
  std::string host = url.host();
  if (host.empty())
    return;

  if (!host_blacklist_)
    host_blacklist_ = new HostBlacklist;
  host_blacklist_->insert(host);

  if (!loader_manager_.get())
    return;

  // Because of the state of the stack we can't destroy the loader now.
  ScheduleDestroy(loader);

  loader_manager_->ReleaseLoader(loader);

  UpdateDisplayableLoader();
}

void InstantController::UpdateDisplayableLoader() {
  InstantLoader* loader = NULL;
  // As soon as the pending loader is displayable it becomes the current loader,
  // so we need only concern ourselves with the current loader here.
  if (loader_manager_.get() && loader_manager_->current_loader() &&
      loader_manager_->current_loader()->ready()) {
    loader = loader_manager_->current_loader();
  }
  if (loader == displayable_loader_)
    return;

  displayable_loader_ = loader;

  if (!displayable_loader_) {
    delegate_->HideInstant();
  } else {
    delegate_->ShowInstant(displayable_loader_->preview_contents());
    NotificationService::current()->Notify(
        NotificationType::INSTANT_CONTROLLER_SHOWN,
        Source<InstantController>(this),
        NotificationService::NoDetails());
  }
}

TabContentsWrapper* InstantController::GetPendingPreviewContents() {
  return loader_manager_.get() && loader_manager_->pending_loader() ?
      loader_manager_->pending_loader()->preview_contents() : NULL;
}

bool InstantController::ShouldUpdateNow(TemplateURLID instant_id,
                                        const GURL& url) {
  DCHECK(loader_manager_.get());

  if (instant_id) {
    // Update sites that support instant immediately, they can do their own
    // throttling.
    return true;
  }

  if (url.SchemeIsFile())
    return true;  // File urls should load quickly, so don't delay loading them.

  if (loader_manager_->WillUpateChangeActiveLoader(instant_id)) {
    // If Update would change loaders, update now. This indicates transitioning
    // from an instant to non-instant loader.
    return true;
  }

  InstantLoader* active_loader = loader_manager_->active_loader();
  // WillUpateChangeActiveLoader should return true if no active loader, so
  // we know there will be an active loader if we get here.
  DCHECK(active_loader);
  // Immediately update if the url is the same (which should result in nothing
  // happening) or the hosts differ, otherwise we'll delay the update.
  return (active_loader->url() == url) ||
      (active_loader->url().host() != url.host());
}

void InstantController::ScheduleUpdate(const GURL& url) {
  scheduled_url_ = url;

  update_timer_.Stop();
  update_timer_.Start(base::TimeDelta::FromMilliseconds(kUpdateDelayMS),
                      this, &InstantController::ProcessScheduledUpdate);
}

void InstantController::ProcessScheduledUpdate() {
  DCHECK(loader_manager_.get());

  // We only delay loading of sites that don't support instant, so we can ignore
  // suggested_text here.
  string16 suggested_text;
  UpdateLoader(NULL, scheduled_url_, last_transition_type_, string16(), false,
               &suggested_text);
}

void InstantController::UpdateLoader(const TemplateURL* template_url,
                                     const GURL& url,
                                     PageTransition::Type transition_type,
                                     const string16& user_text,
                                     bool verbatim,
                                     string16* suggested_text) {
  update_timer_.Stop();

  scoped_ptr<InstantLoader> owned_loader;
  TemplateURLID template_url_id = template_url ? template_url->id() : 0;
  InstantLoader* new_loader =
      loader_manager_->UpdateLoader(template_url_id, &owned_loader);

  new_loader->SetOmniboxBounds(omnibox_bounds_);
  new_loader->Update(tab_contents_, template_url, url, transition_type,
                     user_text, verbatim, suggested_text);
  UpdateDisplayableLoader();
}

bool InstantController::ShouldShowPreviewFor(const AutocompleteMatch& match,
                                             const TemplateURL** template_url) {
  const TemplateURL* t_url = GetTemplateURL(match);
  if (t_url) {
    if (!t_url->id() ||
        !t_url->instant_url() ||
        IsBlacklistedFromInstant(t_url->id()) ||
        !t_url->instant_url()->SupportsReplacement()) {
      // To avoid extra load on other search engines we only enable previews if
      // they support the instant API.
      return false;
    }
  }
  *template_url = t_url;

  if (match.destination_url.SchemeIs(chrome::kJavaScriptScheme))
    return false;

  // Extension keywords don't have a real destionation URL.
  if (match.template_url && match.template_url->IsExtensionKeyword())
    return false;

  // Was the host blacklisted?
  if (host_blacklist_ && host_blacklist_->count(match.destination_url.host()))
    return false;

  return true;
}

void InstantController::BlacklistFromInstant(TemplateURLID id) {
  blacklisted_ids_.insert(id);
}

bool InstantController::IsBlacklistedFromInstant(TemplateURLID id) {
  return blacklisted_ids_.count(id) > 0;
}

void InstantController::ClearBlacklist() {
  blacklisted_ids_.clear();
}

void InstantController::ScheduleDestroy(InstantLoader* loader) {
  loaders_to_destroy_.push_back(loader);
  if (destroy_factory_.empty()) {
    MessageLoop::current()->PostTask(
        FROM_HERE, destroy_factory_.NewRunnableMethod(
            &InstantController::DestroyLoaders));
  }
}

void InstantController::DestroyLoaders() {
  loaders_to_destroy_.reset();
}

const TemplateURL* InstantController::GetTemplateURL(
    const AutocompleteMatch& match) {
  const TemplateURL* template_url = match.template_url;
  if (match.type == AutocompleteMatch::SEARCH_WHAT_YOU_TYPED ||
      match.type == AutocompleteMatch::SEARCH_HISTORY ||
      match.type == AutocompleteMatch::SEARCH_SUGGEST) {
    TemplateURLModel* model = tab_contents_->profile()->GetTemplateURLModel();
    template_url = model ? model->GetDefaultSearchProvider() : NULL;
  }
  return template_url;
}
