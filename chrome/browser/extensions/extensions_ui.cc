// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_ui.h"

#include <algorithm>

#include "base/base64.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/version.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_disabled_infobar_delegate.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/ui/webui/extension_icon_source.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

bool ShouldShowExtension(const Extension* extension) {
  // Don't show themes since this page's UI isn't really useful for themes.
  if (extension->is_theme())
    return false;

  // Don't show component extensions because they are only extensions as an
  // implementation detail of Chrome.
  if (extension->location() == Extension::COMPONENT)
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

////////////////////////////////////////////////////////////////////////////////
//
// ExtensionsHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

ExtensionsUIHTMLSource::ExtensionsUIHTMLSource()
    : DataSource(chrome::kChromeUIExtensionsHost, MessageLoop::current()) {
}

void ExtensionsUIHTMLSource::StartDataRequest(const std::string& path,
    bool is_off_the_record, int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_TITLE));
  localized_strings.SetString("devModeLink",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_DEVELOPER_MODE_LINK));
  localized_strings.SetString("devModePrefix",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_DEVELOPER_MODE_PREFIX));
  localized_strings.SetString("loadUnpackedButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOAD_UNPACKED_BUTTON));
  localized_strings.SetString("packButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_PACK_BUTTON));
  localized_strings.SetString("updateButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UPDATE_BUTTON));
  localized_strings.SetString("noExtensions",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_NONE_INSTALLED));
  localized_strings.SetString("suggestGallery",
      l10n_util::GetStringFUTF16(IDS_EXTENSIONS_NONE_INSTALLED_SUGGEST_GALLERY,
          ASCIIToUTF16("<a href='") +
              ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
                  GURL(Extension::ChromeStoreLaunchURL())).spec()) +
              ASCIIToUTF16("'>"),
          ASCIIToUTF16("</a>")));
  localized_strings.SetString("getMoreExtensions",
      ASCIIToUTF16("<a href='") +
      ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
          GURL(Extension::ChromeStoreLaunchURL())).spec()) +
      ASCIIToUTF16("'>") +
      l10n_util::GetStringUTF16(IDS_GET_MORE_EXTENSIONS) +
      ASCIIToUTF16("</a>"));
  localized_strings.SetString("extensionCrashed",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_CRASHED_EXTENSION));
  localized_strings.SetString("extensionDisabled",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLED_EXTENSION));
  localized_strings.SetString("inDevelopment",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_IN_DEVELOPMENT));
  localized_strings.SetString("viewIncognito",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VIEW_INCOGNITO));
  localized_strings.SetString("extensionId",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ID));
  localized_strings.SetString("extensionVersion",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_VERSION));
  localized_strings.SetString("inspectViews",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSPECT_VIEWS));
  localized_strings.SetString("inspectPopupsInstructions",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSPECT_POPUPS_INSTRUCTIONS));
  localized_strings.SetString("disable",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE));
  localized_strings.SetString("enable",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE));
  localized_strings.SetString("enableIncognito",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE_INCOGNITO));
  localized_strings.SetString("allowFileAccess",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ALLOW_FILE_ACCESS));
  localized_strings.SetString("incognitoWarning",
      l10n_util::GetStringFUTF16(IDS_EXTENSIONS_INCOGNITO_WARNING,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings.SetString("reload",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_RELOAD));
  localized_strings.SetString("uninstall",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNINSTALL));
  localized_strings.SetString("options",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_OPTIONS));
  localized_strings.SetString("packDialogTitle",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_TITLE));
  localized_strings.SetString("packDialogHeading",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_HEADING));
  localized_strings.SetString("rootDirectoryLabel",
      l10n_util::GetStringUTF16(
          IDS_EXTENSION_PACK_DIALOG_ROOT_DIRECTORY_LABEL));
  localized_strings.SetString("packDialogBrowse",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_BROWSE));
  localized_strings.SetString("privateKeyLabel",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_PRIVATE_KEY_LABEL));
  localized_strings.SetString("okButton",
      l10n_util::GetStringUTF16(IDS_OK));
  localized_strings.SetString("cancelButton",
      l10n_util::GetStringUTF16(IDS_CANCEL));
  localized_strings.SetString("showButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_BUTTON));

  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece extensions_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_EXTENSIONS_UI_HTML));
  std::string full_html(extensions_html.data(), extensions_html.size());
  jstemplate_builder::AppendJsonHtml(&localized_strings, &full_html);
  jstemplate_builder::AppendI18nTemplateSourceHtml(&full_html);
  jstemplate_builder::AppendI18nTemplateProcessHtml(&full_html);
  jstemplate_builder::AppendJsTemplateSourceHtml(&full_html);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

std::string ExtensionsUIHTMLSource::GetMimeType(const std::string&) const {
  return "text/html";
}

///////////////////////////////////////////////////////////////////////////////
//
// ExtensionsDOMHandler
//
///////////////////////////////////////////////////////////////////////////////

ExtensionsDOMHandler::ExtensionsDOMHandler(ExtensionService* extension_service)
    : extensions_service_(extension_service),
      ignore_notifications_(false),
      deleting_rvh_(NULL) {
  RegisterForNotifications();
}

void ExtensionsDOMHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("requestExtensionsData",
      NewCallback(this, &ExtensionsDOMHandler::HandleRequestExtensionsData));
  web_ui_->RegisterMessageCallback("toggleDeveloperMode",
      NewCallback(this, &ExtensionsDOMHandler::HandleToggleDeveloperMode));
  web_ui_->RegisterMessageCallback("inspect",
      NewCallback(this, &ExtensionsDOMHandler::HandleInspectMessage));
  web_ui_->RegisterMessageCallback("reload",
      NewCallback(this, &ExtensionsDOMHandler::HandleReloadMessage));
  web_ui_->RegisterMessageCallback("enable",
      NewCallback(this, &ExtensionsDOMHandler::HandleEnableMessage));
  web_ui_->RegisterMessageCallback("enableIncognito",
      NewCallback(this, &ExtensionsDOMHandler::HandleEnableIncognitoMessage));
  web_ui_->RegisterMessageCallback("allowFileAccess",
      NewCallback(this, &ExtensionsDOMHandler::HandleAllowFileAccessMessage));
  web_ui_->RegisterMessageCallback("uninstall",
      NewCallback(this, &ExtensionsDOMHandler::HandleUninstallMessage));
  web_ui_->RegisterMessageCallback("options",
      NewCallback(this, &ExtensionsDOMHandler::HandleOptionsMessage));
  web_ui_->RegisterMessageCallback("showButton",
      NewCallback(this, &ExtensionsDOMHandler::HandleShowButtonMessage));
  web_ui_->RegisterMessageCallback("load",
      NewCallback(this, &ExtensionsDOMHandler::HandleLoadMessage));
  web_ui_->RegisterMessageCallback("pack",
      NewCallback(this, &ExtensionsDOMHandler::HandlePackMessage));
  web_ui_->RegisterMessageCallback("autoupdate",
      NewCallback(this, &ExtensionsDOMHandler::HandleAutoUpdateMessage));
  web_ui_->RegisterMessageCallback("selectFilePath",
      NewCallback(this, &ExtensionsDOMHandler::HandleSelectFilePathMessage));
}

void ExtensionsDOMHandler::HandleRequestExtensionsData(const ListValue* args) {
  DictionaryValue results;

  // Add the extensions to the results structure.
  ListValue* extensions_list = new ListValue();

  const ExtensionList* extensions = extensions_service_->extensions();
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    if (ShouldShowExtension(*extension)) {
      extensions_list->Append(CreateExtensionDetailValue(
          extensions_service_.get(),
          *extension,
          GetActivePagesForExtension(*extension),
          true, false));  // enabled, terminated
    }
  }
  extensions = extensions_service_->disabled_extensions();
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    if (ShouldShowExtension(*extension)) {
      extensions_list->Append(CreateExtensionDetailValue(
          extensions_service_.get(),
          *extension,
          GetActivePagesForExtension(*extension),
          false, false));  // enabled, terminated
    }
  }
  extensions = extensions_service_->terminated_extensions();
  std::vector<ExtensionPage> empty_pages;
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    if (ShouldShowExtension(*extension)) {
      extensions_list->Append(CreateExtensionDetailValue(
          extensions_service_.get(),
          *extension,
          empty_pages,  // Terminated process has no active pages.
          false, true));  // enabled, terminated
    }
  }
  results.Set("extensions", extensions_list);

  bool developer_mode = web_ui_->GetProfile()->GetPrefs()
      ->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  results.SetBoolean("developerMode", developer_mode);

  web_ui_->CallJavascriptFunction(L"returnExtensionsData", results);
}

void ExtensionsDOMHandler::RegisterForNotifications() {
  // Register for notifications that we need to reload the page.
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_PROCESS_CREATED,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UPDATE_DISABLED,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_FUNCTION_DISPATCHER_CREATED,
      NotificationService::AllSources());
  registrar_.Add(this,
      NotificationType::EXTENSION_FUNCTION_DISPATCHER_DESTROYED,
      NotificationService::AllSources());
  registrar_.Add(this,
      NotificationType::NAV_ENTRY_COMMITTED,
      NotificationService::AllSources());
  registrar_.Add(this,
      NotificationType::RENDER_VIEW_HOST_DELETED,
      NotificationService::AllSources());
  registrar_.Add(this,
      NotificationType::BACKGROUND_CONTENTS_NAVIGATED,
      NotificationService::AllSources());
  registrar_.Add(this,
      NotificationType::BACKGROUND_CONTENTS_DELETED,
      NotificationService::AllSources());
  registrar_.Add(this,
      NotificationType::EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
      NotificationService::AllSources());
}

ExtensionInstallUI* ExtensionsDOMHandler::GetExtensionInstallUI() {
  if (!install_ui_.get())
    install_ui_.reset(new ExtensionInstallUI(web_ui_->GetProfile()));
  return install_ui_.get();
}

void ExtensionsDOMHandler::HandleToggleDeveloperMode(const ListValue* args) {
  bool developer_mode = web_ui_->GetProfile()->GetPrefs()
      ->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  web_ui_->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kExtensionsUIDeveloperMode, !developer_mode);
}

void ExtensionsDOMHandler::HandleInspectMessage(const ListValue* args) {
  std::string render_process_id_str;
  std::string render_view_id_str;
  int render_process_id;
  int render_view_id;
  CHECK(args->GetSize() == 2);
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

  DevToolsManager::GetInstance()->OpenDevToolsWindow(host);
}

void ExtensionsDOMHandler::HandleReloadMessage(const ListValue* args) {
  std::string extension_id = WideToASCII(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  extensions_service_->ReloadExtension(extension_id);
}

void ExtensionsDOMHandler::HandleEnableMessage(const ListValue* args) {
  CHECK(args->GetSize() == 2);
  std::string extension_id, enable_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &enable_str));
  if (enable_str == "true") {
    ExtensionPrefs* prefs = extensions_service_->extension_prefs();
    if (prefs->DidExtensionEscalatePermissions(extension_id)) {
      const Extension* extension =
          extensions_service_->GetExtensionById(extension_id, true);
      ShowExtensionDisabledDialog(extensions_service_,
                                  web_ui_->GetProfile(), extension);
    } else {
      extensions_service_->EnableExtension(extension_id);
    }
  } else {
    extensions_service_->DisableExtension(extension_id);
  }
}

void ExtensionsDOMHandler::HandleEnableIncognitoMessage(const ListValue* args) {
  CHECK(args->GetSize() == 2);
  std::string extension_id, enable_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &enable_str));
  const Extension* extension =
      extensions_service_->GetExtensionById(extension_id, true);
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
  ignore_notifications_ = true;
  extensions_service_->SetIsIncognitoEnabled(extension, enable_str == "true");
  ignore_notifications_ = false;
}

void ExtensionsDOMHandler::HandleAllowFileAccessMessage(const ListValue* args) {
  CHECK(args->GetSize() == 2);
  std::string extension_id, allow_str;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetString(1, &allow_str));
  const Extension* extension =
      extensions_service_->GetExtensionById(extension_id, true);
  DCHECK(extension);

  extensions_service_->SetAllowFileAccess(extension, allow_str == "true");
}

void ExtensionsDOMHandler::HandleUninstallMessage(const ListValue* args) {
  std::string extension_id = WideToASCII(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  const Extension* extension =
      extensions_service_->GetExtensionById(extension_id, true);
  if (!extension)
    extension = extensions_service_->GetTerminatedExtension(extension_id);
  if (!extension)
    return;

  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.

  extension_id_prompting_ = extension_id;

  GetExtensionInstallUI()->ConfirmUninstall(this, extension);
}

void ExtensionsDOMHandler::InstallUIProceed() {
  DCHECK(!extension_id_prompting_.empty());

  bool was_terminated = false;

  // The extension can be uninstalled in another window while the UI was
  // showing. Do nothing in that case.
  const Extension* extension =
      extensions_service_->GetExtensionById(extension_id_prompting_, true);
  if (!extension) {
    extension = extensions_service_->GetTerminatedExtension(
        extension_id_prompting_);
    was_terminated = true;
  }
  if (!extension)
    return;

  extensions_service_->UninstallExtension(extension_id_prompting_,
                                          false /* external_uninstall */);
  extension_id_prompting_ = "";

  // There will be no EXTENSION_UNLOADED notification for terminated
  // extensions as they were already unloaded.
  if (was_terminated)
    HandleRequestExtensionsData(NULL);
}

void ExtensionsDOMHandler::InstallUIAbort() {
  extension_id_prompting_ = "";
}

void ExtensionsDOMHandler::HandleOptionsMessage(const ListValue* args) {
  const Extension* extension = GetExtension(args);
  if (!extension || extension->options_url().is_empty())
    return;
  web_ui_->GetProfile()->GetExtensionProcessManager()->OpenOptionsPage(
      extension, NULL);
}

void ExtensionsDOMHandler::HandleShowButtonMessage(const ListValue* args) {
  const Extension* extension = GetExtension(args);
  extensions_service_->SetBrowserActionVisibility(extension, true);
}

void ExtensionsDOMHandler::HandleLoadMessage(const ListValue* args) {
  FilePath::StringType string_path;
  CHECK(args->GetSize() == 1) << args->GetSize();
  CHECK(args->GetString(0, &string_path));
  extensions_service_->LoadExtension(FilePath(string_path));
}

void ExtensionsDOMHandler::ShowAlert(const std::string& message) {
  ListValue arguments;
  arguments.Append(Value::CreateStringValue(message));
  web_ui_->CallJavascriptFunction(L"alert", arguments);
}

void ExtensionsDOMHandler::HandlePackMessage(const ListValue* args) {
  std::string extension_path;
  std::string private_key_path;
  CHECK(args->GetSize() == 2);
  CHECK(args->GetString(0, &extension_path));
  CHECK(args->GetString(1, &private_key_path));

  FilePath root_directory =
      FilePath::FromWStringHack(UTF8ToWide(extension_path));
  FilePath key_file = FilePath::FromWStringHack(UTF8ToWide(private_key_path));

  if (root_directory.empty()) {
    if (extension_path.empty()) {
      ShowAlert(l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_REQUIRED));
    } else {
      ShowAlert(l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_INVALID));
    }

    return;
  }

  if (!private_key_path.empty() && key_file.empty()) {
    ShowAlert(l10n_util::GetStringUTF8(
        IDS_EXTENSION_PACK_DIALOG_ERROR_KEY_INVALID));
    return;
  }

  pack_job_ = new PackExtensionJob(this, root_directory, key_file);
  pack_job_->Start();
}

void ExtensionsDOMHandler::OnPackSuccess(const FilePath& crx_file,
                                         const FilePath& pem_file) {
  ShowAlert(UTF16ToUTF8(PackExtensionJob::StandardSuccessMessage(crx_file,
                                                                 pem_file)));

  ListValue results;
  web_ui_->CallJavascriptFunction(L"hidePackDialog", results);
}

void ExtensionsDOMHandler::OnPackFailure(const std::string& error) {
  ShowAlert(error);
}

void ExtensionsDOMHandler::HandleAutoUpdateMessage(const ListValue* args) {
  ExtensionUpdater* updater = extensions_service_->updater();
  if (updater)
    updater->CheckNow();
}

void ExtensionsDOMHandler::HandleSelectFilePathMessage(const ListValue* args) {
  std::string select_type;
  std::string operation;
  CHECK(args->GetSize() == 2);
  CHECK(args->GetString(0, &select_type));
  CHECK(args->GetString(1, &operation));

  SelectFileDialog::Type type = SelectFileDialog::SELECT_FOLDER;
  static SelectFileDialog::FileTypeInfo info;
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
      file_type_index, FILE_PATH_LITERAL(""),
      web_ui_->tab_contents()->view()->GetTopLevelNativeWindow(), NULL);
}


void ExtensionsDOMHandler::FileSelected(const FilePath& path, int index,
                                        void* params) {
  // Add the extensions to the results structure.
  ListValue results;
  results.Append(Value::CreateStringValue(path.value()));
  web_ui_->CallJavascriptFunction(L"window.handleFilePathSelected", results);
}

void ExtensionsDOMHandler::MultiFilesSelected(
    const std::vector<FilePath>& files, void* params) {
  NOTREACHED();
}

void ExtensionsDOMHandler::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  switch (type.value) {
    // We listen for notifications that will result in the page being
    // repopulated with data twice for the same event in certain cases.
    // For instance, EXTENSION_LOADED & EXTENSION_PROCESS_CREATED because
    // we don't know about the views for an extension at EXTENSION_LOADED, but
    // if we only listen to EXTENSION_PROCESS_CREATED, we'll miss extensions
    // that don't have a process at startup. Similarly, NAV_ENTRY_COMMITTED &
    // EXTENSION_FUNCTION_DISPATCHER_CREATED because we want to handle both
    // the case of live app pages (which don't have an EFD) and
    // chrome-extension:// urls which are served in a TabContents.
    //
    // Doing it this way gets everything but causes the page to be rendered
    // more than we need. It doesn't seem to result in any noticeable flicker.
    case NotificationType::RENDER_VIEW_HOST_DELETED:
      deleting_rvh_ = Details<RenderViewHost>(details).ptr();
      MaybeUpdateAfterNotification();
      break;
    case NotificationType::BACKGROUND_CONTENTS_DELETED:
      deleting_rvh_ = Details<BackgroundContents>(details)->render_view_host();
      MaybeUpdateAfterNotification();
      break;
    case NotificationType::EXTENSION_LOADED:
    case NotificationType::EXTENSION_PROCESS_CREATED:
    case NotificationType::EXTENSION_UNLOADED:
    case NotificationType::EXTENSION_UPDATE_DISABLED:
    case NotificationType::EXTENSION_FUNCTION_DISPATCHER_CREATED:
    case NotificationType::EXTENSION_FUNCTION_DISPATCHER_DESTROYED:
    case NotificationType::NAV_ENTRY_COMMITTED:
    case NotificationType::BACKGROUND_CONTENTS_NAVIGATED:
    case NotificationType::EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED:
      MaybeUpdateAfterNotification();
      break;
    default:
      NOTREACHED();
  }
}

const Extension* ExtensionsDOMHandler::GetExtension(const ListValue* args) {
  std::string extension_id = WideToASCII(ExtractStringValue(args));
  CHECK(!extension_id.empty());
  return extensions_service_->GetExtensionById(extension_id, true);
}

void ExtensionsDOMHandler::MaybeUpdateAfterNotification() {
  if (!ignore_notifications_ &&
      web_ui_->tab_contents() &&
      web_ui_->tab_contents()->render_view_host()) {
    HandleRequestExtensionsData(NULL);
  }
  deleting_rvh_ = NULL;
}

static void CreateScriptFileDetailValue(
    const FilePath& extension_path, const UserScript::FileList& scripts,
    const char* key, DictionaryValue* script_data) {
  if (scripts.empty())
    return;

  ListValue *list = new ListValue();
  for (size_t i = 0; i < scripts.size(); ++i) {
    const UserScript::File& file = scripts[i];
    // TODO(cira): this information is not used on extension page yet. We
    // may want to display actual resource that got loaded, not default.
    list->Append(
        new StringValue(file.relative_path().value()));
  }
  script_data->Set(key, list);
}

// Static
DictionaryValue* ExtensionsDOMHandler::CreateContentScriptDetailValue(
  const UserScript& script, const FilePath& extension_path) {
  DictionaryValue* script_data = new DictionaryValue();
  CreateScriptFileDetailValue(extension_path, script.js_scripts(), "js",
    script_data);
  CreateScriptFileDetailValue(extension_path, script.css_scripts(), "css",
    script_data);

  // Get list of glob "matches" strings
  ListValue *url_pattern_list = new ListValue();
  const std::vector<URLPattern>& url_patterns = script.url_patterns();
  for (std::vector<URLPattern>::const_iterator url_pattern =
      url_patterns.begin();
    url_pattern != url_patterns.end(); ++url_pattern) {
    url_pattern_list->Append(new StringValue(url_pattern->GetAsString()));
  }

  script_data->Set("matches", url_pattern_list);

  return script_data;
}

static bool ExtensionWantsFileAccess(const Extension* extension) {
  for (UserScriptList::const_iterator it = extension->content_scripts().begin();
       it != extension->content_scripts().end(); ++it) {
    for (UserScript::PatternList::const_iterator pattern =
             it->url_patterns().begin();
         pattern != it->url_patterns().end(); ++pattern) {
      if (pattern->MatchesScheme(chrome::kFileScheme))
        return true;
    }
  }

  return false;
}

// Static
DictionaryValue* ExtensionsDOMHandler::CreateExtensionDetailValue(
    ExtensionService* service, const Extension* extension,
    const std::vector<ExtensionPage>& pages, bool enabled, bool terminated) {
  DictionaryValue* extension_data = new DictionaryValue();
  GURL icon =
      ExtensionIconSource::GetIconURL(extension,
                                      Extension::EXTENSION_ICON_MEDIUM,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      !enabled);
  extension_data->SetString("id", extension->id());
  extension_data->SetString("name", extension->name());
  extension_data->SetString("description", extension->description());
  extension_data->SetString("version", extension->version()->GetString());
  extension_data->SetString("icon", icon.spec());
  extension_data->SetBoolean("enabled", enabled);
  extension_data->SetBoolean("terminated", terminated);
  extension_data->SetBoolean("enabledIncognito",
      service ? service->IsIncognitoEnabled(extension) : false);
  extension_data->SetBoolean("wantsFileAccess",
      ExtensionWantsFileAccess(extension));
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

  if (!extension->options_url().is_empty())
    extension_data->SetString("options_url", extension->options_url().spec());

  if (service && !service->GetBrowserActionVisibility(extension))
    extension_data->SetBoolean("enable_show_button", true);

  // Add list of content_script detail DictionaryValues.
  ListValue *content_script_list = new ListValue();
  UserScriptList content_scripts = extension->content_scripts();
  for (UserScriptList::const_iterator script = content_scripts.begin();
    script != content_scripts.end(); ++script) {
      content_script_list->Append(
          CreateContentScriptDetailValue(*script, extension->path()));
  }
  extension_data->Set("content_scripts", content_script_list);

  // Add permissions.
  ListValue *permission_list = new ListValue;
  std::vector<URLPattern> permissions = extension->host_permissions();
  for (std::vector<URLPattern>::iterator permission = permissions.begin();
       permission != permissions.end(); ++permission) {
    permission_list->Append(Value::CreateStringValue(
        permission->GetAsString()));
  }
  extension_data->Set("permissions", permission_list);

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

  return extension_data;
}

std::vector<ExtensionPage> ExtensionsDOMHandler::GetActivePagesForExtension(
    const Extension* extension) {
  std::vector<ExtensionPage> result;

  // Get the extension process's active views.
  ExtensionProcessManager* process_manager =
      extensions_service_->profile()->GetExtensionProcessManager();
  GetActivePagesForExtensionProcess(
      process_manager->GetExtensionProcess(extension->url()),
      extension, &result);

  // Repeat for the incognito process, if applicable.
  if (extensions_service_->profile()->HasOffTheRecordProfile() &&
      extension->incognito_split_mode()) {
    ExtensionProcessManager* process_manager =
        extensions_service_->profile()->GetOffTheRecordProfile()->
            GetExtensionProcessManager();
    GetActivePagesForExtensionProcess(
        process_manager->GetExtensionProcess(extension->url()),
        extension, &result);
  }

  return result;
}

void ExtensionsDOMHandler::GetActivePagesForExtensionProcess(
    RenderProcessHost* process,
    const Extension* extension,
    std::vector<ExtensionPage> *result) {
  if (!process)
    return;

  RenderProcessHost::listeners_iterator iter = process->ListenersIterator();
  for (; !iter.IsAtEnd(); iter.Advance()) {
    const RenderWidgetHost* widget =
        static_cast<const RenderWidgetHost*>(iter.GetCurrentValue());
    DCHECK(widget);
    if (!widget || !widget->IsRenderView())
      continue;
    const RenderViewHost* host = static_cast<const RenderViewHost*>(widget);
    if (host == deleting_rvh_ ||
        ViewType::EXTENSION_POPUP == host->delegate()->GetRenderViewType())
      continue;

    GURL url = host->delegate()->GetURL();
    if (url.SchemeIs(chrome::kExtensionScheme)) {
      if (url.host() != extension->id())
        continue;
    } else if (!extension->web_extent().ContainsURL(url)) {
      continue;
    }

    result->push_back(ExtensionPage(url, process->id(), host->routing_id(),
                                    process->profile()->IsOffTheRecord()));
  }
}

ExtensionsDOMHandler::~ExtensionsDOMHandler() {
  if (pack_job_.get())
    pack_job_->ClearClient();

  registrar_.RemoveAll();
}

// ExtensionsDOMHandler, public: -----------------------------------------------

ExtensionsUI::ExtensionsUI(TabContents* contents) : WebUI(contents) {
  ExtensionService *exstension_service =
      GetProfile()->GetOriginalProfile()->GetExtensionService();

  ExtensionsDOMHandler* handler = new ExtensionsDOMHandler(exstension_service);
  AddMessageHandler(handler);
  handler->Attach(this);

  ExtensionsUIHTMLSource* html_source = new ExtensionsUIHTMLSource();

  // Set up the chrome://extensions/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

// static
RefCountedMemory* ExtensionsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_PLUGIN);
}

// static
void ExtensionsUI::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kExtensionsUIDeveloperMode, false);
}
