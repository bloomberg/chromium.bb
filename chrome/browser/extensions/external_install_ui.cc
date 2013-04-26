// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_install_ui.h"

#include <string>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Whether the external extension can use the streamlined bubble install flow.
bool UseBubbleInstall(const Extension* extension, bool is_new_profile) {
  return extension->UpdatesFromGallery() && !is_new_profile;
}

}  // namespace

static const int kMenuCommandId = IDC_EXTERNAL_EXTENSION_ALERT;

class ExternalInstallGlobalError;

// TODO(mpcomplete): Get rid of the refcounting on this class, or document
// why it's necessary. Will do after refactoring to merge back with
// ExtensionDisabledDialogDelegate.
class ExternalInstallDialogDelegate
    : public ExtensionInstallPrompt::Delegate,
      public base::RefCountedThreadSafe<ExternalInstallDialogDelegate> {
 public:
  ExternalInstallDialogDelegate(Browser* browser,
                                ExtensionService* service,
                                const Extension* extension,
                                ExternalInstallGlobalError* global_error);

 private:
  friend class base::RefCountedThreadSafe<ExternalInstallDialogDelegate>;
  friend class ExternalInstallGlobalError;

  virtual ~ExternalInstallDialogDelegate();

  // ExtensionInstallPrompt::Delegate:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // The UI for showing the install dialog when enabling.
  scoped_ptr<ExtensionInstallPrompt> install_ui_;

  ExtensionService* service_;
  const Extension* extension_;
};

// Only shows a menu item, no bubble. Clicking the menu item shows
// an external install dialog.
class ExternalInstallMenuAlert : public GlobalError,
                                 public content::NotificationObserver {
 public:
  ExternalInstallMenuAlert(ExtensionService* service,
                           const Extension* extension);
  virtual ~ExternalInstallMenuAlert();

  const Extension* extension() const { return extension_; }

  // GlobalError implementation.
  virtual Severity GetSeverity() OVERRIDE;
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual string16 GetBubbleViewTitle() OVERRIDE;
  virtual std::vector<string16> GetBubbleViewMessages() OVERRIDE;
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  ExtensionService* service_;
  const Extension* extension_;
  content::NotificationRegistrar registrar_;
};

// Shows a menu item and a global error bubble, replacing the install dialog.
class ExternalInstallGlobalError : public ExternalInstallMenuAlert {
 public:
  ExternalInstallGlobalError(ExtensionService* service,
                             const Extension* extension);
  virtual ~ExternalInstallGlobalError();

  void set_prompt(const ExtensionInstallPrompt::Prompt& prompt) {
    prompt_ = &prompt;
  }

  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual string16 GetBubbleViewTitle() OVERRIDE;
  virtual std::vector<string16> GetBubbleViewMessages() OVERRIDE;
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

 protected:
  // Ref-counted, but needs to be disposed of if we are dismissed without
  // having been clicked (perhaps because the user enabled the extension
  // manually).
  ExternalInstallDialogDelegate* delegate_;
  const ExtensionInstallPrompt::Prompt* prompt_;
};

static void SetExternalInstallBubble(
    ExternalInstallGlobalError* global_error,
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) {
  global_error->set_prompt(prompt);
}

static void ShowExternalInstallDialog(
    ExtensionService* service,
    Browser* browser,
    const Extension* extension) {
  // This object manages its own lifetime.
  new ExternalInstallDialogDelegate(browser, service, extension, NULL);
}

// ExternalInstallDialogDelegate --------------------------------------------

ExternalInstallDialogDelegate::ExternalInstallDialogDelegate(
    Browser* browser,
    ExtensionService* service,
    const Extension* extension,
    ExternalInstallGlobalError* global_error)
    : service_(service), extension_(extension) {
  AddRef();  // Balanced in Proceed or Abort.

  install_ui_.reset(
      ExtensionInstallUI::CreateInstallPromptWithBrowser(browser));

  const ExtensionInstallPrompt::ShowDialogCallback callback =
      global_error ?
      base::Bind(&SetExternalInstallBubble, global_error) :
      ExtensionInstallPrompt::GetDefaultShowDialogCallback();
  install_ui_->ConfirmExternalInstall(this, extension_, callback);
}

ExternalInstallDialogDelegate::~ExternalInstallDialogDelegate() {
}

void ExternalInstallDialogDelegate::InstallUIProceed() {
  service_->GrantPermissionsAndEnableExtension(extension_);
  Release();
}

void ExternalInstallDialogDelegate::InstallUIAbort(bool user_initiated) {
  service_->UninstallExtension(extension_->id(), false, NULL);
  Release();
}

// ExternalInstallMenuAlert -------------------------------------------------

ExternalInstallMenuAlert::ExternalInstallMenuAlert(
    ExtensionService* service,
    const Extension* extension)
    : service_(service),
      extension_(extension) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(service->profile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(service->profile()));
}

ExternalInstallMenuAlert::~ExternalInstallMenuAlert() {
}

GlobalError::Severity ExternalInstallMenuAlert::GetSeverity() {
  return SEVERITY_LOW;
}

bool ExternalInstallMenuAlert::HasMenuItem() {
  return true;
}

int ExternalInstallMenuAlert::MenuItemCommandID() {
  return kMenuCommandId;
}

string16 ExternalInstallMenuAlert::MenuItemLabel() {
  int id = -1;
  if (extension_->is_app())
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_APP;
  else if (extension_->is_theme())
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_THEME;
  else
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_EXTENSION;
  return l10n_util::GetStringFUTF16(id, UTF8ToUTF16(extension_->name()));
}

void ExternalInstallMenuAlert::ExecuteMenuItem(Browser* browser) {
  ShowExternalInstallDialog(service_, browser, extension_);
}

bool ExternalInstallMenuAlert::HasBubbleView() {
  return false;
}
string16 ExternalInstallMenuAlert::GetBubbleViewTitle() {
  return string16();
}

std::vector<string16> ExternalInstallMenuAlert::GetBubbleViewMessages() {
  return std::vector<string16>();
}

string16 ExternalInstallMenuAlert::GetBubbleViewAcceptButtonLabel() {
  return string16();
}

string16 ExternalInstallMenuAlert::GetBubbleViewCancelButtonLabel() {
  return string16();
}

void ExternalInstallMenuAlert::OnBubbleViewDidClose(Browser* browser) {
  NOTREACHED();
}

void ExternalInstallMenuAlert::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  NOTREACHED();
}

void ExternalInstallMenuAlert::BubbleViewCancelButtonPressed(
    Browser* browser) {
  NOTREACHED();
}

void ExternalInstallMenuAlert::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  const Extension* extension = NULL;
  // The error is invalidated if the extension has been reloaded or unloaded.
  if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    extension = content::Details<const Extension>(details).ptr();
  } else {
    DCHECK_EQ(chrome::NOTIFICATION_EXTENSION_UNLOADED, type);
    extensions::UnloadedExtensionInfo* info =
        content::Details<extensions::UnloadedExtensionInfo>(details).ptr();
    extension = info->extension;
  }
  if (extension == extension_) {
    GlobalErrorService* error_service =
        GlobalErrorServiceFactory::GetForProfile(service_->profile());
    error_service->RemoveGlobalError(this);
    service_->AcknowledgeExternalExtension(extension_->id());
    delete this;
  }
}

// ExternalInstallGlobalError -----------------------------------------------

ExternalInstallGlobalError::ExternalInstallGlobalError(
    ExtensionService* service,
    const Extension* extension)
    : ExternalInstallMenuAlert(service, extension) {
  delegate_ = new ExternalInstallDialogDelegate(
      NULL, service_, extension_, this);
}

ExternalInstallGlobalError::~ExternalInstallGlobalError() {
  if (delegate_)
    delegate_->Release();
}

void ExternalInstallGlobalError::ExecuteMenuItem(Browser* browser) {
  ShowBubbleView(browser);
}

bool ExternalInstallGlobalError::HasBubbleView() {
  return true;
}

string16 ExternalInstallGlobalError::GetBubbleViewTitle() {
  return prompt_->GetDialogTitle();
}

std::vector<string16> ExternalInstallGlobalError::GetBubbleViewMessages() {
  std::vector<string16> messages;
  messages.push_back(prompt_->GetHeading());
  if (prompt_->GetPermissionCount()) {
    messages.push_back(prompt_->GetPermissionsHeading());
    for (size_t i = 0; i < prompt_->GetPermissionCount(); ++i) {
      messages.push_back(l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PERMISSION_LINE,
          prompt_->GetPermission(i)));
    }
  }
  // TODO(yoz): OAuth issue advice?
  return messages;
}

string16 ExternalInstallGlobalError::GetBubbleViewAcceptButtonLabel() {
  return prompt_->GetAcceptButtonLabel();
}

string16 ExternalInstallGlobalError::GetBubbleViewCancelButtonLabel() {
  return prompt_->GetAbortButtonLabel();
}

void ExternalInstallGlobalError::OnBubbleViewDidClose(Browser* browser) {
}

void ExternalInstallGlobalError::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  delegate_->InstallUIProceed();
  delegate_ = NULL;
}

void ExternalInstallGlobalError::BubbleViewCancelButtonPressed(
    Browser* browser) {
  delegate_->InstallUIAbort(true);
  delegate_ = NULL;
}

// Public interface ---------------------------------------------------------

bool AddExternalInstallError(ExtensionService* service,
                             const Extension* extension,
                             bool is_new_profile) {
  GlobalErrorService* error_service =
      GlobalErrorServiceFactory::GetForProfile(service->profile());
  GlobalError* error = error_service->GetGlobalErrorByMenuItemCommandID(
      kMenuCommandId);
  if (error)
    return false;

  if (UseBubbleInstall(extension, is_new_profile)) {
    error_service->AddGlobalError(
        new ExternalInstallGlobalError(service, extension));
  } else {
    error_service->AddGlobalError(
        new ExternalInstallMenuAlert(service, extension));
  }
  return true;
}

void RemoveExternalInstallError(ExtensionService* service) {
  GlobalErrorService* error_service =
      GlobalErrorServiceFactory::GetForProfile(service->profile());
  GlobalError* error = error_service->GetGlobalErrorByMenuItemCommandID(
      kMenuCommandId);
  if (error) {
    error_service->RemoveGlobalError(error);
    delete error;
  }
}

bool HasExternalInstallError(ExtensionService* service) {
  GlobalErrorService* error_service =
      GlobalErrorServiceFactory::GetForProfile(service->profile());
  GlobalError* error = error_service->GetGlobalErrorByMenuItemCommandID(
      kMenuCommandId);
  return !!error;
}

bool HasExternalInstallBubble(ExtensionService* service) {
  GlobalErrorService* error_service =
      GlobalErrorServiceFactory::GetForProfile(service->profile());
  GlobalError* error = error_service->GetGlobalErrorByMenuItemCommandID(
      kMenuCommandId);
  return error && error->HasBubbleView();
}

}  // namespace extensions
