// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/extension_settings_handler.h"

#include "base/auto_reset.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_disabled_infobar_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/extensions/extension_warning_set.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/ui/webui/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

bool ShouldShowExtension(const Extension* extension) {
  // Don't show themes since this page's UI isn't really useful for themes.
  if (extension->is_theme())
    return false;

  // Don't show component extensions because they are only extensions as an
  // implementation detail of Chrome.
  if (extension->location() == Extension::COMPONENT &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kShowComponentExtensionOptions))
    return false;

  // Always show unpacked extensions and apps.
  if (extension->location() == Extension::LOAD)
    return true;

  // Unless they are unpacked, never show hosted apps.
  if (extension->is_hosted_app())
    return false;

  return true;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// ExtensionSettingsHandler
//
///////////////////////////////////////////////////////////////////////////////

ExtensionSettingsHandler::ExtensionSettingsHandler()
    : extension_service_(NULL),
      ignore_notifications_(false),
      deleting_rvh_(NULL),
      registered_for_notifications_(false) {
}

ExtensionSettingsHandler::~ExtensionSettingsHandler() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (load_extension_dialog_.get())
    load_extension_dialog_->ListenerDestroyed();

  registrar_.RemoveAll();
}

// static
void ExtensionSettingsHandler::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kExtensionsUIDeveloperMode,
                             false,
                             PrefService::SYNCABLE_PREF);
}

void ExtensionSettingsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("extensionSettingsRequestExtensionsData",
      base::Bind(&ExtensionSettingsHandler::HandleRequestExtensionsData,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsToggleDeveloperMode",
      base::Bind(&ExtensionSettingsHandler::HandleToggleDeveloperMode,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsInspect",
      base::Bind(&ExtensionSettingsHandler::HandleInspectMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsReload",
      base::Bind(&ExtensionSettingsHandler::HandleReloadMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsEnable",
      base::Bind(&ExtensionSettingsHandler::HandleEnableMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsEnableIncognito",
      base::Bind(&ExtensionSettingsHandler::HandleEnableIncognitoMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsAllowFileAccess",
      base::Bind(&ExtensionSettingsHandler::HandleAllowFileAccessMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsUninstall",
      base::Bind(&ExtensionSettingsHandler::HandleUninstallMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsOptions",
      base::Bind(&ExtensionSettingsHandler::HandleOptionsMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsShowButton",
      base::Bind(&ExtensionSettingsHandler::HandleShowButtonMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsLoad",
      base::Bind(&ExtensionSettingsHandler::HandleLoadMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsAutoupdate",
      base::Bind(&ExtensionSettingsHandler::HandleAutoUpdateMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("extensionSettingsSelectFilePath",
      base::Bind(&ExtensionSettingsHandler::HandleSelectFilePathMessage,
                 base::Unretained(this)));
}

void ExtensionSettingsHandler::HandleRequestExtensionsData(
    const ListValue* args) {
  DictionaryValue results;

  // Add the extensions to the results structure.
  ListValue *extensions_list = new ListValue();

  ExtensionWarningSet* warnings = extension_service_->extension_warnings();

  const ExtensionList* extensions = extension_service_->extensions();
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    if (ShouldShowExtension(*extension)) {
      extensions_list->Append(CreateExtensionDetailValue(
          extension_service_,
          *extension,
          GetActivePagesForExtension(*extension),
          warnings,
          true, false));  // enabled, terminated
    }
  }
  extensions = extension_service_->disabled_extensions();
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    if (ShouldShowExtension(*extension)) {
      extensions_list->Append(CreateExtensionDetailValue(
          extension_service_,
          *extension,
          GetActivePagesForExtension(*extension),
          warnings,
          false, false));  // enabled, terminated
    }
  }
  extensions = extension_service_->terminated_extensions();
  std::vector<ExtensionPage> empty_pages;
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    if (ShouldShowExtension(*extension)) {
      extensions_list->Append(CreateExtensionDetailValue(
          extension_service_,
          *extension,
          empty_pages,  // Terminated process has no active pages.
          warnings,
          false, true));  // enabled, terminated
    }
  }
  results.Set("extensions", extensions_list);

  Profile* profile = Profile::FromWebUI(web_ui_);
  bool developer_mode =
      profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  results.SetBoolean("developerMode", developer_mode);

  web_ui_->CallJavascriptFunction("ExtensionSettings.returnExtensionsData",
                                  results);

  MaybeRegisterForNotifications();
}

void ExtensionSettingsHandler::MaybeRegisterForNotifications() {
  if (registered_for_notifications_)
    return;

  registered_for_notifications_  = true;
  Profile* profile = Profile::FromWebUI(web_ui_);

  // Register for notifications that we need to reload the page.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_WARNING_CHANGED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_VIEW_HOST_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
      content::Source<ExtensionPrefs>(profile->GetExtensionService()->
                             extension_prefs()));
}

ExtensionUninstallDialog*
ExtensionSettingsHandler::GetExtensionUninstallDialog() {
  if (!extension_uninstall_dialog_.get()) {
    extension_uninstall_dialog_.reset(
        ExtensionUninstallDialog::Create(Profile::FromWebUI(web_ui_), this));
  }
  return extension_uninstall_dialog_.get();
}

void ExtensionSettingsHandler::HandleToggleDeveloperMode(
      const ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  bool developer_mode =
      profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  profile->GetPrefs()->SetBoolean(
      prefs::kExtensionsUIDeveloperMode, !developer_mode);
  HandleRequestExtensionsData(NULL);
}

void ExtensionSettingsHandler::HandleInspectMessage(const ListValue* args) {
  std::string render_process_id_str;
  std::string render_view_id_str;
  int render_process_id;
  int render_view_id;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &render_process_id_str));
  CHECK(args->GetString(1, &render_view_id_str));
  CHECK(base::StringToInt(render_process_id_str, &render_process_id));
  CHECK(base::StringToInt(render_view_id_str, &render_view_id));
  RenderViewHost* host = RenderViewHost::FromID(render_process_id,
                                                render_view_id);
  if (!host) {
    // This can happen if the host has gone away since the page was displayed.
    return;
  }

  DevToolsWindow::OpenDevToolsWindow(host);
}

void ExtensionSettingsHandler::HandleReloadMessage(const ListValue* args) {
  std::string extension_id = UTF16ToUTF8(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  extension_service_->ReloadExtension(extension_id);
}

void ExtensionSettingsHandler::HandleEnableMessage(const ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string extension_id, enable_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &enable_str));

  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, true);
  if (!Extension::UserMayDisable(extension->location())) {
    LOG(ERROR) << "Attempt to enable an extension that is non-usermanagable was"
               << "made. Extension id: " << extension->id();
    return;
  }

  if (enable_str == "true") {
    ExtensionPrefs* prefs = extension_service_->extension_prefs();
    if (prefs->DidExtensionEscalatePermissions(extension_id)) {
      ShowExtensionDisabledDialog(extension_service_,
                                  Profile::FromWebUI(web_ui_), extension);
    } else {
      extension_service_->EnableExtension(extension_id);
    }
  } else {
    extension_service_->DisableExtension(extension_id);
  }
}

void ExtensionSettingsHandler::HandleEnableIncognitoMessage(
    const ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string extension_id, enable_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &enable_str));
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, true);
  DCHECK(extension);

  // Flipping the incognito bit will generate unload/load notifications for the
  // extension, but we don't want to reload the page, because a) we've already
  // updated the UI to reflect the change, and b) we want the yellow warning
  // text to stay until the user has left the page.
  //
  // TODO(aa): This creates crapiness in some cases. For example, in a main
  // window, when toggling this, the browser action will flicker because it gets
  // unloaded, then reloaded. It would be better to have a dedicated
  // notification for this case.
  //
  // Bug: http://crbug.com/41384
  AutoReset<bool> auto_reset_ignore_notifications(&ignore_notifications_, true);
  extension_service_->SetIsIncognitoEnabled(extension->id(),
                                            enable_str == "true");
}

void ExtensionSettingsHandler::HandleAllowFileAccessMessage(
    const ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string extension_id, allow_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &allow_str));
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, true);
  DCHECK(extension);

  if (!Extension::UserMayDisable(extension->location())) {
    LOG(ERROR) << "Attempt to change allow file access of an extension that is "
               << "non-usermanagable was made. Extension id : "
               << extension->id();
    return;
  }

  extension_service_->SetAllowFileAccess(extension, allow_str == "true");
}

void ExtensionSettingsHandler::HandleUninstallMessage(const ListValue* args) {
  std::string extension_id = UTF16ToUTF8(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id, true);
  if (!extension)
    extension = extension_service_->GetTerminatedExtension(extension_id);
  if (!extension)
    return;

  if (!Extension::UserMayDisable(extension->location())) {
    LOG(ERROR) << "Attempt to uninstall an extension that is non-usermanagable "
               << "was made. Extension id : " << extension->id();
    return;
  }

  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.

  extension_id_prompting_ = extension_id;

  GetExtensionUninstallDialog()->ConfirmUninstall(extension);
}

void ExtensionSettingsHandler::ExtensionUninstallAccepted() {
  DCHECK(!extension_id_prompting_.empty());

  bool was_terminated = false;

  // The extension can be uninstalled in another window while the UI was
  // showing. Do nothing in that case.
  const Extension* extension =
      extension_service_->GetExtensionById(extension_id_prompting_, true);
  if (!extension) {
    extension = extension_service_->GetTerminatedExtension(
        extension_id_prompting_);
    was_terminated = true;
  }
  if (!extension)
    return;

  extension_service_->UninstallExtension(extension_id_prompting_,
                                         false,  // External uninstall.
                                         NULL);  // Error.
  extension_id_prompting_ = "";

  // There will be no EXTENSION_UNLOADED notification for terminated
  // extensions as they were already unloaded.
  if (was_terminated)
    HandleRequestExtensionsData(NULL);
}

void ExtensionSettingsHandler::ExtensionUninstallCanceled() {
  extension_id_prompting_ = "";
}

void ExtensionSettingsHandler::HandleOptionsMessage(const ListValue* args) {
  const Extension* extension = GetExtension(args);
  if (!extension || extension->options_url().is_empty())
    return;
  Profile::FromWebUI(web_ui_)->GetExtensionProcessManager()->OpenOptionsPage(
      extension, NULL);
}

void ExtensionSettingsHandler::HandleShowButtonMessage(const ListValue* args) {
  const Extension* extension = GetExtension(args);
  extension_service_->SetBrowserActionVisibility(extension, true);
}

void ExtensionSettingsHandler::HandleLoadMessage(const ListValue* args) {
  FilePath::StringType string_path;
  CHECK_EQ(1U, args->GetSize()) << args->GetSize();
  CHECK(args->GetString(0, &string_path));
  extensions::UnpackedInstaller::Create(extension_service_)->
      Load(FilePath(string_path));
}

void ExtensionSettingsHandler::ShowAlert(const std::string& message) {
  ListValue arguments;
  arguments.Append(Value::CreateStringValue(message));
  web_ui_->CallJavascriptFunction("alert", arguments);
}

void ExtensionSettingsHandler::HandleAutoUpdateMessage(const ListValue* args) {
  ExtensionUpdater* updater = extension_service_->updater();
  if (updater)
    updater->CheckNow();
}

void ExtensionSettingsHandler::HandleSelectFilePathMessage(
    const ListValue* args) {
  std::string select_type;
  std::string operation;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &select_type));
  CHECK(args->GetString(1, &operation));

  SelectFileDialog::Type type = SelectFileDialog::SELECT_FOLDER;
  SelectFileDialog::FileTypeInfo info;
  int file_type_index = 0;
  if (select_type == "file")
    type = SelectFileDialog::SELECT_OPEN_FILE;

  string16 select_title;
  if (operation == "load") {
    select_title = l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);
  } else if (operation == "packRoot") {
    select_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_PACK_DIALOG_SELECT_ROOT);
  } else if (operation == "pem") {
    select_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_PACK_DIALOG_SELECT_KEY);
    info.extensions.push_back(std::vector<FilePath::StringType>());
        info.extensions.front().push_back(FILE_PATH_LITERAL("pem"));
        info.extension_description_overrides.push_back(
            l10n_util::GetStringUTF16(
                IDS_EXTENSION_PACK_DIALOG_KEY_FILE_TYPE_DESCRIPTION));
        info.include_all_files = true;
    file_type_index = 1;
  } else {
    NOTREACHED();
    return;
  }

  load_extension_dialog_ = SelectFileDialog::Create(this);
  load_extension_dialog_->SelectFile(type, select_title, FilePath(), &info,
      file_type_index, FILE_PATH_LITERAL(""), web_ui_->tab_contents(),
      web_ui_->tab_contents()->view()->GetTopLevelNativeWindow(), NULL);
}


void ExtensionSettingsHandler::FileSelected(const FilePath& path, int index,
                                            void* params) {
  // Add the extensions to the results structure.
  ListValue results;
  results.Append(Value::CreateStringValue(path.value()));
  web_ui_->CallJavascriptFunction("window.handleFilePathSelected", results);
}

void ExtensionSettingsHandler::MultiFilesSelected(
    const std::vector<FilePath>& files, void* params) {
  NOTREACHED();
}

void ExtensionSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "extensionSettings",
                IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE);

  localized_strings->SetString("extensionSettingsVisitWebsite",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VISIT_WEBSITE));

  localized_strings->SetString("extensionSettingsDeveloperMode",
    l10n_util::GetStringUTF16(IDS_EXTENSIONS_DEVELOPER_MODE_LINK));
  localized_strings->SetString("extensionSettingsNoExtensions",
    l10n_util::GetStringUTF16(IDS_EXTENSIONS_NONE_INSTALLED));
  localized_strings->SetString("extensionSettingsSuggestGallery",
      l10n_util::GetStringFUTF16(IDS_EXTENSIONS_NONE_INSTALLED_SUGGEST_GALLERY,
      ASCIIToUTF16("<a href='") +
          ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
              GURL(extension_urls::GetWebstoreLaunchURL())).spec()) +
      ASCIIToUTF16("'>"),
      ASCIIToUTF16("</a>")));
  localized_strings->SetString("extensionSettingsGetMoreExtensions",
      ASCIIToUTF16("<a href='") +
      ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
          GURL(extension_urls::GetWebstoreLaunchURL())).spec()) +
          ASCIIToUTF16("'>") +
          l10n_util::GetStringUTF16(IDS_GET_MORE_EXTENSIONS) +
      ASCIIToUTF16("</a>"));
  localized_strings->SetString("extensionSettingsExtensionId",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ID));
  localized_strings->SetString("extensionSettingsExtensionPath",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_PATH));
  localized_strings->SetString("extensionSettingsInspectViews",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSPECT_VIEWS));
  localized_strings->SetString("viewIncognito",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VIEW_INCOGNITO));
  localized_strings->SetString("extensionSettingsEnable",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE));
  localized_strings->SetString("extensionSettingsEnabled",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLED));
  localized_strings->SetString("extensionSettingsRemove",
    l10n_util::GetStringUTF16(IDS_EXTENSIONS_REMOVE));
  localized_strings->SetString("extensionSettingsEnableIncognito",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE_INCOGNITO));
  localized_strings->SetString("extensionSettingsAllowFileAccess",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ALLOW_FILE_ACCESS));
  localized_strings->SetString("extensionSettingsIncognitoWarning",
      l10n_util::GetStringFUTF16(IDS_EXTENSIONS_INCOGNITO_WARNING,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings->SetString("extensionSettingsReload",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_RELOAD));
  localized_strings->SetString("extensionSettingsOptions",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_OPTIONS));
  localized_strings->SetString("extensionSettingsPolicyControlled",
     l10n_util::GetStringUTF16(IDS_EXTENSIONS_POLICY_CONTROLLED));
  localized_strings->SetString("extensionSettingsShowButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_BUTTON));
  localized_strings->SetString("extensionSettingsLoadUnpackedButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_UNPACKED_BUTTON));
  localized_strings->SetString("extensionSettingsPackButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_PACK_BUTTON));
  localized_strings->SetString("extensionSettingsUpdateButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UPDATE_BUTTON));
  localized_strings->SetString("extensionSettingsCrashMessage",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_CRASHED_EXTENSION));
  localized_strings->SetString("extensionSettingsInDevelopment",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_IN_DEVELOPMENT));
  localized_strings->SetString("extensionSettingsWarningsTitle",
      l10n_util::GetStringUTF16(IDS_EXTENSION_WARNINGS_TITLE));
  localized_strings->SetString("extensionSettingsShowDetails",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));
  localized_strings->SetString("extensionSettingsHideDetails",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_DETAILS));
}

void ExtensionSettingsHandler::Initialize() {
}

WebUIMessageHandler* ExtensionSettingsHandler::Attach(WebUI* web_ui) {
  // Call through to superclass.
  WebUIMessageHandler* handler = OptionsPageUIHandler::Attach(web_ui);

  extension_service_ = Profile::FromWebUI(web_ui_)
      ->GetOriginalProfile()->GetExtensionService();

  // Return result from the superclass.
  return handler;
}

void ExtensionSettingsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  Profile* source_profile = NULL;
  switch (type) {
    // We listen for notifications that will result in the page being
    // repopulated with data twice for the same event in certain cases.
    // For instance, EXTENSION_LOADED & EXTENSION_HOST_CREATED because
    // we don't know about the views for an extension at EXTENSION_LOADED, but
    // if we only listen to EXTENSION_HOST_CREATED, we'll miss extensions
    // that don't have a process at startup.
    //
    // Doing it this way gets everything but causes the page to be rendered
    // more than we need. It doesn't seem to result in any noticeable flicker.
    case content::NOTIFICATION_RENDER_VIEW_HOST_DELETED:
      deleting_rvh_ = content::Source<RenderViewHost>(source).ptr();
      // Fall through.
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED:
      source_profile = Profile::FromBrowserContext(
          content::Source<RenderViewHost>(source)->site_instance()->
          browsing_instance()->browser_context());
      if (!profile->IsSameProfile(source_profile))
        return;
      MaybeUpdateAfterNotification();
      break;
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED:
      deleting_rvh_ = content::Details<BackgroundContents>(details)->
          tab_contents()->render_view_host();
      // Fall through.
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED:
    case chrome::NOTIFICATION_EXTENSION_HOST_CREATED:
      source_profile = content::Source<Profile>(source).ptr();
      if (!profile->IsSameProfile(source_profile))
          return;
      MaybeUpdateAfterNotification();
      break;
    case chrome::NOTIFICATION_EXTENSION_LOADED:
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
    case chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED:
    case chrome::NOTIFICATION_EXTENSION_WARNING_CHANGED:
    case chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED:
      MaybeUpdateAfterNotification();
      break;
    default:
      NOTREACHED();
  }
}

const Extension* ExtensionSettingsHandler::GetExtension(const ListValue* args) {
  std::string extension_id = UTF16ToUTF8(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  return extension_service_->GetExtensionById(extension_id, true);
}

void ExtensionSettingsHandler::MaybeUpdateAfterNotification() {
  TabContents* contents = web_ui_->tab_contents();
  if (!ignore_notifications_ && contents && contents->render_view_host())
    HandleRequestExtensionsData(NULL);
  deleting_rvh_ = NULL;
}

// Static
DictionaryValue* ExtensionSettingsHandler::CreateExtensionDetailValue(
    ExtensionService* service, const Extension* extension,
    const std::vector<ExtensionPage>& pages,
    const ExtensionWarningSet* warnings_set,
    bool enabled, bool terminated) {
  DictionaryValue* extension_data = new DictionaryValue();
  GURL icon =
      ExtensionIconSource::GetIconURL(extension,
                                      Extension::EXTENSION_ICON_MEDIUM,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      !enabled, NULL);
  extension_data->SetString("id", extension->id());
  extension_data->SetString("name", extension->name());
  extension_data->SetString("description", extension->description());
  if (extension->location() == Extension::LOAD)
    extension_data->SetString("path", extension->path().value());
  extension_data->SetString("version", extension->version()->GetString());
  extension_data->SetString("icon", icon.spec());
  extension_data->SetBoolean("isUnpacked",
                             extension->location() == Extension::LOAD);
  extension_data->SetBoolean("mayDisable",
                             Extension::UserMayDisable(extension->location()));
  extension_data->SetBoolean("enabled", enabled);
  extension_data->SetBoolean("terminated", terminated);
  extension_data->SetBoolean("enabledIncognito",
      service ? service->IsIncognitoEnabled(extension->id()) : false);
  extension_data->SetBoolean("wantsFileAccess", extension->wants_file_access());
  extension_data->SetBoolean("allowFileAccess",
      service ? service->AllowFileAccess(extension) : false);
  extension_data->SetBoolean("allow_reload",
                             extension->location() == Extension::LOAD);
  extension_data->SetBoolean("is_hosted_app", extension->is_hosted_app());

  // Determine the sort order: Extensions loaded through --load-extensions show
  // up at the top. Disabled extensions show up at the bottom.
  if (extension->location() == Extension::LOAD)
    extension_data->SetInteger("order", 1);
  else
    extension_data->SetInteger("order", 2);

  if (!extension->options_url().is_empty() && enabled)
    extension_data->SetString("options_url", extension->options_url().spec());

  if (service && !service->GetBrowserActionVisibility(extension))
    extension_data->SetBoolean("enable_show_button", true);

  // Add views
  ListValue* views = new ListValue;
  for (std::vector<ExtensionPage>::const_iterator iter = pages.begin();
       iter != pages.end(); ++iter) {
    DictionaryValue* view_value = new DictionaryValue;
    if (iter->url.scheme() == chrome::kExtensionScheme) {
      // No leading slash.
      view_value->SetString("path", iter->url.path().substr(1));
    } else {
      // For live pages, use the full URL.
      view_value->SetString("path", iter->url.spec());
    }
    view_value->SetInteger("renderViewId", iter->render_view_id);
    view_value->SetInteger("renderProcessId", iter->render_process_id);
    view_value->SetBoolean("incognito", iter->incognito);
    views->Append(view_value);
  }
  extension_data->Set("views", views);
  extension_data->SetBoolean("hasPopupAction",
      extension->browser_action() || extension->page_action());
  extension_data->SetString("homepageUrl", extension->GetHomepageURL().spec());

  // Add warnings.
  ListValue* warnings_list = new ListValue;
  if (warnings_set) {
    std::set<ExtensionWarningSet::WarningType> warnings;
    warnings_set->GetWarningsAffectingExtension(extension->id(), &warnings);

    for (std::set<ExtensionWarningSet::WarningType>::const_iterator iter =
             warnings.begin();
         iter != warnings.end();
         ++iter) {
      string16 warning_string(ExtensionWarningSet::GetLocalizedWarning(*iter));
      warnings_list->Append(Value::CreateStringValue(warning_string));
    }
  }
  extension_data->Set("warnings", warnings_list);

  return extension_data;
}

std::vector<ExtensionPage> ExtensionSettingsHandler::GetActivePagesForExtension(
    const Extension* extension) {
  std::vector<ExtensionPage> result;

  // Get the extension process's active views.
  ExtensionProcessManager* process_manager =
      extension_service_->profile()->GetExtensionProcessManager();
  GetActivePagesForExtensionProcess(
      process_manager->GetRenderViewHostsForExtension(
          extension->id()), &result);

  // Repeat for the incognito process, if applicable.
  if (extension_service_->profile()->HasOffTheRecordProfile() &&
      extension->incognito_split_mode()) {
    ExtensionProcessManager* process_manager =
        extension_service_->profile()->GetOffTheRecordProfile()->
            GetExtensionProcessManager();
    GetActivePagesForExtensionProcess(
        process_manager->GetRenderViewHostsForExtension(
            extension->id()), &result);
  }

  return result;
}

void ExtensionSettingsHandler::GetActivePagesForExtensionProcess(
    const std::set<RenderViewHost*>& views,
    std::vector<ExtensionPage> *result) {
  for (std::set<RenderViewHost*>::const_iterator iter = views.begin();
       iter != views.end(); ++iter) {
    RenderViewHost* host = *iter;
    int host_type = host->delegate()->GetRenderViewType();
    if (host == deleting_rvh_ ||
        chrome::VIEW_TYPE_EXTENSION_POPUP == host_type ||
        chrome::VIEW_TYPE_EXTENSION_DIALOG == host_type)
      continue;

    GURL url = host->delegate()->GetURL();
    content::RenderProcessHost* process = host->process();
    result->push_back(
        ExtensionPage(url, process->GetID(), host->routing_id(),
                      process->GetBrowserContext()->IsOffTheRecord()));
  }
}
