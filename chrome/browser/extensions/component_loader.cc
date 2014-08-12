// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/id_util.h"
#include "extensions/common/manifest_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "grit/keyboard_resources.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#include "ui/keyboard/keyboard_util.h"
#endif

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/defaults.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/ime/input_method_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extensions_browser_client.h"
#include "webkit/browser/fileapi/file_system_context.h"
#endif

#if defined(ENABLE_APP_LIST)
#include "grit/chromium_strings.h"
#endif

using content::BrowserThread;

namespace extensions {

namespace {

static bool enable_background_extensions_during_testing = false;

std::string GenerateId(const base::DictionaryValue* manifest,
                       const base::FilePath& path) {
  std::string raw_key;
  std::string id_input;
  CHECK(manifest->GetString(manifest_keys::kPublicKey, &raw_key));
  CHECK(Extension::ParsePEMKeyBytes(raw_key, &id_input));
  std::string id = id_util::GenerateId(id_input);
  return id;
}

#if defined(OS_CHROMEOS)
scoped_ptr<base::DictionaryValue>
LoadManifestOnFileThread(
    const base::FilePath& chromevox_path, const char* manifest_filename) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  std::string error;
  scoped_ptr<base::DictionaryValue> manifest(
      file_util::LoadManifest(chromevox_path, manifest_filename, &error));
  CHECK(manifest) << error;
  return manifest.Pass();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

ComponentLoader::ComponentExtensionInfo::ComponentExtensionInfo(
    const base::DictionaryValue* manifest, const base::FilePath& directory)
    : manifest(manifest),
      root_directory(directory) {
  if (!root_directory.IsAbsolute()) {
    CHECK(PathService::Get(chrome::DIR_RESOURCES, &root_directory));
    root_directory = root_directory.Append(directory);
  }
  extension_id = GenerateId(manifest, root_directory);
}

ComponentLoader::ComponentLoader(ExtensionServiceInterface* extension_service,
                                 PrefService* profile_prefs,
                                 PrefService* local_state,
                                 content::BrowserContext* browser_context)
    : profile_prefs_(profile_prefs),
      local_state_(local_state),
      browser_context_(browser_context),
      extension_service_(extension_service),
      weak_factory_(this) {}

ComponentLoader::~ComponentLoader() {
  ClearAllRegistered();
}

void ComponentLoader::LoadAll() {
  for (RegisteredComponentExtensions::iterator it =
          component_extensions_.begin();
      it != component_extensions_.end(); ++it) {
    Load(*it);
  }
}

base::DictionaryValue* ComponentLoader::ParseManifest(
    const std::string& manifest_contents) const {
  JSONStringValueSerializer serializer(manifest_contents);
  scoped_ptr<base::Value> manifest(serializer.Deserialize(NULL, NULL));

  if (!manifest.get() || !manifest->IsType(base::Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Failed to parse extension manifest.";
    return NULL;
  }
  // Transfer ownership to the caller.
  return static_cast<base::DictionaryValue*>(manifest.release());
}

void ComponentLoader::ClearAllRegistered() {
  for (RegisteredComponentExtensions::iterator it =
          component_extensions_.begin();
      it != component_extensions_.end(); ++it) {
      delete it->manifest;
  }

  component_extensions_.clear();
}

std::string ComponentLoader::GetExtensionID(
    int manifest_resource_id,
    const base::FilePath& root_directory) {
  std::string manifest_contents = ResourceBundle::GetSharedInstance().
      GetRawDataResource(manifest_resource_id).as_string();
  base::DictionaryValue* manifest = ParseManifest(manifest_contents);
  if (!manifest)
    return std::string();

  ComponentExtensionInfo info(manifest, root_directory);
  return info.extension_id;
}

std::string ComponentLoader::Add(int manifest_resource_id,
                                 const base::FilePath& root_directory) {
  std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          manifest_resource_id).as_string();
  return Add(manifest_contents, root_directory);
}

std::string ComponentLoader::Add(const std::string& manifest_contents,
                                 const base::FilePath& root_directory) {
  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  base::DictionaryValue* manifest = ParseManifest(manifest_contents);
  if (manifest)
    return Add(manifest, root_directory);
  return std::string();
}

std::string ComponentLoader::Add(const base::DictionaryValue* parsed_manifest,
                                 const base::FilePath& root_directory) {
  ComponentExtensionInfo info(parsed_manifest, root_directory);
  component_extensions_.push_back(info);
  if (extension_service_->is_ready())
    Load(info);
  return info.extension_id;
}

std::string ComponentLoader::AddOrReplace(const base::FilePath& path) {
  base::FilePath absolute_path = base::MakeAbsoluteFilePath(path);
  std::string error;
  scoped_ptr<base::DictionaryValue> manifest(
      file_util::LoadManifest(absolute_path, &error));
  if (!manifest) {
    LOG(ERROR) << "Could not load extension from '" <<
                  absolute_path.value() << "'. " << error;
    return std::string();
  }
  Remove(GenerateId(manifest.get(), absolute_path));

  return Add(manifest.release(), absolute_path);
}

void ComponentLoader::Reload(const std::string& extension_id) {
  for (RegisteredComponentExtensions::iterator it =
         component_extensions_.begin(); it != component_extensions_.end();
         ++it) {
    if (it->extension_id == extension_id) {
      Load(*it);
      break;
    }
  }
}

void ComponentLoader::Load(const ComponentExtensionInfo& info) {
  // TODO(abarth): We should REQUIRE_MODERN_MANIFEST_VERSION once we've updated
  //               our component extensions to the new manifest version.
  int flags = Extension::REQUIRE_KEY;

  std::string error;

  scoped_refptr<const Extension> extension(Extension::Create(
      info.root_directory,
      Manifest::COMPONENT,
      *info.manifest,
      flags,
      &error));
  if (!extension.get()) {
    LOG(ERROR) << error;
    return;
  }

  CHECK_EQ(info.extension_id, extension->id()) << extension->name();
  extension_service_->AddComponentExtension(extension.get());
}

void ComponentLoader::Remove(const base::FilePath& root_directory) {
  // Find the ComponentExtensionInfo for the extension.
  RegisteredComponentExtensions::iterator it = component_extensions_.begin();
  for (; it != component_extensions_.end(); ++it) {
    if (it->root_directory == root_directory) {
      Remove(GenerateId(it->manifest, root_directory));
      break;
    }
  }
}

void ComponentLoader::Remove(const std::string& id) {
  RegisteredComponentExtensions::iterator it = component_extensions_.begin();
  for (; it != component_extensions_.end(); ++it) {
    if (it->extension_id == id) {
      UnloadComponent(&(*it));
      it = component_extensions_.erase(it);
      break;
    }
  }
}

bool ComponentLoader::Exists(const std::string& id) const {
  RegisteredComponentExtensions::const_iterator it =
      component_extensions_.begin();
  for (; it != component_extensions_.end(); ++it)
    if (it->extension_id == id)
      return true;
  return false;
}

void ComponentLoader::AddFileManagerExtension() {
#if defined(OS_CHROMEOS)
#ifndef NDEBUG
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kFileManagerExtensionPath)) {
    base::FilePath filemgr_extension_path(
        command_line->GetSwitchValuePath(switches::kFileManagerExtensionPath));
    AddWithNameAndDescription(IDR_FILEMANAGER_MANIFEST,
                              filemgr_extension_path,
                              IDS_FILEMANAGER_APP_NAME,
                              IDS_FILEMANAGER_APP_DESCRIPTION);
    return;
  }
#endif  // NDEBUG
  AddWithNameAndDescription(IDR_FILEMANAGER_MANIFEST,
                            base::FilePath(FILE_PATH_LITERAL("file_manager")),
                            IDS_FILEMANAGER_APP_NAME,
                            IDS_FILEMANAGER_APP_DESCRIPTION);
#endif  // defined(OS_CHROMEOS)
}

void ComponentLoader::AddVideoPlayerExtension() {
#if defined(OS_CHROMEOS)
  Add(IDR_VIDEO_PLAYER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("video_player")));
#endif  // defined(OS_CHROMEOS)
}

void ComponentLoader::AddGalleryExtension() {
#if defined(OS_CHROMEOS)
  Add(IDR_GALLERY_MANIFEST, base::FilePath(FILE_PATH_LITERAL("gallery")));
#endif
}

void ComponentLoader::AddHangoutServicesExtension() {
#if defined(GOOGLE_CHROME_BUILD) || defined(ENABLE_HANGOUT_SERVICES_EXTENSION)
  Add(IDR_HANGOUT_SERVICES_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("hangout_services")));
#endif
}

void ComponentLoader::AddHotwordHelperExtension() {
  if (HotwordServiceFactory::IsHotwordAllowed(browser_context_)) {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kEnableExperimentalHotwording)) {
      Add(IDR_HOTWORD_MANIFEST,
          base::FilePath(FILE_PATH_LITERAL("hotword")));
    } else {
      Add(IDR_HOTWORD_HELPER_MANIFEST,
          base::FilePath(FILE_PATH_LITERAL("hotword_helper")));
    }
  }
}

void ComponentLoader::AddImageLoaderExtension() {
#if defined(IMAGE_LOADER_EXTENSION)
  Add(IDR_IMAGE_LOADER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("image_loader")));
#endif  // defined(IMAGE_LOADER_EXTENSION)
}

void ComponentLoader::AddNetworkSpeechSynthesisExtension() {
  Add(IDR_NETWORK_SPEECH_SYNTHESIS_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("network_speech_synthesis")));
}

#if defined(OS_CHROMEOS)
void ComponentLoader::AddChromeVoxExtension(
    const base::Closure& done_cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::FilePath resources_path;
  PathService::Get(chrome::DIR_RESOURCES, &resources_path);
  base::FilePath chromevox_path =
      resources_path.Append(extension_misc::kChromeVoxExtensionPath);

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  const char* manifest_filename =
      command_line->HasSwitch(chromeos::switches::kGuestSession) ?
      extension_misc::kChromeVoxGuestManifestFilename :
          extension_misc::kChromeVoxManifestFilename;
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&LoadManifestOnFileThread, chromevox_path, manifest_filename),
      base::Bind(&ComponentLoader::AddChromeVoxExtensionWithManifest,
                 weak_factory_.GetWeakPtr(),
                 chromevox_path,
                 done_cb));
}

void ComponentLoader::AddChromeVoxExtensionWithManifest(
    const base::FilePath& chromevox_path,
    const base::Closure& done_cb,
    scoped_ptr<base::DictionaryValue> manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string extension_id = Add(manifest.release(), chromevox_path);
  CHECK_EQ(extension_misc::kChromeVoxExtensionId, extension_id);
  if (!done_cb.is_null())
    done_cb.Run();
}

std::string ComponentLoader::AddChromeOsSpeechSynthesisExtension() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  int idr = command_line->HasSwitch(chromeos::switches::kGuestSession) ?
      IDR_SPEECH_SYNTHESIS_GUEST_MANIFEST : IDR_SPEECH_SYNTHESIS_MANIFEST;
  std::string id = Add(idr,
      base::FilePath(extension_misc::kSpeechSynthesisExtensionPath));
  EnableFileSystemInGuestMode(id);
  return id;
}
#endif

void ComponentLoader::AddWithNameAndDescription(
    int manifest_resource_id,
    const base::FilePath& root_directory,
    int name_string_id,
    int description_string_id) {
  std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          manifest_resource_id).as_string();

  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  base::DictionaryValue* manifest = ParseManifest(manifest_contents);

  if (manifest) {
    manifest->SetString(manifest_keys::kName,
                        l10n_util::GetStringUTF8(name_string_id));
    manifest->SetString(manifest_keys::kDescription,
                        l10n_util::GetStringUTF8(description_string_id));
    Add(manifest, root_directory);
  }
}

void ComponentLoader::AddChromeApp() {
#if defined(ENABLE_APP_LIST)
  AddWithNameAndDescription(IDR_CHROME_APP_MANIFEST,
                            base::FilePath(FILE_PATH_LITERAL("chrome_app")),
                            IDS_SHORT_PRODUCT_NAME,
                            IDS_CHROME_SHORTCUT_DESCRIPTION);
#endif
}

void ComponentLoader::AddKeyboardApp() {
#if defined(OS_CHROMEOS)
  Add(IDR_KEYBOARD_MANIFEST, base::FilePath(FILE_PATH_LITERAL("keyboard")));
#endif
}

void ComponentLoader::AddWebStoreApp() {
  AddWithNameAndDescription(IDR_WEBSTORE_MANIFEST,
                            base::FilePath(FILE_PATH_LITERAL("web_store")),
                            IDS_WEBSTORE_NAME_STORE,
                            IDS_WEBSTORE_APP_DESCRIPTION);
}

// static
void ComponentLoader::EnableBackgroundExtensionsForTesting() {
  enable_background_extensions_during_testing = true;
}

void ComponentLoader::AddDefaultComponentExtensions(
    bool skip_session_components) {
  // Do not add component extensions that have background pages here -- add them
  // to AddDefaultComponentExtensionsWithBackgroundPages.
#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager* input_method_manager =
      chromeos::input_method::InputMethodManager::Get();
  if (input_method_manager)
    input_method_manager->InitializeComponentExtension();

  Add(IDR_MOBILE_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("/usr/share/chromeos-assets/mobile")));

#if defined(GOOGLE_CHROME_BUILD)
  if (browser_defaults::enable_help_app) {
    Add(IDR_HELP_MANIFEST, base::FilePath(FILE_PATH_LITERAL(
                               "/usr/share/chromeos-assets/helpapp")));
  }
#endif

  // Skip all other extensions that require user session presence.
  if (!skip_session_components) {
    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(chromeos::switches::kGuestSession))
      Add(IDR_BOOKMARKS_MANIFEST,
          base::FilePath(FILE_PATH_LITERAL("bookmark_manager")));

    Add(IDR_CROSH_BUILTIN_MANIFEST, base::FilePath(FILE_PATH_LITERAL(
        "/usr/share/chromeos-assets/crosh_builtin")));
  }
#else  // !defined(OS_CHROMEOS)
  DCHECK(!skip_session_components);
  Add(IDR_BOOKMARKS_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("bookmark_manager")));
  // Cloud Print component app. Not required on Chrome OS.
  Add(IDR_CLOUDPRINT_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("cloud_print")));
#endif

  if (!skip_session_components) {
    AddWebStoreApp();
    AddChromeApp();
  }

  AddKeyboardApp();

  AddDefaultComponentExtensionsWithBackgroundPages(skip_session_components);
}

void ComponentLoader::AddDefaultComponentExtensionsForKioskMode(
    bool skip_session_components) {
#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager* input_method_manager =
      chromeos::input_method::InputMethodManager::Get();
  if (input_method_manager)
    input_method_manager->InitializeComponentExtension();
#endif
  // No component extension for kiosk app launch splash screen.
  if (skip_session_components)
    return;

  // Component extensions needed for kiosk apps.
  AddVideoPlayerExtension();
  AddFileManagerExtension();
  AddGalleryExtension();

  // Add virtual keyboard.
  AddKeyboardApp();
}

void ComponentLoader::AddDefaultComponentExtensionsWithBackgroundPages(
    bool skip_session_components) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  // Component extensions with background pages are not enabled during tests
  // because they generate a lot of background behavior that can interfere.
  if (!enable_background_extensions_during_testing &&
      (command_line->HasSwitch(switches::kTestType) ||
          command_line->HasSwitch(
              switches::kDisableComponentExtensionsWithBackgroundPages))) {
    return;
  }

#if defined(OS_CHROMEOS) && defined(GOOGLE_CHROME_BUILD)
  // Since this is a v2 app it has a background page.
  if (!command_line->HasSwitch(chromeos::switches::kDisableGeniusApp)) {
    AddWithNameAndDescription(IDR_GENIUS_APP_MANIFEST,
                              base::FilePath(FILE_PATH_LITERAL(
                                  "/usr/share/chromeos-assets/genius_app")),
                              IDS_GENIUS_APP_NAME,
                              IDS_GENIUS_APP_DESCRIPTION);
  }
#endif

  if (!skip_session_components) {
    AddVideoPlayerExtension();
    AddFileManagerExtension();
    AddGalleryExtension();

    AddHangoutServicesExtension();
    AddHotwordHelperExtension();
    AddImageLoaderExtension();

#if defined(ENABLE_SETTINGS_APP)
    Add(IDR_SETTINGS_APP_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("settings_app")));
#endif
  }

  // If (!enable_background_extensions_during_testing || this isn't a test)
  //   install_feedback = false;
  bool install_feedback = enable_background_extensions_during_testing;
#if defined(GOOGLE_CHROME_BUILD)
  install_feedback = true;
#endif  // defined(GOOGLE_CHROME_BUILD)
  if (install_feedback)
    Add(IDR_FEEDBACK_MANIFEST, base::FilePath(FILE_PATH_LITERAL("feedback")));

#if defined(OS_CHROMEOS)
  if (!skip_session_components) {
#if defined(GOOGLE_CHROME_BUILD)
    if (!command_line->HasSwitch(
            chromeos::switches::kDisableOfficeEditingComponentApp)) {
      std::string id = Add(IDR_QUICKOFFICE_MANIFEST, base::FilePath(
          FILE_PATH_LITERAL("/usr/share/chromeos-assets/quickoffice")));
      EnableFileSystemInGuestMode(id);
    }
#endif  // defined(GOOGLE_CHROME_BUILD)

    Add(IDR_ECHO_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("/usr/share/chromeos-assets/echo")));

    if (!command_line->HasSwitch(chromeos::switches::kGuestSession)) {
      Add(IDR_WALLPAPERMANAGER_MANIFEST,
          base::FilePath(FILE_PATH_LITERAL("chromeos/wallpaper_manager")));
    }

    Add(IDR_FIRST_RUN_DIALOG_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("chromeos/first_run/app")));

    Add(IDR_NETWORK_CONFIGURATION_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("chromeos/network_configuration")));

    Add(IDR_CONNECTIVITY_DIAGNOSTICS_MANIFEST,
        base::FilePath(extension_misc::kConnectivityDiagnosticsPath));
    Add(IDR_CONNECTIVITY_DIAGNOSTICS_LAUNCHER_MANIFEST,
        base::FilePath(extension_misc::kConnectivityDiagnosticsLauncherPath));
  }

  // Load ChromeVox extension now if spoken feedback is enabled.
  if (chromeos::AccessibilityManager::Get() &&
      chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled()) {
    AddChromeVoxExtension(base::Closure());
  }
#endif  // defined(OS_CHROMEOS)

#if defined(ENABLE_GOOGLE_NOW)
  const char kEnablePrefix[] = "Enable";
  const char kFieldTrialName[] = "GoogleNow";
  std::string enable_prefix(kEnablePrefix);
  std::string field_trial_result =
      base::FieldTrialList::FindFullName(kFieldTrialName);

  bool enabled_via_field_trial =
      field_trial_result.compare(0, enable_prefix.length(), enable_prefix) == 0;

  // Enable the feature on trybots and trunk builds.
  bool enabled_via_trunk_build =
      chrome::VersionInfo::GetChannel() == chrome::VersionInfo::CHANNEL_UNKNOWN;

  bool enabled = enabled_via_field_trial || enabled_via_trunk_build;

  if (!skip_session_components && enabled) {
    Add(IDR_GOOGLE_NOW_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("google_now")));
  }
#endif

#if defined(GOOGLE_CHROME_BUILD)
#if !defined(OS_CHROMEOS)  // http://crbug.com/314799
  AddNetworkSpeechSynthesisExtension();
#endif

#endif  // defined(GOOGLE_CHROME_BUILD)

#if defined(ENABLE_PLUGINS)
  base::FilePath pdf_path;
  content::PluginService* plugin_service =
      content::PluginService::GetInstance();
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kOutOfProcessPdf) &&
      PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path) &&
      plugin_service->GetRegisteredPpapiPluginInfo(pdf_path)) {
    Add(IDR_PDF_MANIFEST, base::FilePath(FILE_PATH_LITERAL("pdf")));
  }
#endif

  Add(IDR_CRYPTOTOKEN_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("cryptotoken")));
}

void ComponentLoader::UnloadComponent(ComponentExtensionInfo* component) {
  delete component->manifest;
  if (extension_service_->is_ready()) {
    extension_service_->
        RemoveComponentExtension(component->extension_id);
  }
}

void ComponentLoader::EnableFileSystemInGuestMode(const std::string& id) {
#if defined(OS_CHROMEOS)
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kGuestSession)) {
    // TODO(dpolukhin): Hack to enable HTML5 temporary file system for
    // the extension. Some component extensions don't work without temporary
    // file system access. Make sure temporary file system is enabled in the off
    // the record browser context (as that is the one used in guest session).
    content::BrowserContext* off_the_record_context =
        ExtensionsBrowserClient::Get()->GetOffTheRecordContext(
            browser_context_);
    GURL site = content::SiteInstance::GetSiteForURL(
        off_the_record_context, Extension::GetBaseURLFromExtensionId(id));
    fileapi::FileSystemContext* file_system_context =
        content::BrowserContext::GetStoragePartitionForSite(
            off_the_record_context, site)->GetFileSystemContext();
    file_system_context->EnableTemporaryFileSystemInIncognito();
  }
#endif
}

}  // namespace extensions
