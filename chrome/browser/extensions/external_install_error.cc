// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_install_error.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace extensions {

namespace {

// Return the menu label for a global error.
base::string16 GetMenuItemLabel(const Extension* extension) {
  if (!extension)
    return base::string16();

  int id = -1;
  if (extension->is_app())
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_APP;
  else if (extension->is_theme())
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_THEME;
  else
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_EXTENSION;

  return l10n_util::GetStringFUTF16(id, base::UTF8ToUTF16(extension->name()));
}

// A global error that spawns a dialog when the menu item is clicked.
class ExternalInstallMenuAlert : public GlobalError {
 public:
  explicit ExternalInstallMenuAlert(ExternalInstallError* error);
  virtual ~ExternalInstallMenuAlert();

 private:
  // GlobalError implementation.
  virtual Severity GetSeverity() OVERRIDE;
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual base::string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual bool HasShownBubbleView() OVERRIDE;
  virtual void ShowBubbleView(Browser* browser) OVERRIDE;
  virtual GlobalErrorBubbleViewBase* GetBubbleView() OVERRIDE;

  // The owning ExternalInstallError.
  ExternalInstallError* error_;

  DISALLOW_COPY_AND_ASSIGN(ExternalInstallMenuAlert);
};

// A global error that spawns a bubble when the menu item is clicked.
class ExternalInstallBubbleAlert : public GlobalErrorWithStandardBubble {
 public:
  explicit ExternalInstallBubbleAlert(ExternalInstallError* error,
                                      ExtensionInstallPrompt::Prompt* prompt);
  virtual ~ExternalInstallBubbleAlert();

 private:
  // GlobalError implementation.
  virtual Severity GetSeverity() OVERRIDE;
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual base::string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;

  // GlobalErrorWithStandardBubble implementation.
  virtual gfx::Image GetBubbleViewIcon() OVERRIDE;
  virtual base::string16 GetBubbleViewTitle() OVERRIDE;
  virtual std::vector<base::string16> GetBubbleViewMessages() OVERRIDE;
  virtual base::string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual base::string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

  // The owning ExternalInstallError.
  ExternalInstallError* error_;

  // The Prompt with all information, which we then use to populate the bubble.
  ExtensionInstallPrompt::Prompt* prompt_;

  DISALLOW_COPY_AND_ASSIGN(ExternalInstallBubbleAlert);
};

////////////////////////////////////////////////////////////////////////////////
// ExternalInstallMenuAlert

ExternalInstallMenuAlert::ExternalInstallMenuAlert(ExternalInstallError* error)
    : error_(error) {
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
  return IDC_EXTERNAL_EXTENSION_ALERT;
}

base::string16 ExternalInstallMenuAlert::MenuItemLabel() {
  return GetMenuItemLabel(error_->GetExtension());
}

void ExternalInstallMenuAlert::ExecuteMenuItem(Browser* browser) {
  error_->ShowDialog(browser);
}

bool ExternalInstallMenuAlert::HasBubbleView() {
  return false;
}

bool ExternalInstallMenuAlert::HasShownBubbleView() {
  NOTREACHED();
  return true;
}

void ExternalInstallMenuAlert::ShowBubbleView(Browser* browser) {
  NOTREACHED();
}

GlobalErrorBubbleViewBase* ExternalInstallMenuAlert::GetBubbleView() {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ExternalInstallBubbleAlert

ExternalInstallBubbleAlert::ExternalInstallBubbleAlert(
    ExternalInstallError* error,
    ExtensionInstallPrompt::Prompt* prompt)
    : error_(error), prompt_(prompt) {
  DCHECK(error_);
  DCHECK(prompt_);
}

ExternalInstallBubbleAlert::~ExternalInstallBubbleAlert() {
}

GlobalError::Severity ExternalInstallBubbleAlert::GetSeverity() {
  return SEVERITY_LOW;
}

bool ExternalInstallBubbleAlert::HasMenuItem() {
  return true;
}

int ExternalInstallBubbleAlert::MenuItemCommandID() {
  return IDC_EXTERNAL_EXTENSION_ALERT;
}

base::string16 ExternalInstallBubbleAlert::MenuItemLabel() {
  return GetMenuItemLabel(error_->GetExtension());
}

void ExternalInstallBubbleAlert::ExecuteMenuItem(Browser* browser) {
  ShowBubbleView(browser);
}

gfx::Image ExternalInstallBubbleAlert::GetBubbleViewIcon() {
  if (prompt_->icon().IsEmpty())
    return GlobalErrorWithStandardBubble::GetBubbleViewIcon();
  // Scale icon to a reasonable size.
  return gfx::Image(gfx::ImageSkiaOperations::CreateResizedImage(
      *prompt_->icon().ToImageSkia(),
      skia::ImageOperations::RESIZE_BEST,
      gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                extension_misc::EXTENSION_ICON_SMALL)));
}

base::string16 ExternalInstallBubbleAlert::GetBubbleViewTitle() {
  return prompt_->GetDialogTitle();
}

std::vector<base::string16>
ExternalInstallBubbleAlert::GetBubbleViewMessages() {
  std::vector<base::string16> messages;
  messages.push_back(prompt_->GetHeading());
  if (prompt_->GetPermissionCount()) {
    messages.push_back(prompt_->GetPermissionsHeading());
    for (size_t i = 0; i < prompt_->GetPermissionCount(); ++i) {
      messages.push_back(l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PERMISSION_LINE, prompt_->GetPermission(i)));
    }
  }
  // TODO(yoz): OAuth issue advice?
  return messages;
}

base::string16 ExternalInstallBubbleAlert::GetBubbleViewAcceptButtonLabel() {
  return prompt_->GetAcceptButtonLabel();
}

base::string16 ExternalInstallBubbleAlert::GetBubbleViewCancelButtonLabel() {
  return prompt_->GetAbortButtonLabel();
}

void ExternalInstallBubbleAlert::OnBubbleViewDidClose(Browser* browser) {
}

void ExternalInstallBubbleAlert::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  error_->InstallUIProceed();
}

void ExternalInstallBubbleAlert::BubbleViewCancelButtonPressed(
    Browser* browser) {
  error_->InstallUIAbort(true);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExternalInstallError

ExternalInstallError::ExternalInstallError(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    AlertType alert_type,
    ExternalInstallManager* manager)
    : browser_context_(browser_context),
      extension_id_(extension_id),
      alert_type_(alert_type),
      manager_(manager),
      error_service_(GlobalErrorServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_))),
      weak_factory_(this) {
  prompt_ = new ExtensionInstallPrompt::Prompt(
      ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT);

  webstore_data_fetcher_.reset(new WebstoreDataFetcher(
      this, browser_context_->GetRequestContext(), GURL(), extension_id_));
  webstore_data_fetcher_->Start();
}

ExternalInstallError::~ExternalInstallError() {
  if (global_error_.get())
    error_service_->RemoveGlobalError(global_error_.get());
}

void ExternalInstallError::InstallUIProceed() {
  const Extension* extension = GetExtension();
  if (extension) {
    ExtensionSystem::Get(browser_context_)
        ->extension_service()
        ->GrantPermissionsAndEnableExtension(extension);
    // Since the manager listens for the extension to be loaded, this will
    // remove the error...
  } else {
    // ... Otherwise we have to do it explicitly.
    manager_->RemoveExternalInstallError();
  }
}

void ExternalInstallError::InstallUIAbort(bool user_initiated) {
  if (user_initiated && GetExtension()) {
    ExtensionSystem::Get(browser_context_)
        ->extension_service()
        ->UninstallExtension(extension_id_,
                             extensions::UNINSTALL_REASON_INSTALL_CANCELED,
                             NULL);  // Ignore error.
    // Since the manager listens for the extension to be removed, this will
    // remove the error...
  } else {
    // ... Otherwise we have to do it explicitly.
    manager_->RemoveExternalInstallError();
  }
}

void ExternalInstallError::ShowDialog(Browser* browser) {
  DCHECK(install_ui_.get());
  DCHECK(prompt_.get());
  DCHECK(browser);
  content::WebContents* web_contents = NULL;
#if !defined(OS_ANDROID)
  web_contents = browser->tab_strip_model()->GetActiveWebContents();
#endif
  ExtensionInstallPrompt::ShowParams params(web_contents);
  ExtensionInstallPrompt::GetDefaultShowDialogCallback().Run(
      params, this, prompt_);
}

const Extension* ExternalInstallError::GetExtension() const {
  return ExtensionRegistry::Get(browser_context_)
      ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
}

void ExternalInstallError::OnWebstoreRequestFailure() {
  OnFetchComplete();
}

void ExternalInstallError::OnWebstoreResponseParseSuccess(
    scoped_ptr<base::DictionaryValue> webstore_data) {
  std::string localized_user_count;
  double average_rating = 0;
  int rating_count = 0;
  if (!webstore_data->GetString(kUsersKey, &localized_user_count) ||
      !webstore_data->GetDouble(kAverageRatingKey, &average_rating) ||
      !webstore_data->GetInteger(kRatingCountKey, &rating_count)) {
    // If we don't get a valid webstore response, short circuit, and continue
    // to show a prompt without webstore data.
    OnFetchComplete();
    return;
  }

  bool show_user_count = true;
  webstore_data->GetBoolean(kShowUserCountKey, &show_user_count);

  prompt_->SetWebstoreData(
      localized_user_count, show_user_count, average_rating, rating_count);
  OnFetchComplete();
}

void ExternalInstallError::OnWebstoreResponseParseFailure(
    const std::string& error) {
  OnFetchComplete();
}

void ExternalInstallError::OnFetchComplete() {
  // Create a new ExtensionInstallPrompt. We pass in NULL for the UI
  // components because we display at a later point, and don't want
  // to pass ones which may be invalidated.
  install_ui_.reset(
      new ExtensionInstallPrompt(Profile::FromBrowserContext(browser_context_),
                                 NULL,    // NULL native window.
                                 NULL));  // NULL navigator.

  install_ui_->ConfirmExternalInstall(
      this,
      GetExtension(),
      base::Bind(&ExternalInstallError::OnDialogReady,
                 weak_factory_.GetWeakPtr()),
      prompt_);
}

void ExternalInstallError::OnDialogReady(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* prompt_delegate,
    scoped_refptr<ExtensionInstallPrompt::Prompt> prompt) {
  DCHECK_EQ(this, prompt_delegate);
  prompt_ = prompt;

  if (alert_type_ == BUBBLE_ALERT) {
    global_error_.reset(new ExternalInstallBubbleAlert(this, prompt_));
    error_service_->AddGlobalError(global_error_.get());

    Browser* browser = NULL;
#if !defined(OS_ANDROID)
    browser =
        chrome::FindTabbedBrowser(Profile::FromBrowserContext(browser_context_),
                                  true,
                                  chrome::GetActiveDesktop());
#endif  // !defined(OS_ANDROID)
    if (browser)
      global_error_->ShowBubbleView(browser);
  } else {
    DCHECK(alert_type_ == MENU_ALERT);
    global_error_.reset(new ExternalInstallMenuAlert(this));
    error_service_->AddGlobalError(global_error_.get());
  }
}

}  // namespace extensions
