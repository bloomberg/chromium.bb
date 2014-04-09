// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_disabled_ui.h"

#include <string>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/size.h"

using extensions::Extension;

namespace {

static const int kIconSize = extension_misc::EXTENSION_ICON_SMALL;

static base::LazyInstance<
    std::bitset<IDC_EXTENSION_DISABLED_LAST -
                IDC_EXTENSION_DISABLED_FIRST + 1> >
    menu_command_ids = LAZY_INSTANCE_INITIALIZER;

// Get an available menu ID.
int GetMenuCommandID() {
  int id;
  for (id = IDC_EXTENSION_DISABLED_FIRST;
       id <= IDC_EXTENSION_DISABLED_LAST; ++id) {
    if (!menu_command_ids.Get()[id - IDC_EXTENSION_DISABLED_FIRST]) {
      menu_command_ids.Get().set(id - IDC_EXTENSION_DISABLED_FIRST);
      return id;
    }
  }
  // This should not happen.
  DCHECK(id <= IDC_EXTENSION_DISABLED_LAST) <<
      "No available menu command IDs for ExtensionDisabledGlobalError";
  return IDC_EXTENSION_DISABLED_LAST;
}

// Make a menu ID available when it is no longer used.
void ReleaseMenuCommandID(int id) {
  menu_command_ids.Get().reset(id - IDC_EXTENSION_DISABLED_FIRST);
}

}  // namespace

// ExtensionDisabledDialogDelegate --------------------------------------------

class ExtensionDisabledDialogDelegate
    : public ExtensionInstallPrompt::Delegate,
      public base::RefCountedThreadSafe<ExtensionDisabledDialogDelegate> {
 public:
  ExtensionDisabledDialogDelegate(ExtensionService* service,
                                  scoped_ptr<ExtensionInstallPrompt> install_ui,
                                  const Extension* extension);

 private:
  friend class base::RefCountedThreadSafe<ExtensionDisabledDialogDelegate>;

  virtual ~ExtensionDisabledDialogDelegate();

  // ExtensionInstallPrompt::Delegate:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // The UI for showing the install dialog when enabling.
  scoped_ptr<ExtensionInstallPrompt> install_ui_;

  ExtensionService* service_;
  const Extension* extension_;
};

ExtensionDisabledDialogDelegate::ExtensionDisabledDialogDelegate(
    ExtensionService* service,
    scoped_ptr<ExtensionInstallPrompt> install_ui,
    const Extension* extension)
    : install_ui_(install_ui.Pass()),
      service_(service),
      extension_(extension) {
  AddRef();  // Balanced in Proceed or Abort.
  install_ui_->ConfirmReEnable(this, extension_);
}

ExtensionDisabledDialogDelegate::~ExtensionDisabledDialogDelegate() {
}

void ExtensionDisabledDialogDelegate::InstallUIProceed() {
  service_->GrantPermissionsAndEnableExtension(extension_);
  Release();
}

void ExtensionDisabledDialogDelegate::InstallUIAbort(bool user_initiated) {
  std::string histogram_name = user_initiated ?
      "Extensions.Permissions_ReEnableCancel" :
      "Extensions.Permissions_ReEnableAbort";
  ExtensionService::RecordPermissionMessagesHistogram(
      extension_, histogram_name.c_str());

  // Do nothing. The extension will remain disabled.
  Release();
}

// ExtensionDisabledGlobalError -----------------------------------------------

class ExtensionDisabledGlobalError : public GlobalErrorWithStandardBubble,
                                     public content::NotificationObserver,
                                     public ExtensionUninstallDialog::Delegate {
 public:
  ExtensionDisabledGlobalError(ExtensionService* service,
                               const Extension* extension,
                               const gfx::Image& icon);
  virtual ~ExtensionDisabledGlobalError();

  // GlobalError implementation.
  virtual Severity GetSeverity() OVERRIDE;
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual base::string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual gfx::Image GetBubbleViewIcon() OVERRIDE;
  virtual base::string16 GetBubbleViewTitle() OVERRIDE;
  virtual std::vector<base::string16> GetBubbleViewMessages() OVERRIDE;
  virtual base::string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual base::string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

  // ExtensionUninstallDialog::Delegate implementation.
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  ExtensionService* service_;
  const Extension* extension_;
  gfx::Image icon_;

  // How the user responded to the error; used for metrics.
  enum UserResponse {
    IGNORED,
    REENABLE,
    UNINSTALL,
    EXTENSION_DISABLED_UI_BUCKET_BOUNDARY
  };
  UserResponse user_response_;

  scoped_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  // Menu command ID assigned for this extension's error.
  int menu_command_id_;

  content::NotificationRegistrar registrar_;
};

// TODO(yoz): create error at startup for disabled extensions.
ExtensionDisabledGlobalError::ExtensionDisabledGlobalError(
    ExtensionService* service,
    const Extension* extension,
    const gfx::Image& icon)
    : service_(service),
      extension_(extension),
      icon_(icon),
      user_response_(IGNORED),
      menu_command_id_(GetMenuCommandID()) {
  if (icon_.IsEmpty()) {
    icon_ = gfx::Image(
        gfx::ImageSkiaOperations::CreateResizedImage(
            extension_->is_app() ?
                extensions::IconsInfo::GetDefaultAppIcon() :
                extensions::IconsInfo::GetDefaultExtensionIcon(),
            skia::ImageOperations::RESIZE_BEST,
            gfx::Size(kIconSize, kIconSize)));
  }
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(service->profile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_REMOVED,
                 content::Source<Profile>(service->profile()));
}

ExtensionDisabledGlobalError::~ExtensionDisabledGlobalError() {
  ReleaseMenuCommandID(menu_command_id_);
  UMA_HISTOGRAM_ENUMERATION("Extensions.DisabledUIUserResponse",
                            user_response_,
                            EXTENSION_DISABLED_UI_BUCKET_BOUNDARY);
}

GlobalError::Severity ExtensionDisabledGlobalError::GetSeverity() {
  return SEVERITY_LOW;
}

bool ExtensionDisabledGlobalError::HasMenuItem() {
  return true;
}

int ExtensionDisabledGlobalError::MenuItemCommandID() {
  return menu_command_id_;
}

base::string16 ExtensionDisabledGlobalError::MenuItemLabel() {
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_DISABLED_ERROR_TITLE,
                                    base::UTF8ToUTF16(extension_->name()));
}

void ExtensionDisabledGlobalError::ExecuteMenuItem(Browser* browser) {
  ShowBubbleView(browser);
}

gfx::Image ExtensionDisabledGlobalError::GetBubbleViewIcon() {
  return icon_;
}

base::string16 ExtensionDisabledGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_DISABLED_ERROR_TITLE,
                                    base::UTF8ToUTF16(extension_->name()));
}

std::vector<base::string16>
ExtensionDisabledGlobalError::GetBubbleViewMessages() {
  std::vector<base::string16> messages;
  messages.push_back(l10n_util::GetStringFUTF16(
      extension_->is_app() ?
      IDS_APP_DISABLED_ERROR_LABEL : IDS_EXTENSION_DISABLED_ERROR_LABEL,
      base::UTF8ToUTF16(extension_->name())));
  messages.push_back(l10n_util::GetStringUTF16(
      IDS_EXTENSION_PROMPT_WILL_NOW_HAVE_ACCESS_TO));
  std::vector<base::string16> permission_warnings =
      extensions::PermissionMessageProvider::Get()->GetWarningMessages(
          extension_->GetActivePermissions(), extension_->GetType());
  for (size_t i = 0; i < permission_warnings.size(); ++i) {
    messages.push_back(l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PERMISSION_LINE, permission_warnings[i]));
  }
  return messages;
}

base::string16 ExtensionDisabledGlobalError::GetBubbleViewAcceptButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_RE_ENABLE_BUTTON);
}

base::string16 ExtensionDisabledGlobalError::GetBubbleViewCancelButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNINSTALL);
}

void ExtensionDisabledGlobalError::OnBubbleViewDidClose(Browser* browser) {
}

void ExtensionDisabledGlobalError::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  // Delay extension reenabling so this bubble closes properly.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&ExtensionService::GrantPermissionsAndEnableExtension,
                 service_->AsWeakPtr(), extension_));
}

void ExtensionDisabledGlobalError::BubbleViewCancelButtonPressed(
    Browser* browser) {
#if !defined(OS_ANDROID)
  uninstall_dialog_.reset(
      ExtensionUninstallDialog::Create(service_->profile(), browser, this));
  // Delay showing the uninstall dialog, so that this function returns
  // immediately, to close the bubble properly. See crbug.com/121544.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&ExtensionUninstallDialog::ConfirmUninstall,
                 uninstall_dialog_->AsWeakPtr(), extension_));
#endif  // !defined(OS_ANDROID)
}

void ExtensionDisabledGlobalError::ExtensionUninstallAccepted() {
  service_->UninstallExtension(extension_->id(), false, NULL);
}

void ExtensionDisabledGlobalError::ExtensionUninstallCanceled() {
  // Nothing happens, and the error is still there.
}

void ExtensionDisabledGlobalError::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // The error is invalidated if the extension has been loaded or removed.
  DCHECK(type == chrome::NOTIFICATION_EXTENSION_LOADED ||
         type == chrome::NOTIFICATION_EXTENSION_REMOVED);
  const Extension* extension = content::Details<const Extension>(details).ptr();
  if (extension != extension_)
    return;
  GlobalErrorServiceFactory::GetForProfile(service_->profile())->
      RemoveGlobalError(this);

  if (type == chrome::NOTIFICATION_EXTENSION_LOADED)
    user_response_ = REENABLE;
  else if (type == chrome::NOTIFICATION_EXTENSION_REMOVED)
    user_response_ = UNINSTALL;
  delete this;
}

// Globals --------------------------------------------------------------------

namespace extensions {

void AddExtensionDisabledErrorWithIcon(base::WeakPtr<ExtensionService> service,
                                       const std::string& extension_id,
                                       const gfx::Image& icon) {
  if (!service.get())
    return;
  const Extension* extension = service->GetInstalledExtension(extension_id);
  if (extension) {
    GlobalErrorServiceFactory::GetForProfile(service->profile())
        ->AddGlobalError(
              new ExtensionDisabledGlobalError(service.get(), extension, icon));
  }
}

void AddExtensionDisabledError(ExtensionService* service,
                               const Extension* extension) {
  // Do not display notifications for ephemeral apps that have been disabled.
  // Instead, a prompt will be shown the next time the app is launched.
  if (extension->is_ephemeral())
    return;

  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      extension, kIconSize, ExtensionIconSet::MATCH_BIGGER);
  gfx::Size size(kIconSize, kIconSize);
  ImageLoader::Get(service->profile())->LoadImageAsync(
      extension, image, size,
      base::Bind(&AddExtensionDisabledErrorWithIcon,
                 service->AsWeakPtr(), extension->id()));
}

void ShowExtensionDisabledDialog(ExtensionService* service,
                                 content::WebContents* web_contents,
                                 const Extension* extension) {
  scoped_ptr<ExtensionInstallPrompt> install_ui(
      new ExtensionInstallPrompt(web_contents));
  // This object manages its own lifetime.
  new ExtensionDisabledDialogDelegate(service, install_ui.Pass(), extension);
}

}  // namespace extensions
