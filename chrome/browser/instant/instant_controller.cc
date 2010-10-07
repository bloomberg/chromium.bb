// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_controller.h"

#include "base/command_line.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/instant/instant_delegate.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/instant/instant_loader_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"

// static
bool InstantController::IsEnabled() {
  static bool enabled = false;
  static bool checked = false;
  if (!checked) {
    checked = true;
    enabled = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableMatchPreview);
  }
  return enabled;
}

InstantController::InstantController(InstantDelegate* delegate)
    : delegate_(delegate),
      tab_contents_(NULL),
      is_active_(false),
      commit_on_mouse_up_(false),
      last_transition_type_(PageTransition::LINK) {
}

InstantController::~InstantController() {
}

void InstantController::Update(TabContents* tab_contents,
                               const AutocompleteMatch& match,
                               const string16& user_text,
                               string16* suggested_text) {
  if (tab_contents != tab_contents_)
    DestroyPreviewContents();

  const GURL& url = match.destination_url;

  tab_contents_ = tab_contents;
  commit_on_mouse_up_ = false;
  last_transition_type_ = match.transition;

  if (loader_manager_.get() && loader_manager_->active_loader()->url() == url)
    return;

  if (url.is_empty() || !url.is_valid() || !ShouldShowPreviewFor(url)) {
    DestroyPreviewContents();
    return;
  }

  if (!loader_manager_.get())
    loader_manager_.reset(new InstantLoaderManager(this));

  const TemplateURL* template_url = match.template_url;
  if (match.type == AutocompleteMatch::SEARCH_WHAT_YOU_TYPED ||
      match.type == AutocompleteMatch::SEARCH_HISTORY ||
      match.type == AutocompleteMatch::SEARCH_SUGGEST) {
    TemplateURLModel* model = tab_contents->profile()->GetTemplateURLModel();
    template_url = model ? model->GetDefaultSearchProvider() : NULL;
  }
  if (template_url && (!template_url->id() ||
                       IsBlacklistedFromInstant(template_url->id()) ||
                       !TemplateURL::SupportsReplacement(template_url))) {
    template_url = NULL;
  }
  TemplateURLID template_url_id = template_url ? template_url->id() : 0;

  UpdateLoader(template_url_id, match.destination_url, match.transition,
               user_text, template_url, suggested_text);
}

void InstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (omnibox_bounds_ == bounds)
    return;

  if (loader_manager_.get()) {
    omnibox_bounds_ = bounds;
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

  delegate_->HideInstant();
  delete ReleasePreviewContents(INSTANT_COMMIT_DESTROY);
}

bool InstantController::IsCurrent() {
  return loader_manager_.get() && loader_manager_->active_loader()->ready();
}

void InstantController::CommitCurrentPreview(InstantCommitType type) {
  DCHECK(loader_manager_.get());
  DCHECK(loader_manager_->current_loader());
  delegate_->CommitInstant(ReleasePreviewContents(type));
}

void InstantController::SetCommitOnMouseUp() {
  commit_on_mouse_up_ = true;
}

bool InstantController::IsMouseDownFromActivate() {
  DCHECK(loader_manager_.get());
  DCHECK(loader_manager_->current_loader());
  return loader_manager_->current_loader()->IsMouseDownFromActivate();
}

TabContents* InstantController::ReleasePreviewContents(InstantCommitType type) {
  if (!loader_manager_.get())
    return NULL;

  scoped_ptr<InstantLoader> loader(loader_manager_->ReleaseCurrentLoader());
  TabContents* tab = loader->ReleasePreviewContents(type);

  ClearBlacklist();
  is_active_ = false;
  omnibox_bounds_ = gfx::Rect();
  commit_on_mouse_up_ = false;
  loader_manager_.reset(NULL);
  return tab;
}

TabContents* InstantController::GetPreviewContents() {
  return loader_manager_.get() ?
      loader_manager_->current_loader()->preview_contents() : NULL;
}

bool InstantController::IsShowingInstant() {
  return loader_manager_.get() &&
      loader_manager_->current_loader()->is_showing_instant();
}

void InstantController::ShowInstantLoader(InstantLoader* loader) {
  DCHECK(loader_manager_.get());
  if (loader_manager_->current_loader() == loader) {
    is_active_ = true;
    delegate_->ShowInstant(loader->preview_contents());
  } else if (loader_manager_->pending_loader() == loader) {
    scoped_ptr<InstantLoader> old_loader;
    loader_manager_->MakePendingCurrent(&old_loader);
    delegate_->ShowInstant(loader->preview_contents());
  } else {
    // The loader supports instant but isn't active yet. Nothing to do.
  }
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
    InstantLoader* loader,
    bool needs_reload) {
  DCHECK(!loader->ready());  // We better not be showing this loader.
  DCHECK(loader->template_url_id());

  BlacklistFromInstant(loader->template_url_id());

  if (loader_manager_->active_loader() == loader) {
    // The loader is active. Continue to use it, but make sure it isn't tied to
    // to the search engine anymore. ClearTemplateURLID ends up showing the
    // loader.
    loader_manager_->RemoveLoaderFromInstant(loader);
    loader->ClearTemplateURLID();

    if (needs_reload) {
      string16 suggested_text;
      loader->Update(tab_contents_, loader->url(), last_transition_type_,
                     loader->user_text(), NULL, &suggested_text);
    }

  } else {
    loader_manager_->DestroyLoader(loader);
    loader = NULL;
  }
}

void InstantController::UpdateLoader(TemplateURLID template_url_id,
                                     const GURL& url,
                                     PageTransition::Type transition_type,
                                     const string16& user_text,
                                     const TemplateURL* template_url,
                                     string16* suggested_text) {
  InstantLoader* old_loader = loader_manager_->current_loader();
  scoped_ptr<InstantLoader> owned_loader;
  InstantLoader* new_loader =
      loader_manager_->UpdateLoader(template_url_id, &owned_loader);

  new_loader->SetOmniboxBounds(omnibox_bounds_);
  new_loader->Update(tab_contents_, url, transition_type, user_text,
                     template_url, suggested_text);
  if (old_loader != new_loader && new_loader->ready())
    delegate_->ShowInstant(new_loader->preview_contents());
}

bool InstantController::ShouldShowPreviewFor(const GURL& url) {
  return !url.SchemeIs(chrome::kJavaScriptScheme);
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
