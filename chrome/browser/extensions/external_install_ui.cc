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

static const int kMenuCommandId = IDC_EXTERNAL_EXTENSION_ALERT;

// ExternalInstallDialogDelegate --------------------------------------------

// TODO(mpcomplete): Get rid of the refcounting on this class, or document
// why it's necessary. Will do after refactoring to merge back with
// ExtensionDisabledDialogDelegate.
class ExternalInstallDialogDelegate
    : public ExtensionInstallPrompt::Delegate,
      public base::RefCountedThreadSafe<ExternalInstallDialogDelegate> {
 public:
  ExternalInstallDialogDelegate(Browser* browser,
                                ExtensionService* service,
                                const Extension* extension);

 private:
  friend class base::RefCountedThreadSafe<ExternalInstallDialogDelegate>;

  virtual ~ExternalInstallDialogDelegate();

  // ExtensionInstallPrompt::Delegate:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // The UI for showing the install dialog when enabling.
  scoped_ptr<ExtensionInstallPrompt> install_ui_;

  ExtensionService* service_;
  const Extension* extension_;
};

ExternalInstallDialogDelegate::ExternalInstallDialogDelegate(
    Browser* browser,
    ExtensionService* service,
    const Extension* extension)
    : service_(service), extension_(extension) {
  AddRef();  // Balanced in Proceed or Abort.

  install_ui_.reset(
      ExtensionInstallUI::CreateInstallPromptWithBrowser(browser));
  install_ui_->ConfirmExternalInstall(this, extension_);
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

static void ShowExternalInstallDialog(ExtensionService* service,
                                      Browser* browser,
                                      const Extension* extension) {
  // This object manages its own lifetime.
  new ExternalInstallDialogDelegate(browser, service, extension);
}

// ExternalInstallGlobalError -----------------------------------------------

class ExternalInstallGlobalError : public GlobalError,
                                   public content::NotificationObserver {
 public:
  ExternalInstallGlobalError(ExtensionService* service,
                               const Extension* extension);
  virtual ~ExternalInstallGlobalError();

  const Extension* extension() const { return extension_; }

  // GlobalError implementation.
  virtual Severity GetSeverity() OVERRIDE;
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual string16 MenuItemLabel() OVERRIDE;
  virtual int MenuItemIconResourceID() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual string16 GetBubbleViewTitle() OVERRIDE;
  virtual string16 GetBubbleViewMessage() OVERRIDE;
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  ExtensionService* service_;
  const Extension* extension_;
  content::NotificationRegistrar registrar_;
};

ExternalInstallGlobalError::ExternalInstallGlobalError(
    ExtensionService* service,
    const Extension* extension)
    : service_(service),
      extension_(extension) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(service->profile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(service->profile()));
}

ExternalInstallGlobalError::~ExternalInstallGlobalError() {
}

GlobalError::Severity ExternalInstallGlobalError::GetSeverity() {
  return SEVERITY_LOW;
}

bool ExternalInstallGlobalError::HasMenuItem() {
  return true;
}

int ExternalInstallGlobalError::MenuItemCommandID() {
  return kMenuCommandId;
}

int ExternalInstallGlobalError::MenuItemIconResourceID() {
  return IDR_UPDATE_MENU_EXTENSION;
}

string16 ExternalInstallGlobalError::MenuItemLabel() {
  int id = -1;
  if (extension_->is_app())
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_APP;
  else if (extension_->is_theme())
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_THEME;
  else
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_EXTENSION;
  return l10n_util::GetStringFUTF16(id, UTF8ToUTF16(extension_->name()));
}

void ExternalInstallGlobalError::ExecuteMenuItem(Browser* browser) {
  ShowExternalInstallDialog(service_, browser, extension_);
}

bool ExternalInstallGlobalError::HasBubbleView() {
  return false;
}

string16 ExternalInstallGlobalError::GetBubbleViewTitle() {
  return string16();
}

string16 ExternalInstallGlobalError::GetBubbleViewMessage() {
  return string16();
}

string16 ExternalInstallGlobalError::GetBubbleViewAcceptButtonLabel() {
  return string16();
}

string16 ExternalInstallGlobalError::GetBubbleViewCancelButtonLabel() {
  return string16();
}

void ExternalInstallGlobalError::OnBubbleViewDidClose(Browser* browser) {
  NOTREACHED();
}

void ExternalInstallGlobalError::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  NOTREACHED();
}

void ExternalInstallGlobalError::BubbleViewCancelButtonPressed(
    Browser* browser) {
  NOTREACHED();
}

void ExternalInstallGlobalError::Observe(
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

// Public interface ---------------------------------------------------------

bool AddExternalInstallError(ExtensionService* service,
                             const Extension* extension) {
  GlobalErrorService* error_service =
      GlobalErrorServiceFactory::GetForProfile(service->profile());
  GlobalError* error = error_service->GetGlobalErrorByMenuItemCommandID(
      kMenuCommandId);
  if (error)
    return false;

  error_service->AddGlobalError(
      new ExternalInstallGlobalError(service, extension));
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

}  // namespace extensions
