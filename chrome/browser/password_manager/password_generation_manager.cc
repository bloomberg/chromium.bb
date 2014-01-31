// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_generation_manager.h"

#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/rect.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PasswordGenerationManager);

PasswordGenerationManager::PasswordGenerationManager(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      observer_(NULL) {}

PasswordGenerationManager::~PasswordGenerationManager() {}

void PasswordGenerationManager::SetTestObserver(
    autofill::PasswordGenerationPopupObserver* observer) {
  observer_ = observer;
}

void PasswordGenerationManager::DetectAccountCreationForms(
    const std::vector<autofill::FormStructure*>& forms) {
  std::vector<autofill::FormData> account_creation_forms;
  for (std::vector<autofill::FormStructure*>::const_iterator form_it =
           forms.begin(); form_it != forms.end(); ++form_it) {
    autofill::FormStructure* form = *form_it;
    for (std::vector<autofill::AutofillField*>::const_iterator field_it =
             form->begin(); field_it != form->end(); ++field_it) {
      autofill::AutofillField* field = *field_it;
      if (field->server_type() == autofill::ACCOUNT_CREATION_PASSWORD) {
        account_creation_forms.push_back(form->ToFormData());
        break;
      }
    }
  }
  if (!account_creation_forms.empty() && IsGenerationEnabled()) {
    SendAccountCreationFormsToRenderer(web_contents()->GetRenderViewHost(),
                                       account_creation_forms);
  }
}

bool PasswordGenerationManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordGenerationManager, message)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ShowPasswordGenerationPopup,
                        OnShowPasswordGenerationPopup)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ShowPasswordEditingPopup,
                        OnShowPasswordEditingPopup)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_HidePasswordGenerationPopup,
                        OnHidePasswordGenerationPopup)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

// In order for password generation to be enabled, we need to make sure:
// (1) Password sync is enabled, and
// (2) Password saving is enabled.
bool PasswordGenerationManager::IsGenerationEnabled() const {
  if (!web_contents())
    return false;

  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());

  if (!PasswordManager::FromWebContents(web_contents())->IsSavingEnabled()) {
    DVLOG(2) << "Generation disabled because password saving is disabled";
    return false;
  }

  bool password_sync_enabled = false;
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (sync_service) {
    syncer::ModelTypeSet sync_set = sync_service->GetActiveDataTypes();
    password_sync_enabled = (sync_service->HasSyncSetupCompleted() &&
                             sync_set.Has(syncer::PASSWORDS));
  }
  if (!password_sync_enabled) {
    DVLOG(2) << "Generation disabled because passwords are not being synced";
    return false;
  }

  return true;
}

void PasswordGenerationManager::SendAccountCreationFormsToRenderer(
    content::RenderViewHost* host,
    const std::vector<autofill::FormData>& forms) {
  host->Send(new AutofillMsg_AccountCreationFormsDetected(
      host->GetRoutingID(), forms));
}

gfx::RectF PasswordGenerationManager::GetBoundsInScreenSpace(
    const gfx::RectF& bounds) {
  gfx::Rect client_area;
  web_contents()->GetView()->GetContainerBounds(&client_area);
  return bounds + client_area.OffsetFromOrigin();
}

void PasswordGenerationManager::OnShowPasswordGenerationPopup(
    const gfx::RectF& bounds,
    int max_length,
    const autofill::PasswordForm& form) {
  // TODO(gcasto): Validate data in PasswordForm.

  // Only implemented for Aura right now.
#if defined(USE_AURA)
  // Convert element_bounds to be in screen space.
  gfx::RectF element_bounds_in_screen_space = GetBoundsInScreenSpace(bounds);

  password_generator_.reset(new autofill::PasswordGenerator(max_length));

  popup_controller_ =
      autofill::PasswordGenerationPopupControllerImpl::GetOrCreate(
          popup_controller_,
          element_bounds_in_screen_space,
          form,
          password_generator_.get(),
          PasswordManager::FromWebContents(web_contents()),
          observer_,
          web_contents(),
          web_contents()->GetView()->GetNativeView());
  popup_controller_->Show();
#endif  // #if defined(USE_AURA)
}

void PasswordGenerationManager::OnShowPasswordEditingPopup(
    const gfx::RectF& bounds) {
  // TODO(gcasto): Enable this.
}

void PasswordGenerationManager::OnHidePasswordGenerationPopup() {
  HidePopup();
}

void PasswordGenerationManager::HidePopup() {
  if (popup_controller_)
    popup_controller_->HideAndDestroy();
}
