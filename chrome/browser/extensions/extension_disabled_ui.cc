// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_disabled_ui.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_install_error_menu_item_id_provider.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"

using extensions::Extension;
using extensions::PermissionMessage;
using extensions::PermissionMessages;

namespace {

static const int kIconSize = extension_misc::EXTENSION_ICON_SMALL;

}  // namespace

// ExtensionDisabledGlobalError -----------------------------------------------

class ExtensionDisabledGlobalError
    : public GlobalErrorWithStandardBubble,
      public content::NotificationObserver,
      public extensions::ExtensionUninstallDialog::Delegate,
      public extensions::ExtensionRegistryObserver {
 public:
  ExtensionDisabledGlobalError(ExtensionService* service,
                               const Extension* extension,
                               bool is_remote_install,
                               const gfx::Image& icon);
  ~ExtensionDisabledGlobalError() override;

  // GlobalError:
  Severity GetSeverity() override;
  bool HasMenuItem() override;
  int MenuItemCommandID() override;
  base::string16 MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;
  gfx::Image GetBubbleViewIcon() override;
  base::string16 GetBubbleViewTitle() override;
  std::vector<base::string16> GetBubbleViewMessages() override;
  base::string16 GetBubbleViewAcceptButtonLabel() override;
  base::string16 GetBubbleViewCancelButtonLabel() override;
  void OnBubbleViewDidClose(Browser* browser) override;
  void BubbleViewAcceptButtonPressed(Browser* browser) override;
  void BubbleViewCancelButtonPressed(Browser* browser) override;
  bool ShouldCloseOnDeactivate() const override;
  bool ShouldShowCloseButton() const override;

  // ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const base::string16& error) override;

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  void RemoveGlobalError();

  ExtensionService* service_;
  const Extension* extension_;
  bool is_remote_install_;
  gfx::Image icon_;

  // How the user responded to the error; used for metrics.
  enum UserResponse {
    IGNORED,
    REENABLE,
    UNINSTALL,
    EXTENSION_DISABLED_UI_BUCKET_BOUNDARY
  };
  UserResponse user_response_;

  std::unique_ptr<extensions::ExtensionUninstallDialog> uninstall_dialog_;

  // Helper to get menu command ID assigned for this extension's error.
  extensions::ExtensionInstallErrorMenuItemIdProvider id_provider_;

  content::NotificationRegistrar registrar_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver> registry_observer_;
};

// TODO(yoz): create error at startup for disabled extensions.
ExtensionDisabledGlobalError::ExtensionDisabledGlobalError(
    ExtensionService* service,
    const Extension* extension,
    bool is_remote_install,
    const gfx::Image& icon)
    : service_(service),
      extension_(extension),
      is_remote_install_(is_remote_install),
      icon_(icon),
      user_response_(IGNORED),
      registry_observer_(this) {
  if (icon_.IsEmpty()) {
    icon_ = gfx::Image(
        gfx::ImageSkiaOperations::CreateResizedImage(
            extension_->is_app() ?
                extensions::util::GetDefaultAppIcon() :
                extensions::util::GetDefaultExtensionIcon(),
            skia::ImageOperations::RESIZE_BEST,
            gfx::Size(kIconSize, kIconSize)));
  }
  registry_observer_.Add(
      extensions::ExtensionRegistry::Get(service->profile()));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_REMOVED,
                 content::Source<Profile>(service->profile()));
}

ExtensionDisabledGlobalError::~ExtensionDisabledGlobalError() {
  if (is_remote_install_) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.DisabledUIUserResponseRemoteInstall",
                              user_response_,
                              EXTENSION_DISABLED_UI_BUCKET_BOUNDARY);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Extensions.DisabledUIUserResponse",
                              user_response_,
                              EXTENSION_DISABLED_UI_BUCKET_BOUNDARY);
  }
}

GlobalError::Severity ExtensionDisabledGlobalError::GetSeverity() {
  return SEVERITY_LOW;
}

bool ExtensionDisabledGlobalError::HasMenuItem() {
  return true;
}

int ExtensionDisabledGlobalError::MenuItemCommandID() {
  return id_provider_.menu_command_id();
}

base::string16 ExtensionDisabledGlobalError::MenuItemLabel() {
  std::string extension_name = extension_->name();
  // Ampersands need to be escaped to avoid being treated like
  // mnemonics in the menu.
  base::ReplaceChars(extension_name, "&", "&&", &extension_name);

  if (is_remote_install_) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_DISABLED_REMOTE_INSTALL_ERROR_TITLE,
        base::UTF8ToUTF16(extension_name));
  } else {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_DISABLED_ERROR_TITLE,
                                      base::UTF8ToUTF16(extension_name));
  }
}

void ExtensionDisabledGlobalError::ExecuteMenuItem(Browser* browser) {
  ShowBubbleView(browser);
}

gfx::Image ExtensionDisabledGlobalError::GetBubbleViewIcon() {
  return icon_;
}

base::string16 ExtensionDisabledGlobalError::GetBubbleViewTitle() {
  if (is_remote_install_) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_DISABLED_REMOTE_INSTALL_ERROR_TITLE,
        base::UTF8ToUTF16(extension_->name()));
  } else {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_DISABLED_ERROR_TITLE,
                                      base::UTF8ToUTF16(extension_->name()));
  }
}

std::vector<base::string16>
ExtensionDisabledGlobalError::GetBubbleViewMessages() {
  std::vector<base::string16> messages;
  PermissionMessages permission_warnings =
      extension_->permissions_data()->GetPermissionMessages();
  if (is_remote_install_) {
    if (!permission_warnings.empty())
      messages.push_back(
          l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO));
  } else {
    // TODO(treib): If NeedCustodianApprovalForPermissionIncrease, add an extra
    // message for supervised users. crbug.com/461261
    messages.push_back(l10n_util::GetStringFUTF16(
        extension_->is_app() ? IDS_APP_DISABLED_ERROR_LABEL
                             : IDS_EXTENSION_DISABLED_ERROR_LABEL,
        base::UTF8ToUTF16(extension_->name())));
    messages.push_back(l10n_util::GetStringUTF16(
        IDS_EXTENSION_PROMPT_WILL_NOW_HAVE_ACCESS_TO));
  }
  for (const PermissionMessage& msg : permission_warnings) {
    messages.push_back(l10n_util::GetStringFUTF16(IDS_EXTENSION_PERMISSION_LINE,
                                                  msg.message()));
  }
  return messages;
}

base::string16 ExtensionDisabledGlobalError::GetBubbleViewAcceptButtonLabel() {
  if (extensions::util::IsExtensionSupervised(extension_,
                                              service_->profile())) {
    // TODO(treib): Probably use a new string here once we get UX design.
    // For now, just use "OK". crbug.com/461261
    return l10n_util::GetStringUTF16(IDS_OK);
  }
  if (is_remote_install_) {
    return l10n_util::GetStringUTF16(
        extension_->is_app()
            ? IDS_EXTENSION_PROMPT_REMOTE_INSTALL_BUTTON_APP
            : IDS_EXTENSION_PROMPT_REMOTE_INSTALL_BUTTON_EXTENSION);
  }
  return l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_RE_ENABLE_BUTTON);
}

base::string16 ExtensionDisabledGlobalError::GetBubbleViewCancelButtonLabel() {
  if (extensions::util::IsExtensionSupervised(extension_,
                                              service_->profile())) {
    // The supervised user can't approve the update, and hence there is no
    // "cancel" button. Return an empty string such that the "cancel" button
    // is not shown in the dialog.
    return base::string16();
  }
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNINSTALL);
}

void ExtensionDisabledGlobalError::OnBubbleViewDidClose(Browser* browser) {
}

void ExtensionDisabledGlobalError::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  if (extensions::util::IsExtensionSupervised(extension_,
                                              service_->profile())) {
    return;
  }
  // Delay extension reenabling so this bubble closes properly.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ExtensionService::GrantPermissionsAndEnableExtension,
                 service_->AsWeakPtr(), extension_));
}

void ExtensionDisabledGlobalError::BubbleViewCancelButtonPressed(
    Browser* browser) {
  // For custodian-installed extensions, this button should not exist because
  // there is only an "OK" button.
  // Supervised users may never remove custodian-installed extensions.
  DCHECK(!extensions::util::IsExtensionSupervised(extension_,
                                                  service_->profile()));

  uninstall_dialog_.reset(extensions::ExtensionUninstallDialog::Create(
      service_->profile(), browser->window()->GetNativeWindow(), this));
  // Delay showing the uninstall dialog, so that this function returns
  // immediately, to close the bubble properly. See crbug.com/121544.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&extensions::ExtensionUninstallDialog::ConfirmUninstall,
                 uninstall_dialog_->AsWeakPtr(), extension_,
                 extensions::UNINSTALL_REASON_EXTENSION_DISABLED,
                 extensions::UNINSTALL_SOURCE_PERMISSIONS_INCREASE));
}

bool ExtensionDisabledGlobalError::ShouldCloseOnDeactivate() const {
  // Since this indicates that an extension was disabled, we should definitely
  // have the user acknowledge it, rather than having the bubble disappear when
  // a new window pops up.
  return false;
}

bool ExtensionDisabledGlobalError::ShouldShowCloseButton() const {
  // As we don't close the bubble on deactivation (see ShouldCloseOnDeactivate),
  // we add a close button so the user doesn't *need* to act right away.
  // If the bubble is closed, the error remains in the wrench menu and the user
  // can address it later.
  return true;
}

void ExtensionDisabledGlobalError::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const base::string16& error) {
  // No need to do anything.
}

void ExtensionDisabledGlobalError::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // The error is invalidated if the extension has been loaded or removed.
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_REMOVED, type);
  const Extension* extension = content::Details<const Extension>(details).ptr();
  if (extension != extension_)
    return;
  user_response_ = UNINSTALL;
  RemoveGlobalError();
}

void ExtensionDisabledGlobalError::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension != extension_)
    return;
  user_response_ = REENABLE;
  RemoveGlobalError();
}

void ExtensionDisabledGlobalError::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  DCHECK_EQ(extensions::ExtensionRegistry::Get(service_->profile()), registry);
  registry_observer_.RemoveAll();
}

void ExtensionDisabledGlobalError::RemoveGlobalError() {
  std::unique_ptr<GlobalError> ptr =
      GlobalErrorServiceFactory::GetForProfile(service_->profile())
          ->RemoveGlobalError(this);
  registrar_.RemoveAll();
  registry_observer_.RemoveAll();
  // Delete this object after any running tasks, so that the extension dialog
  // still has it as a delegate to finish the current tasks.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, ptr.release());
}

// Globals --------------------------------------------------------------------

namespace extensions {

void AddExtensionDisabledErrorWithIcon(base::WeakPtr<ExtensionService> service,
                                       const std::string& extension_id,
                                       bool is_remote_install,
                                       const gfx::Image& icon) {
  if (!service.get())
    return;
  const Extension* extension = service->GetInstalledExtension(extension_id);
  if (extension) {
    GlobalErrorServiceFactory::GetForProfile(service->profile())
        ->AddGlobalError(base::MakeUnique<ExtensionDisabledGlobalError>(
            service.get(), extension, is_remote_install, icon));
  }
}

void AddExtensionDisabledError(ExtensionService* service,
                               const Extension* extension,
                               bool is_remote_install) {
  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      extension, kIconSize, ExtensionIconSet::MATCH_BIGGER);
  gfx::Size size(kIconSize, kIconSize);
  ImageLoader::Get(service->profile())
      ->LoadImageAsync(extension,
                       image,
                       size,
                       base::Bind(&AddExtensionDisabledErrorWithIcon,
                                  service->AsWeakPtr(),
                                  extension->id(),
                                  is_remote_install));
}

}  // namespace extensions
