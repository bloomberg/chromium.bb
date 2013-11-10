// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/id_util.h"
#include "extensions/common/manifest_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(USE_AURA)
#include "grit/keyboard_resources.h"
#include "ui/keyboard/keyboard_util.h"
#endif

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/defaults.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/storage_partition.h"
#include "webkit/browser/fileapi/file_system_context.h"
#endif

#if defined(ENABLE_APP_LIST)
#include "grit/chromium_strings.h"
#endif

namespace extensions {

namespace {

static bool enable_background_extensions_during_testing = false;

std::string LookupWebstoreName() {
  const char kWebStoreNameFieldTrialName[] = "WebStoreName";
  const char kStoreControl[] = "StoreControl";
  const char kWebStore[] = "WebStore";
  const char kGetApps[] = "GetApps";
  const char kAddApps[] = "AddApps";
  const char kMoreApps[] = "MoreApps";

  typedef std::map<std::string, int> NameMap;
  CR_DEFINE_STATIC_LOCAL(NameMap, names, ());
  if (names.empty()) {
    names.insert(std::make_pair(kStoreControl, IDS_WEBSTORE_NAME_STORE));
    names.insert(std::make_pair(kWebStore, IDS_WEBSTORE_NAME_WEBSTORE));
    names.insert(std::make_pair(kGetApps, IDS_WEBSTORE_NAME_GET_APPS));
    names.insert(std::make_pair(kAddApps, IDS_WEBSTORE_NAME_ADD_APPS));
    names.insert(std::make_pair(kMoreApps, IDS_WEBSTORE_NAME_MORE_APPS));
  }
  std::string field_trial_name =
      base::FieldTrialList::FindFullName(kWebStoreNameFieldTrialName);
  NameMap::iterator it = names.find(field_trial_name);
  int string_id = it == names.end() ? names[kStoreControl] : it->second;
  return l10n_util::GetStringUTF8(string_id);
}

std::string GenerateId(const base::DictionaryValue* manifest,
                       const base::FilePath& path) {
  std::string raw_key;
  std::string id_input;
  CHECK(manifest->GetString(manifest_keys::kPublicKey, &raw_key));
  CHECK(Extension::ParsePEMKeyBytes(raw_key, &id_input));
  std::string id = id_util::GenerateId(id_input);
  return id;
}

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
                                 PrefService* local_state)
    : profile_prefs_(profile_prefs),
      local_state_(local_state),
      extension_service_(extension_service) {}

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
      extension_file_util::LoadManifest(absolute_path, &error));
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
#if defined(FILE_MANAGER_EXTENSION)
#ifndef NDEBUG
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kFileManagerExtensionPath)) {
    base::FilePath filemgr_extension_path(
        command_line->GetSwitchValuePath(switches::kFileManagerExtensionPath));
    Add(IDR_FILEMANAGER_MANIFEST, filemgr_extension_path);
    return;
  }
#endif  // NDEBUG
  Add(IDR_FILEMANAGER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("file_manager")));
#endif  // defined(FILE_MANAGER_EXTENSION)
}

void ComponentLoader::AddHangoutServicesExtension() {
  Add(IDR_HANGOUT_SERVICES_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("hangout_services")));
}

void ComponentLoader::AddImageLoaderExtension() {
#if defined(IMAGE_LOADER_EXTENSION)
#ifndef NDEBUG
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kImageLoaderExtensionPath)) {
    base::FilePath image_loader_extension_path(
        command_line->GetSwitchValuePath(switches::kImageLoaderExtensionPath));
    Add(IDR_IMAGE_LOADER_MANIFEST, image_loader_extension_path);
    return;
  }
#endif  // NDEBUG
  Add(IDR_IMAGE_LOADER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("image_loader")));
#endif  // defined(IMAGE_LOADER_EXTENSION)
}

void ComponentLoader::AddBookmarksExtensions() {
  Add(IDR_BOOKMARKS_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("bookmark_manager")));
#if defined(ENABLE_ENHANCED_BOOKMARKS)
  Add(IDR_ENHANCED_BOOKMARKS_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("enhanced_bookmark_manager")));
#endif
}

void ComponentLoader::AddNetworkSpeechSynthesisExtension() {
  Add(IDR_NETWORK_SPEECH_SYNTHESIS_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("network_speech_synthesis")));
}

void ComponentLoader::AddWithName(int manifest_resource_id,
                                  const base::FilePath& root_directory,
                                  const std::string& name) {
  std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          manifest_resource_id).as_string();

  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  base::DictionaryValue* manifest = ParseManifest(manifest_contents);

  if (manifest) {
    // Update manifest to use a proper name.
    manifest->SetString(manifest_keys::kName, name);
    Add(manifest, root_directory);
  }
}

void ComponentLoader::AddChromeApp() {
#if defined(ENABLE_APP_LIST)
  AddWithName(IDR_CHROME_APP_MANIFEST,
              base::FilePath(FILE_PATH_LITERAL("chrome_app")),
              l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME));
#endif
}

void ComponentLoader::AddKeyboardApp() {
#if defined(USE_AURA)
  if (keyboard::IsKeyboardEnabled())
    Add(IDR_KEYBOARD_MANIFEST, base::FilePath(FILE_PATH_LITERAL("keyboard")));
#endif
}

void ComponentLoader::AddWebStoreApp() {
  AddWithName(IDR_WEBSTORE_MANIFEST,
              base::FilePath(FILE_PATH_LITERAL("web_store")),
              LookupWebstoreName());
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
      AddBookmarksExtensions();

    Add(IDR_CROSH_BUILTIN_MANIFEST, base::FilePath(FILE_PATH_LITERAL(
        "/usr/share/chromeos-assets/crosh_builtin")));
  }
#else  // !defined(OS_CHROMEOS)
  DCHECK(!skip_session_components);
  AddBookmarksExtensions();
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
    AddWithName(IDR_GENIUS_APP_MANIFEST,
                base::FilePath(FILE_PATH_LITERAL(
                    "/usr/share/chromeos-assets/genius_app")),
                l10n_util::GetStringUTF8(IDS_GENIUS_APP_NAME));
  }
#endif

  if (!skip_session_components) {
    // Apps Debugger
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAppsDevtool) &&
        profile_prefs_->GetBoolean(prefs::kExtensionsUIDeveloperMode)) {
      Add(IDR_APPS_DEBUGGER_MANIFEST,
          base::FilePath(FILE_PATH_LITERAL("apps_debugger")));
    }


    AddFileManagerExtension();
    AddHangoutServicesExtension();
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
            chromeos::switches::kDisableQuickofficeComponentApp)) {
      int manifest_id = IDR_QUICKOFFICE_EDITOR_MANIFEST;
      if (command_line->HasSwitch(switches::kEnableQuickofficeViewing)) {
        manifest_id = IDR_QUICKOFFICE_VIEWING_MANIFEST;
      }
      std::string id = Add(manifest_id, base::FilePath(
          FILE_PATH_LITERAL("/usr/share/chromeos-assets/quick_office")));
      if (command_line->HasSwitch(chromeos::switches::kGuestSession)) {
        // TODO(dpolukhin): Hack to enable HTML5 temporary file system for
        // Quickoffice. It doesn't work without temporary file system access.
        Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
        ExtensionService* service =
            extensions::ExtensionSystem::Get(profile)->extension_service();
        GURL site = service->GetSiteForExtensionId(id);
        fileapi::FileSystemContext* context =
            content::BrowserContext::GetStoragePartitionForSite(profile, site)->
                GetFileSystemContext();
        context->EnableTemporaryFileSystemInIncognito();
      }
    }
#endif  // defined(GOOGLE_CHROME_BUILD)

    base::FilePath echo_extension_path(FILE_PATH_LITERAL(
        "/usr/share/chromeos-assets/echo"));
    if (command_line->HasSwitch(chromeos::switches::kEchoExtensionPath)) {
      echo_extension_path = command_line->GetSwitchValuePath(
          chromeos::switches::kEchoExtensionPath);
    }
    Add(IDR_ECHO_MANIFEST, echo_extension_path);

    if (!command_line->HasSwitch(chromeos::switches::kGuestSession)) {
      Add(IDR_WALLPAPERMANAGER_MANIFEST,
          base::FilePath(FILE_PATH_LITERAL("chromeos/wallpaper_manager")));
    }

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
    base::FilePath path =
        base::FilePath(extension_misc::kChromeVoxExtensionPath);
    Add(IDR_CHROMEVOX_MANIFEST, path);
  }
#endif  // defined(OS_CHROMEOS)

#if defined(ENABLE_GOOGLE_NOW)
  const char kEnablePrefix[] = "Enable";
  const char kFieldTrialName[] = "GoogleNow";
  std::string enable_prefix(kEnablePrefix);
  std::string field_trial_result =
      base::FieldTrialList::FindFullName(kFieldTrialName);

  bool enabled_via_field_trial = field_trial_result.compare(
      0,
      enable_prefix.length(),
      enable_prefix) == 0;

  // Enable the feature on trybots.
  bool enabled_via_trunk_build = chrome::VersionInfo::GetChannel() ==
      chrome::VersionInfo::CHANNEL_UNKNOWN;

  bool enabled_via_flag =
      chrome::VersionInfo::GetChannel() !=
          chrome::VersionInfo::CHANNEL_STABLE &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGoogleNowIntegration);

  bool enabled =
      enabled_via_field_trial || enabled_via_trunk_build || enabled_via_flag;

  bool disabled_via_flag =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGoogleNowIntegration);

  if (!skip_session_components && enabled && !disabled_via_flag) {
    Add(IDR_GOOGLE_NOW_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("google_now")));
  }
#endif

#if defined(GOOGLE_CHROME_BUILD)
#if !defined(OS_CHROMEOS)  // http://crbug.com/314799
  AddNetworkSpeechSynthesisExtension();
#endif
#endif  // defined(GOOGLE_CHROME_BUILD)
}

void ComponentLoader::UnloadComponent(ComponentExtensionInfo* component) {
  delete component->manifest;
  if (extension_service_->is_ready()) {
    extension_service_->
        RemoveComponentExtension(component->extension_id);
  }
}

}  // namespace extensions
