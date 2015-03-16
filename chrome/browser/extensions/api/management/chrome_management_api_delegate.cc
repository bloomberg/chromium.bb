// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/chrome_requirements_checker.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_json_parser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "components/favicon/core/browser/favicon_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace {

class ManagementSetEnabledFunctionInstallPromptDelegate
    : public ExtensionInstallPrompt::Delegate,
      public extensions::InstallPromptDelegate {
 public:
  ManagementSetEnabledFunctionInstallPromptDelegate(
      extensions::ManagementSetEnabledFunction* function,
      const extensions::Extension* extension)
      : function_(function) {
    install_prompt_.reset(
        new ExtensionInstallPrompt(function->GetSenderWebContents()));
    install_prompt_->ConfirmReEnable(this, extension);
  }
  ~ManagementSetEnabledFunctionInstallPromptDelegate() override {}

 protected:
  // ExtensionInstallPrompt::Delegate.
  void InstallUIProceed() override { function_->InstallUIProceed(); }
  void InstallUIAbort(bool user_initiated) override {
    function_->InstallUIAbort(user_initiated);
  }

 private:
  extensions::ManagementSetEnabledFunction* function_;

  // Used for prompting to re-enable items with permissions escalation updates.
  scoped_ptr<ExtensionInstallPrompt> install_prompt_;

  DISALLOW_COPY_AND_ASSIGN(ManagementSetEnabledFunctionInstallPromptDelegate);
};

class ManagementUninstallFunctionUninstallDialogDelegate
    : public extensions::ExtensionUninstallDialog::Delegate,
      public extensions::UninstallDialogDelegate {
 public:
  ManagementUninstallFunctionUninstallDialogDelegate(
      extensions::ManagementUninstallFunctionBase* function,
      const extensions::Extension* target_extension,
      bool show_programmatic_uninstall_ui)
      : function_(function) {
    content::WebContents* web_contents = function->GetSenderWebContents();
    extension_uninstall_dialog_.reset(
        extensions::ExtensionUninstallDialog::Create(
            Profile::FromBrowserContext(function->browser_context()),
            web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr,
            this));
    if (show_programmatic_uninstall_ui) {
      extension_uninstall_dialog_->ConfirmProgrammaticUninstall(
          target_extension, function->extension());
    } else {
      extension_uninstall_dialog_->ConfirmUninstall(target_extension);
    }
  }
  ~ManagementUninstallFunctionUninstallDialogDelegate() override {}

  // ExtensionUninstallDialog::Delegate implementation.
  void ExtensionUninstallAccepted() override {
    function_->ExtensionUninstallAccepted();
  }
  void ExtensionUninstallCanceled() override {
    function_->ExtensionUninstallCanceled();
  }

 private:
  extensions::ManagementUninstallFunctionBase* function_;
  scoped_ptr<extensions::ExtensionUninstallDialog> extension_uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ManagementUninstallFunctionUninstallDialogDelegate);
};

class ChromeAppForLinkDelegate : public extensions::AppForLinkDelegate {
 public:
  ChromeAppForLinkDelegate() {}
  ~ChromeAppForLinkDelegate() override {}

  void OnFaviconForApp(
      extensions::ManagementGenerateAppForLinkFunction* function,
      content::BrowserContext* context,
      const std::string& title,
      const GURL& launch_url,
      const favicon_base::FaviconImageResult& image_result) {
    WebApplicationInfo web_app;
    web_app.title = base::UTF8ToUTF16(title);
    web_app.app_url = launch_url;

    if (!image_result.image.IsEmpty()) {
      WebApplicationInfo::IconInfo icon;
      icon.data = image_result.image.AsBitmap();
      icon.width = icon.data.width();
      icon.height = icon.data.height();
      web_app.icons.push_back(icon);
    }

    bookmark_app_helper_.reset(new extensions::BookmarkAppHelper(
        Profile::FromBrowserContext(context), web_app, NULL));
    bookmark_app_helper_->Create(
        base::Bind(&extensions::ManagementGenerateAppForLinkFunction::
                       FinishCreateBookmarkApp,
                   function));
  }

  scoped_ptr<extensions::BookmarkAppHelper> bookmark_app_helper_;

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;
};

}  // namespace

ChromeManagementAPIDelegate::ChromeManagementAPIDelegate() {
}

ChromeManagementAPIDelegate::~ChromeManagementAPIDelegate() {
}

bool ChromeManagementAPIDelegate::LaunchAppFunctionDelegate(
    const extensions::Extension* extension,
    content::BrowserContext* context) const {
  // Look at prefs to find the right launch container.
  // If the user has not set a preference, the default launch value will be
  // returned.
  extensions::LaunchContainer launch_container =
      GetLaunchContainer(extensions::ExtensionPrefs::Get(context), extension);
  OpenApplication(AppLaunchParams(
      Profile::FromBrowserContext(context), extension, launch_container,
      NEW_FOREGROUND_TAB, extensions::SOURCE_MANAGEMENT_API));
  extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_EXTENSION_API,
                                  extension->GetType());

  return true;
}

GURL ChromeManagementAPIDelegate::GetFullLaunchURL(
    const extensions::Extension* extension) const {
  return extensions::AppLaunchInfo::GetFullLaunchURL(extension);
}

extensions::LaunchType ChromeManagementAPIDelegate::GetLaunchType(
    const extensions::ExtensionPrefs* prefs,
    const extensions::Extension* extension) const {
  return extensions::GetLaunchType(prefs, extension);
}

void ChromeManagementAPIDelegate::
    GetPermissionWarningsByManifestFunctionDelegate(
        extensions::ManagementGetPermissionWarningsByManifestFunction* function,
        const std::string& manifest_str) const {
  scoped_refptr<SafeJsonParser> parser(new SafeJsonParser(
      manifest_str,
      base::Bind(
          &extensions::ManagementGetPermissionWarningsByManifestFunction::
              OnParseSuccess,
          function),
      base::Bind(
          &extensions::ManagementGetPermissionWarningsByManifestFunction::
              OnParseFailure,
          function)));
  parser->Start();
}

scoped_ptr<extensions::InstallPromptDelegate>
ChromeManagementAPIDelegate::SetEnabledFunctionDelegate(
    extensions::ManagementSetEnabledFunction* function,
    const extensions::Extension* extension) const {
  return scoped_ptr<ManagementSetEnabledFunctionInstallPromptDelegate>(
      new ManagementSetEnabledFunctionInstallPromptDelegate(function,
                                                            extension));
}

scoped_ptr<extensions::RequirementsChecker>
ChromeManagementAPIDelegate::CreateRequirementsChecker() const {
  return make_scoped_ptr(new extensions::ChromeRequirementsChecker());
}

scoped_ptr<extensions::UninstallDialogDelegate>
ChromeManagementAPIDelegate::UninstallFunctionDelegate(
    extensions::ManagementUninstallFunctionBase* function,
    const extensions::Extension* target_extension,
    bool show_programmatic_uninstall_ui) const {
  return scoped_ptr<extensions::UninstallDialogDelegate>(
      new ManagementUninstallFunctionUninstallDialogDelegate(
          function, target_extension, show_programmatic_uninstall_ui));
}

bool ChromeManagementAPIDelegate::CreateAppShortcutFunctionDelegate(
    extensions::ManagementCreateAppShortcutFunction* function,
    const extensions::Extension* extension) const {
  Browser* browser = chrome::FindBrowserWithProfile(
      Profile::FromBrowserContext(function->browser_context()),
      chrome::HOST_DESKTOP_TYPE_NATIVE);
  if (!browser) {
    // Shouldn't happen if we have user gesture.
    function->SetError(
        extension_management_api_constants::kNoBrowserToCreateShortcut);
    return false;
  }

  chrome::ShowCreateChromeAppShortcutsDialog(
      browser->window()->GetNativeWindow(), browser->profile(), extension,
      base::Bind(&extensions::ManagementCreateAppShortcutFunction::
                     OnCloseShortcutPrompt,
                 function));

  return true;
}

scoped_ptr<extensions::AppForLinkDelegate>
ChromeManagementAPIDelegate::GenerateAppForLinkFunctionDelegate(
    extensions::ManagementGenerateAppForLinkFunction* function,
    content::BrowserContext* context,
    const std::string& title,
    const GURL& launch_url) const {
  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context), ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(favicon_service);

  ChromeAppForLinkDelegate* delegate = new ChromeAppForLinkDelegate;

  favicon_service->GetFaviconImageForPageURL(
      launch_url,
      base::Bind(
          &ChromeAppForLinkDelegate::OnFaviconForApp,
          base::Unretained(delegate),
          scoped_refptr<extensions::ManagementGenerateAppForLinkFunction>(
              function),
          context, title, launch_url),
      &delegate->cancelable_task_tracker_);

  return scoped_ptr<extensions::AppForLinkDelegate>(delegate);
}

bool ChromeManagementAPIDelegate::IsNewBookmarkAppsEnabled() const {
  return extensions::util::IsNewBookmarkAppsEnabled();
}

void ChromeManagementAPIDelegate::EnableExtension(
    content::BrowserContext* context,
    const std::string& extension_id) const {
  extensions::ExtensionSystem::Get(context)
      ->extension_service()
      ->EnableExtension(extension_id);
}

void ChromeManagementAPIDelegate::DisableExtension(
    content::BrowserContext* context,
    const std::string& extension_id,
    extensions::Extension::DisableReason disable_reason) const {
  extensions::ExtensionSystem::Get(context)
      ->extension_service()
      ->DisableExtension(extension_id, disable_reason);
}

bool ChromeManagementAPIDelegate::UninstallExtension(
    content::BrowserContext* context,
    const std::string& transient_extension_id,
    extensions::UninstallReason reason,
    const base::Closure& deletion_done_callback,
    base::string16* error) const {
  return extensions::ExtensionSystem::Get(context)
      ->extension_service()
      ->UninstallExtension(transient_extension_id, reason,
                           deletion_done_callback, error);
}

void ChromeManagementAPIDelegate::SetLaunchType(
    content::BrowserContext* context,
    const std::string& extension_id,
    extensions::LaunchType launch_type) const {
  extensions::SetLaunchType(context, extension_id, launch_type);
}

GURL ChromeManagementAPIDelegate::GetIconURL(
    const extensions::Extension* extension,
    int icon_size,
    ExtensionIconSet::MatchType match,
    bool grayscale,
    bool* exists) const {
  return extensions::ExtensionIconSource::GetIconURL(extension, icon_size,
                                                     match, grayscale, exists);
}
