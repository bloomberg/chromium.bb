// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/prefs/pref_notifier.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OFFICIAL_BUILD)
#include "chrome/browser/defaults.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

#if defined(USE_ASH)
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace extensions {

namespace {

std::string GenerateId(const DictionaryValue* manifest, const FilePath& path) {
  std::string raw_key;
  std::string id_input;
  std::string id;
  CHECK(manifest->GetString(extension_manifest_keys::kPublicKey, &raw_key));
  CHECK(Extension::ParsePEMKeyBytes(raw_key, &id_input));
  CHECK(Extension::GenerateId(id_input, &id));
  return id;
}

}  // namespace

ComponentLoader::ComponentExtensionInfo::ComponentExtensionInfo(
    const DictionaryValue* manifest, const FilePath& directory)
    : manifest(manifest),
      root_directory(directory) {
  if (!root_directory.IsAbsolute()) {
    CHECK(PathService::Get(chrome::DIR_RESOURCES, &root_directory));
    root_directory = root_directory.Append(directory);
  }
  extension_id = GenerateId(manifest, root_directory);
}

ComponentLoader::ComponentLoader(ExtensionServiceInterface* extension_service,
                                 PrefService* prefs,
                                 PrefService* local_state)
    : prefs_(prefs),
      local_state_(local_state),
      extension_service_(extension_service) {
  pref_change_registrar_.Init(prefs);

  // This pref is set by policy. We have to watch it for change because on
  // ChromeOS, policy isn't loaded until after the browser process is started.
  pref_change_registrar_.Add(
      prefs::kEnterpriseWebStoreURL,
      base::Bind(&ComponentLoader::AddOrReloadEnterpriseWebStore,
                 base::Unretained(this)));
}

ComponentLoader::~ComponentLoader() {
  ClearAllRegistered();
}

const Extension* ComponentLoader::GetScriptBubble() const {
  if (script_bubble_id_.empty())
    return NULL;

  return extension_service_->extensions()->GetByID(script_bubble_id_);
}

void ComponentLoader::LoadAll() {
  for (RegisteredComponentExtensions::iterator it =
          component_extensions_.begin();
      it != component_extensions_.end(); ++it) {
    Load(*it);
  }
}

DictionaryValue* ComponentLoader::ParseManifest(
    const std::string& manifest_contents) const {
  JSONStringValueSerializer serializer(manifest_contents);
  scoped_ptr<Value> manifest(serializer.Deserialize(NULL, NULL));

  if (!manifest.get() || !manifest->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Failed to parse extension manifest.";
    return NULL;
  }
  // Transfer ownership to the caller.
  return static_cast<DictionaryValue*>(manifest.release());
}

void ComponentLoader::ClearAllRegistered() {
  for (RegisteredComponentExtensions::iterator it =
          component_extensions_.begin();
      it != component_extensions_.end(); ++it) {
      delete it->manifest;
  }

  component_extensions_.clear();
}

std::string ComponentLoader::Add(int manifest_resource_id,
                                 const FilePath& root_directory) {
  std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          manifest_resource_id).as_string();
  return Add(manifest_contents, root_directory);
}

std::string ComponentLoader::Add(const std::string& manifest_contents,
                                 const FilePath& root_directory) {
  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  DictionaryValue* manifest = ParseManifest(manifest_contents);
  if (manifest)
    return Add(manifest, root_directory);
  return "";
}

std::string ComponentLoader::Add(const DictionaryValue* parsed_manifest,
                                 const FilePath& root_directory) {
  ComponentExtensionInfo info(parsed_manifest, root_directory);
  component_extensions_.push_back(info);
  if (extension_service_->is_ready())
    Load(info);
  return info.extension_id;
}

std::string ComponentLoader::AddOrReplace(const FilePath& path) {
  FilePath absolute_path = path;
  file_util::AbsolutePath(&absolute_path);
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      extension_file_util::LoadManifest(absolute_path, &error));
  if (!manifest.get()) {
    LOG(ERROR) << "Could not load extension from '" <<
                  absolute_path.value() << "'. " << error;
    return NULL;
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

const Extension* ComponentLoader::Load(const ComponentExtensionInfo& info) {
  // TODO(abarth): We should REQUIRE_MODERN_MANIFEST_VERSION once we've updated
  //               our component extensions to the new manifest version.
  int flags = Extension::REQUIRE_KEY;

  std::string error;

  scoped_refptr<const Extension> extension(Extension::Create(
      info.root_directory,
      Extension::COMPONENT,
      *info.manifest,
      flags,
      &error));
  if (!extension.get()) {
    LOG(ERROR) << error;
    return NULL;
  }
  CHECK_EQ(info.extension_id, extension->id()) << extension->name();
  extension_service_->AddExtension(extension);
  return extension;
}

void ComponentLoader::Remove(const FilePath& root_directory) {
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
      delete it->manifest;
      it = component_extensions_.erase(it);
      if (extension_service_->is_ready())
        extension_service_->
            UnloadExtension(id, extension_misc::UNLOAD_REASON_DISABLE);
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
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  int manifest_id = command_line->HasSwitch(switches::kFileManagerPackaged) ?
      IDR_FILEMANAGER_MANIFEST :
      IDR_FILEMANAGER_MANIFEST_V1;
#ifndef NDEBUG
  if (command_line->HasSwitch(switches::kFileManagerExtensionPath)) {
    FilePath filemgr_extension_path(
        command_line->GetSwitchValuePath(switches::kFileManagerExtensionPath));
    Add(manifest_id, filemgr_extension_path);
    return;
  }
#endif  // NDEBUG
  Add(manifest_id, FilePath(FILE_PATH_LITERAL("file_manager")));
#endif  // defined(FILE_MANAGER_EXTENSION)
}

#if defined(OS_CHROMEOS)
void ComponentLoader::AddGaiaAuthExtension() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAuthExtensionPath)) {
    FilePath auth_extension_path =
        command_line->GetSwitchValuePath(switches::kAuthExtensionPath);
    Add(IDR_GAIA_TEST_AUTH_MANIFEST, auth_extension_path);
    return;
  }
  Add(IDR_GAIA_AUTH_MANIFEST, FilePath(FILE_PATH_LITERAL("gaia_auth")));
}
#endif  // NDEBUG

void ComponentLoader::AddOrReloadEnterpriseWebStore() {
  FilePath path(FILE_PATH_LITERAL("enterprise_web_store"));

  // Remove the extension if it was already loaded.
  Remove(path);

  std::string enterprise_webstore_url =
      prefs_->GetString(prefs::kEnterpriseWebStoreURL);

  // Load the extension only if the URL preference is set.
  if (!enterprise_webstore_url.empty()) {
    std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ENTERPRISE_WEBSTORE_MANIFEST).as_string();

    // The manifest is missing some values that are provided by policy.
    DictionaryValue* manifest = ParseManifest(manifest_contents);
    if (manifest) {
      std::string name = prefs_->GetString(prefs::kEnterpriseWebStoreName);
      manifest->SetString("app.launch.web_url", enterprise_webstore_url);
      manifest->SetString("name", name);
      Add(manifest, path);
    }
  }
}

void ComponentLoader::AddChromeApp() {
#if defined(USE_ASH)
  std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_CHROME_APP_MANIFEST).as_string();

  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  DictionaryValue* manifest = ParseManifest(manifest_contents);

  // Update manifest to use a proper name.
  manifest->SetString(extension_manifest_keys::kName,
                      l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME));

  if (manifest)
    Add(manifest, FilePath(FILE_PATH_LITERAL("chrome_app")));
#endif
}

void ComponentLoader::AddScriptBubble() {
  if (FeatureSwitch::script_bubble()->IsEnabled()) {
    script_bubble_id_ =
        Add(IDR_SCRIPT_BUBBLE_MANIFEST,
            FilePath(FILE_PATH_LITERAL("script_bubble")));
  }
}

void ComponentLoader::AddDefaultComponentExtensions() {
#if defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession))
    Add(IDR_BOOKMARKS_MANIFEST,
        FilePath(FILE_PATH_LITERAL("bookmark_manager")));
#else
  Add(IDR_BOOKMARKS_MANIFEST, FilePath(FILE_PATH_LITERAL("bookmark_manager")));
#endif

// Apps Debugger
Add(IDR_APPS_DEBUGGER_MANIFEST,
    FilePath(FILE_PATH_LITERAL("apps_debugger")));

#if defined(OS_CHROMEOS)
  Add(IDR_WALLPAPERMANAGER_MANIFEST,
      FilePath(FILE_PATH_LITERAL("chromeos/wallpaper_manager")));
#endif

#if defined(FILE_MANAGER_EXTENSION)
  AddFileManagerExtension();
#endif

#if defined(OS_CHROMEOS)
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableBackgroundLoader)) {
    Add(IDR_BACKLOADER_MANIFEST,
        FilePath(FILE_PATH_LITERAL("backloader")));
  }

  Add(IDR_MOBILE_MANIFEST,
      FilePath(FILE_PATH_LITERAL("/usr/share/chromeos-assets/mobile")));

  Add(IDR_CROSH_BUILTIN_MANIFEST, FilePath(FILE_PATH_LITERAL(
      "/usr/share/chromeos-assets/crosh_builtin")));

  AddGaiaAuthExtension();

  // TODO(gauravsh): Only include echo extension on official builds.
  FilePath echo_extension_path(FILE_PATH_LITERAL(
      "/usr/share/chromeos-assets/echo"));
  if (command_line->HasSwitch(switches::kEchoExtensionPath)) {
    echo_extension_path =
        command_line->GetSwitchValuePath(switches::kEchoExtensionPath);
  }
  Add(IDR_ECHO_MANIFEST, echo_extension_path);

#if defined(OFFICIAL_BUILD)
  if (browser_defaults::enable_help_app) {
    Add(IDR_HELP_MANIFEST,
        FilePath(FILE_PATH_LITERAL("/usr/share/chromeos-assets/helpapp")));
  }
#endif
#endif  // !defined(OS_CHROMEOS)

  Add(IDR_WEBSTORE_MANIFEST, FilePath(FILE_PATH_LITERAL("web_store")));

#if defined(ENABLE_SETTINGS_APP)
  Add(IDR_SETTINGS_APP_MANIFEST, FilePath(FILE_PATH_LITERAL("settings_app")));
#endif

#if !defined(OS_CHROMEOS)
  // Cloud Print component app. Not required on Chrome OS.
  Add(IDR_CLOUDPRINT_MANIFEST, FilePath(FILE_PATH_LITERAL("cloud_print")));
#endif

#if defined(OS_CHROMEOS)
  // Load ChromeVox extension now if spoken feedback is enabled.
  if (local_state_->GetBoolean(prefs::kSpokenFeedbackEnabled)) {
    FilePath path = FilePath(extension_misc::kChromeVoxExtensionPath);
    Add(IDR_CHROMEVOX_MANIFEST, path);
  }
#endif

  // If a URL for the enterprise webstore has been specified, load the
  // component extension. This extension might also be loaded later, because
  // it is specified by policy, and on ChromeOS policies are loaded after
  // the browser process has started.
  AddOrReloadEnterpriseWebStore();

#if defined(USE_ASH)
  AddChromeApp();
#endif

  AddScriptBubble();
}

// static
void ComponentLoader::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kEnterpriseWebStoreURL,
                            std::string() /* default_value */,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kEnterpriseWebStoreName,
                            std::string() /* default_value */,
                            PrefService::UNSYNCABLE_PREF);
}

}  // namespace extensions
