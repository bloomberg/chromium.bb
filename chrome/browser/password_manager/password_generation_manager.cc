// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_generation_manager.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/common/autofill_messages.h"
#include "components/autofill/browser/password_generator.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/password_form.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/rect.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PasswordGenerationManager);

PasswordGenerationManager::PasswordGenerationManager(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      enabled_(false),
      weak_factory_(this) {
  RegisterWithSyncService();
}

PasswordGenerationManager::~PasswordGenerationManager() {}

// static
void PasswordGenerationManager::CreateForWebContents(
    content::WebContents* contents) {
  content::WebContentsUserData<PasswordGenerationManager>::
      CreateForWebContents(contents);

  // Start observing changes to relevant prefs. This is not called in the
  // constructor so that it's not enabled in testing.
  FromWebContents(contents)->SetUpPrefChangeRegistrar();
}

// static
void PasswordGenerationManager::RegisterUserPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kPasswordGenerationEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void PasswordGenerationManager::RegisterWithSyncService() {
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (sync_service)
    sync_service->AddObserver(this);
}

void PasswordGenerationManager::SetUpPrefChangeRegistrar() {
  registrar_.Init(Profile::FromBrowserContext(
      web_contents()->GetBrowserContext())->GetPrefs());
  registrar_.Add(
      prefs::kPasswordGenerationEnabled,
      base::Bind(&PasswordGenerationManager::OnPrefStateChanged,
                 weak_factory_.GetWeakPtr()));
}

void PasswordGenerationManager::RenderViewCreated(
    content::RenderViewHost* host) {
  UpdateState(host, true);
}

bool PasswordGenerationManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordGenerationManager, message)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ShowPasswordGenerationPopup,
                        OnShowPasswordGenerationPopup)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PasswordGenerationManager::WebContentsDestroyed(
    content::WebContents* contents) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (sync_service && sync_service->HasObserver(this))
    sync_service->RemoveObserver(this);
}

void PasswordGenerationManager::OnPrefStateChanged() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (web_contents() && web_contents()->GetRenderViewHost())
    UpdateState(web_contents()->GetRenderViewHost(), false);
}

void PasswordGenerationManager::OnStateChanged() {
  // It is possible for sync state to change during tab contents destruction.
  // In this case, we don't need to update the renderer since it's going away.
  if (web_contents() && web_contents()->GetRenderViewHost())
    UpdateState(web_contents()->GetRenderViewHost(), false);
}

// In order for password generation to be enabled, we need to make sure:
// (1) Password sync is enabled,
// (2) Password manager is enabled, and
// (3) Password generation preference check box is checked.
void PasswordGenerationManager::UpdateState(content::RenderViewHost* host,
                                            bool new_renderer) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());

  bool saving_passwords_enabled =
      PasswordManager::FromWebContents(web_contents())->IsSavingEnabled();

  bool preference_checked = profile->GetPrefs()->GetBoolean(
      prefs::kPasswordGenerationEnabled);

  bool password_sync_enabled = false;
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (sync_service) {
    syncer::ModelTypeSet sync_set = sync_service->GetPreferredDataTypes();
    password_sync_enabled = (sync_service->HasSyncSetupCompleted() &&
                             sync_set.Has(syncer::PASSWORDS));
  }

  bool new_enabled = (password_sync_enabled &&
                      saving_passwords_enabled &&
                      preference_checked);

  if (new_enabled != enabled_ || new_renderer) {
    enabled_ = new_enabled;
    SendStateToRenderer(host, enabled_);
  }
}

void PasswordGenerationManager::SendStateToRenderer(
    content::RenderViewHost* host, bool enabled) {
  host->Send(new AutofillMsg_PasswordGenerationEnabled(host->GetRoutingID(),
                                                       enabled));
}

void PasswordGenerationManager::OnShowPasswordGenerationPopup(
    const gfx::Rect& bounds,
    int max_length,
    const content::PasswordForm& form) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else
  password_generator_.reset(new autofill::PasswordGenerator(max_length));
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  browser->window()->ShowPasswordGenerationBubble(bounds,
                                                  form,
                                                  password_generator_.get());
#endif  // #if defined(OS_ANDROID)
}
