// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_controller.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/instant/instant_delegate.h"
#include "chrome/browser/instant/instant_field_trial.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/instant/promo_counter.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"

#if defined(TOOLKIT_VIEWS)
#include "views/focus/focus_manager.h"
#include "views/view.h"
#include "views/widget/widget.h"
#endif

InstantController::InstantController(Profile* profile,
                                     InstantDelegate* delegate)
    : delegate_(delegate),
      tab_contents_(NULL),
      is_active_(false),
      is_displayable_(false),
      commit_on_mouse_up_(false),
      last_transition_type_(content::PAGE_TRANSITION_LINK),
      ALLOW_THIS_IN_INITIALIZER_LIST(destroy_factory_(this)) {
  PrefService* service = profile->GetPrefs();
  if (service &&
      InstantFieldTrial::GetGroup(profile) == InstantFieldTrial::INACTIVE) {
    // kInstantEnabledOnce was added after instant, set it now to make sure it
    // is correctly set.
    service->SetBoolean(prefs::kInstantEnabledOnce, true);
  }
}

InstantController::~InstantController() {
}

// static
void InstantController::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kInstantConfirmDialogShown,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kInstantEnabled,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kInstantEnabledOnce,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterInt64Pref(prefs::kInstantEnabledTime,
                           false,
                           PrefService::SYNCABLE_PREF);
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
  return prefs->GetBoolean(prefs::kInstantEnabled) ||
         InstantFieldTrial::IsExperimentGroup(profile);
}

// static
void InstantController::Enable(Profile* profile) {
  PromoCounter* promo_counter = profile->GetInstantPromoCounter();
  if (promo_counter)
    promo_counter->Hide();

  PrefService* service = profile->GetPrefs();
  if (!service)
    return;

  service->SetBoolean(prefs::kInstantEnabledOnce, true);
  service->SetBoolean(prefs::kInstantEnabled, true);
  service->SetBoolean(prefs::kInstantConfirmDialogShown, true);
  service->SetInt64(prefs::kInstantEnabledTime,
                    base::Time::Now().ToInternalValue());
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

  UMA_HISTOGRAM_COUNTS(
      "Instant.OptOut" + InstantFieldTrial::GetGroupName(profile), 1);

  service->SetBoolean(prefs::kInstantEnabledOnce, true);
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

bool InstantController::Update(TabContentsWrapper* tab_contents,
                               const AutocompleteMatch& match,
                               const string16& user_text,
                               bool verbatim,
                               string16* suggested_text) {
  suggested_text->clear();

  tab_contents_ = tab_contents;
  commit_on_mouse_up_ = false;
  last_transition_type_ = match.transition;
  if (!ShouldUseInstant(match)) {
    DestroyPreviewContentsAndLeaveActive();
    return false;
  }

  const TemplateURL* template_url = match.template_url;
  DCHECK(template_url);  // ShouldUseInstant returns false if no turl.
  if (!loader_.get())
    loader_.reset(new InstantLoader(this, template_url->id()));

  if (!is_active_)
    is_active_ = true;

  UpdateLoader(template_url, match.destination_url, match.transition, user_text,
               verbatim, suggested_text);

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
      Source<InstantController>(this),
      NotificationService::NoDetails());
  return true;
}

void InstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (omnibox_bounds_ == bounds)
    return;

  // Always track the omnibox bounds. That way if Update is later invoked the
  // bounds are in sync.
  omnibox_bounds_ = bounds;
  if (loader_.get())
    loader_->SetOmniboxBounds(bounds);
}

void InstantController::DestroyPreviewContents() {
  if (!loader_.get()) {
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
  if (is_displayable_) {
    is_displayable_ = false;
    delegate_->HideInstant();
  }
}

bool InstantController::IsCurrent() {
  // TODO(mmenke):  See if we can do something more intelligent in the
  //                navigation pending case.
  return is_displayable_ && !loader_->IsNavigationPending() &&
      !loader_->needs_reload();
}

TabContentsWrapper* InstantController::CommitCurrentPreview(
    InstantCommitType type) {
  DCHECK(loader_.get());
  TabContentsWrapper* tab = ReleasePreviewContents(type);
  tab->controller().CopyStateFromAndPrune(&tab_contents_->controller());
  delegate_->CommitInstant(tab);
  CompleteRelease(tab);
  return tab;
}

void InstantController::SetCommitOnMouseUp() {
  commit_on_mouse_up_ = true;
}

bool InstantController::IsMouseDownFromActivate() {
  DCHECK(loader_.get());
  return loader_->IsMouseDownFromActivate();
}

#if defined(OS_MACOSX)
void InstantController::OnAutocompleteLostFocus(
    gfx::NativeView view_gaining_focus) {
  // If |IsMouseDownFromActivate()| returns false, the RenderWidgetHostView did
  // not receive a mouseDown event.  Therefore, we should destroy the preview.
  // Otherwise, the RWHV was clicked, so we commit the preview.
  if (!IsCurrent() || !IsMouseDownFromActivate())
    DestroyPreviewContents();
  else
    SetCommitOnMouseUp();
}
#else
void InstantController::OnAutocompleteLostFocus(
    gfx::NativeView view_gaining_focus) {
  if (!IsCurrent()) {
    DestroyPreviewContents();
    return;
  }

  RenderWidgetHostView* rwhv =
      GetPreviewContents()->tab_contents()->GetRenderWidgetHostView();
  if (!view_gaining_focus || !rwhv) {
    DestroyPreviewContents();
    return;
  }

#if defined(TOOLKIT_VIEWS)
  // For views the top level widget is always focused. If the focus change
  // originated in views determine the child Widget from the view that is being
  // focused.
  if (view_gaining_focus) {
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
  }
#endif

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

    // We're showing instant results. As instant results may shift when
    // committing we commit on the mouse up. This way a slow click still works
    // fine.
    SetCommitOnMouseUp();
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
#endif

void InstantController::OnAutocompleteGotFocus(
    TabContentsWrapper* tab_contents) {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(switches::kPreloadInstantSearch) &&
      !InstantFieldTrial::IsExperimentGroup(tab_contents->profile())) {
    return;
  }

  TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
      tab_contents->profile());
  if (!model)
    return;

  const TemplateURL* template_url = model->GetDefaultSearchProvider();
  if (!IsValidInstantTemplateURL(template_url))
    return;

  tab_contents_ = tab_contents;

  if (!loader_.get())
    loader_.reset(new InstantLoader(this, template_url->id()));
  loader_->MaybeLoadInstantURL(tab_contents, template_url);
}

TabContentsWrapper* InstantController::ReleasePreviewContents(
    InstantCommitType type) {
  if (!loader_.get())
    return NULL;

  TabContentsWrapper* tab = loader_->ReleasePreviewContents(type);
  ClearBlacklist();
  is_active_ = false;
  is_displayable_ = false;
  commit_on_mouse_up_ = false;
  omnibox_bounds_ = gfx::Rect();
  loader_.reset();
  return tab;
}

void InstantController::CompleteRelease(TabContentsWrapper* tab) {
  tab->blocked_content_tab_helper()->SetAllContentsBlocked(false);
}

TabContentsWrapper* InstantController::GetPreviewContents() {
  return loader_.get() ? loader_->preview_contents() : NULL;
}

void InstantController::InstantStatusChanged(InstantLoader* loader) {
  DCHECK(loader_.get());
  UpdateIsDisplayable();
}

void InstantController::SetSuggestedTextFor(
    InstantLoader* loader,
    const string16& text,
    InstantCompleteBehavior behavior) {
  delegate_->SetSuggestedText(text, behavior);
}

gfx::Rect InstantController::GetInstantBounds() {
  return delegate_->GetInstantBounds();
}

bool InstantController::ShouldCommitInstantOnMouseUp() {
  return commit_on_mouse_up_;
}

void InstantController::CommitInstantLoader(InstantLoader* loader) {
  if (loader_.get() && loader_.get() == loader) {
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
  VLOG(1) << "provider does not support instant";

  // Don't attempt to use instant for this search engine again.
  BlacklistFromInstant();
}

void InstantController::AddToBlacklist(InstantLoader* loader, const GURL& url) {
  // Don't attempt to use instant for this search engine again.
  BlacklistFromInstant();

  // Because of the state of the stack we can't destroy the loader now.
  ScheduleDestroy(loader_.release());
  UpdateIsDisplayable();
}

void InstantController::SwappedTabContents(InstantLoader* loader) {
  if (is_displayable_)
    delegate_->ShowInstant(loader->preview_contents());
}

void InstantController::UpdateIsDisplayable() {
  bool displayable =
      (loader_.get() && loader_->ready() && loader_->http_status_ok());
  if (displayable == is_displayable_)
    return;

  is_displayable_ = displayable;
  if (!is_displayable_) {
    delegate_->HideInstant();
  } else {
    delegate_->ShowInstant(loader_->preview_contents());
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_CONTROLLER_SHOWN,
        Source<InstantController>(this),
        NotificationService::NoDetails());
  }
}

void InstantController::UpdateLoader(const TemplateURL* template_url,
                                     const GURL& url,
                                     content::PageTransition transition_type,
                                     const string16& user_text,
                                     bool verbatim,
                                     string16* suggested_text) {
  loader_->SetOmniboxBounds(omnibox_bounds_);
  loader_->Update(tab_contents_, template_url, url, transition_type, user_text,
                  verbatim, suggested_text);
  UpdateIsDisplayable();
}

bool InstantController::ShouldUseInstant(const AutocompleteMatch& match) {
  TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
      tab_contents_->profile());
  if (!model)
    return false;

  const TemplateURL* default_t_url = model->GetDefaultSearchProvider();
  const TemplateURL* match_t_url = match.template_url;
  return IsValidInstantTemplateURL(default_t_url) &&
      IsValidInstantTemplateURL(match_t_url) &&
      (match_t_url->id() == default_t_url->id());
}

// Returns true if |template_url| is a valid TemplateURL for use by instant.
bool InstantController::IsValidInstantTemplateURL(
    const TemplateURL* template_url) {
  return template_url && template_url->instant_url() && template_url->id() &&
      template_url->instant_url()->SupportsReplacement() &&
      !IsBlacklistedFromInstant(template_url->id());
}

void InstantController::BlacklistFromInstant() {
  if (!loader_.get())
    return;

  DCHECK(loader_->template_url_id());
  blacklisted_ids_.insert(loader_->template_url_id());

  // Because of the state of the stack we can't destroy the loader now.
  ScheduleDestroy(loader_.release());
  UpdateIsDisplayable();
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
