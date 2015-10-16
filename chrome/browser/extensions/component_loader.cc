// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/profiler/scoped_profile.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/extensions/component_extensions_whitelist/whitelist.h"
#include "chrome/browser/extensions/data_deleter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "grit/browser_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/devicetype_utils.h"
#include "components/chrome_apps/grit/chrome_apps_resources.h"
#include "components/user_manager/user_manager.h"
#include "grit/keyboard_resources.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#include "ui/keyboard/keyboard_util.h"
#endif

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/defaults.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extensions_browser_client.h"
#include "storage/browser/fileapi/file_system_context.h"
#endif

#if defined(ENABLE_APP_LIST)
#include "chrome/grit/chromium_strings.h"
#endif

#if defined(ENABLE_APP_LIST) && defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/google_now_extension.h"
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
  std::string id = crx_file::id_util::GenerateId(id_input);
  return id;
}

#if defined(OS_CHROMEOS)
scoped_ptr<base::DictionaryValue>
LoadManifestOnFileThread(
    const base::FilePath& root_directory,
    const base::FilePath::CharType* manifest_filename) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  std::string error;
  scoped_ptr<base::DictionaryValue> manifest(
      file_util::LoadManifest(root_directory, manifest_filename, &error));
  if (!manifest) {
    LOG(ERROR) << "Can't load "
               << root_directory.Append(manifest_filename).AsUTF8Unsafe()
               << ": " << error;
    return nullptr;
  }
  bool localized = extension_l10n_util::LocalizeExtension(
      root_directory, manifest.get(), &error);
  CHECK(localized) << error;
  return manifest.Pass();
}

bool IsNormalSession() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kGuestSession) &&
         user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
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
                                 Profile* profile)
    : profile_prefs_(profile_prefs),
      local_state_(local_state),
      profile_(profile),
      extension_service_(extension_service),
      ignore_whitelist_for_testing_(false),
      weak_factory_(this) {}

ComponentLoader::~ComponentLoader() {
  ClearAllRegistered();
}

void ComponentLoader::LoadAll() {
  TRACE_EVENT0("browser,startup", "ComponentLoader::LoadAll");
  TRACK_SCOPED_REGION("Startup", "ComponentLoader::LoadAll");
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.LoadAllComponentTime");

  for (RegisteredComponentExtensions::iterator it =
          component_extensions_.begin();
      it != component_extensions_.end(); ++it) {
    Load(*it);
  }
}

base::DictionaryValue* ComponentLoader::ParseManifest(
    const std::string& manifest_contents) const {
  JSONStringValueDeserializer deserializer(manifest_contents);
  scoped_ptr<base::Value> manifest = deserializer.Deserialize(NULL, NULL);

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
  if (!ignore_whitelist_for_testing_ &&
      !IsComponentExtensionWhitelisted(manifest_resource_id))
    return std::string();

  std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          manifest_resource_id).as_string();
  return Add(manifest_contents, root_directory, true);
}

std::string ComponentLoader::Add(const std::string& manifest_contents,
                                 const base::FilePath& root_directory) {
  return Add(manifest_contents, root_directory, false);
}

std::string ComponentLoader::Add(const std::string& manifest_contents,
                                 const base::FilePath& root_directory,
                                 bool skip_whitelist) {
  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  base::DictionaryValue* manifest = ParseManifest(manifest_contents);
  if (manifest)
    return Add(manifest, root_directory, skip_whitelist);
  return std::string();
}

std::string ComponentLoader::Add(const base::DictionaryValue* parsed_manifest,
                                 const base::FilePath& root_directory,
                                 bool skip_whitelist) {
  ComponentExtensionInfo info(parsed_manifest, root_directory);
  if (!ignore_whitelist_for_testing_ &&
      !skip_whitelist &&
      !IsComponentExtensionWhitelisted(info.extension_id))
    return std::string();

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

  // We don't check component extensions loaded by path because this is only
  // used by developers for testing.
  return Add(manifest.release(), absolute_path, true);
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
  std::string error;
  scoped_refptr<const Extension> extension(CreateExtension(info, &error));
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
  AddWithNameAndDescription(
      IDR_FILEMANAGER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("file_manager")),
      l10n_util::GetStringUTF8(IDS_FILEMANAGER_APP_NAME),
      l10n_util::GetStringUTF8(IDS_FILEMANAGER_APP_DESCRIPTION));
#endif  // defined(OS_CHROMEOS)
}

void ComponentLoader::AddVideoPlayerExtension() {
#if defined(OS_CHROMEOS)
  Add(IDR_VIDEO_PLAYER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("video_player")));
#endif  // defined(OS_CHROMEOS)
}

void ComponentLoader::AddAudioPlayerExtension() {
#if defined(OS_CHROMEOS)
  Add(IDR_AUDIO_PLAYER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("audio_player")));
#endif  // defined(OS_CHROMEOS)
}

void ComponentLoader::AddGalleryExtension() {
#if defined(OS_CHROMEOS)
  Add(IDR_GALLERY_MANIFEST, base::FilePath(FILE_PATH_LITERAL("gallery")));
#endif
}

void ComponentLoader::AddWebstoreWidgetExtension() {
#if defined(OS_CHROMEOS)
  AddWithNameAndDescription(
      IDR_CHROME_APPS_WEBSTORE_WIDGET_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("webstore_widget")),
      l10n_util::GetStringUTF8(IDS_WEBSTORE_WIDGET_APP_NAME),
      l10n_util::GetStringUTF8(IDS_WEBSTORE_WIDGET_APP_DESC));
#endif
}

void ComponentLoader::AddHangoutServicesExtension() {
#if defined(GOOGLE_CHROME_BUILD) || defined(ENABLE_HANGOUT_SERVICES_EXTENSION)
  Add(IDR_HANGOUT_SERVICES_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("hangout_services")));
#endif
}

void ComponentLoader::AddHotwordAudioVerificationApp() {
  if (HotwordServiceFactory::IsAlwaysOnAvailable()) {
    Add(IDR_HOTWORD_AUDIO_VERIFICATION_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("hotword_audio_verification")));
  }
}

void ComponentLoader::AddHotwordHelperExtension() {
  if (HotwordServiceFactory::IsHotwordAllowed(profile_)) {
    Add(IDR_HOTWORD_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("hotword")));
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

void ComponentLoader::AddGoogleNowExtension() {
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
      chrome::GetChannel() == version_info::Channel::UNKNOWN;

  bool is_authenticated =
      SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated();

  bool enabled =
      (enabled_via_field_trial && is_authenticated) || enabled_via_trunk_build;

#if defined(ENABLE_APP_LIST) && defined(OS_CHROMEOS)
  // Don't load if newer trial is running (== new extension id is available).
  std::string ignored_extension_id;
  if (GetGoogleNowExtensionId(&ignored_extension_id)) {
    enabled = false;
  }
#endif  // defined(ENABLE_APP_LIST) && defined(OS_CHROMEOS)

  const int google_now_manifest_id = IDR_GOOGLE_NOW_MANIFEST;
  const base::FilePath root_directory =
      base::FilePath(FILE_PATH_LITERAL("google_now"));
  if (enabled) {
    Add(google_now_manifest_id, root_directory);
  } else {
    DeleteData(google_now_manifest_id, root_directory);
  }
#endif  // defined(ENABLE_GOOGLE_NOW)
}

#if defined(OS_CHROMEOS)
void ComponentLoader::AddChromeVoxExtension(
    const base::Closure& done_cb) {
  base::FilePath resources_path;
  CHECK(PathService::Get(chrome::DIR_RESOURCES, &resources_path));

  base::FilePath chromevox_path =
      resources_path.Append(extension_misc::kChromeVoxExtensionPath);

  const base::FilePath::CharType* manifest_filename =
      IsNormalSession() ? extensions::kManifestFilename
                        : extension_misc::kGuestManifestFilename;
  AddWithManifestFile(
      manifest_filename,
      chromevox_path,
      extension_misc::kChromeVoxExtensionId,
      done_cb);
}

void ComponentLoader::AddChromeOsSpeechSynthesisExtension() {
  const base::FilePath::CharType* manifest_filename =
      IsNormalSession() ? extensions::kManifestFilename
                        : extension_misc::kGuestManifestFilename;
  AddWithManifestFile(
      manifest_filename,
      base::FilePath(extension_misc::kSpeechSynthesisExtensionPath),
      extension_misc::kSpeechSynthesisExtensionId,
      base::Bind(&ComponentLoader::EnableFileSystemInGuestMode,
                 weak_factory_.GetWeakPtr(),
                 extension_misc::kChromeVoxExtensionId));
}
#endif

void ComponentLoader::AddWithNameAndDescription(
    int manifest_resource_id,
    const base::FilePath& root_directory,
    const std::string& name_string,
    const std::string& description_string) {
  if (!ignore_whitelist_for_testing_ &&
      !IsComponentExtensionWhitelisted(manifest_resource_id))
    return;

  std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          manifest_resource_id).as_string();

  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  base::DictionaryValue* manifest = ParseManifest(manifest_contents);

  if (manifest) {
    manifest->SetString(manifest_keys::kName, name_string);
    manifest->SetString(manifest_keys::kDescription, description_string);
    Add(manifest, root_directory, true);
  }
}

void ComponentLoader::AddChromeApp() {
#if defined(ENABLE_APP_LIST)
  AddWithNameAndDescription(
      IDR_CHROME_APP_MANIFEST, base::FilePath(FILE_PATH_LITERAL("chrome_app")),
      l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME),
      l10n_util::GetStringUTF8(IDS_CHROME_SHORTCUT_DESCRIPTION));
#endif
}

void ComponentLoader::AddKeyboardApp() {
#if defined(OS_CHROMEOS)
  Add(IDR_KEYBOARD_MANIFEST, base::FilePath(FILE_PATH_LITERAL("keyboard")));
#endif
}

void ComponentLoader::AddWebStoreApp() {
#if defined(OS_CHROMEOS)
  if (!IsNormalSession())
    return;
#endif

  AddWithNameAndDescription(
      IDR_WEBSTORE_MANIFEST, base::FilePath(FILE_PATH_LITERAL("web_store")),
      l10n_util::GetStringUTF8(IDS_WEBSTORE_NAME_STORE),
      l10n_util::GetStringUTF8(IDS_WEBSTORE_APP_DESCRIPTION));
}

scoped_refptr<const Extension> ComponentLoader::CreateExtension(
    const ComponentExtensionInfo& info, std::string* utf8_error) {
  // TODO(abarth): We should REQUIRE_MODERN_MANIFEST_VERSION once we've updated
  //               our component extensions to the new manifest version.
  int flags = Extension::REQUIRE_KEY;
  return Extension::Create(
      info.root_directory,
      Manifest::COMPONENT,
      *info.manifest,
      flags,
      utf8_error);
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
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
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

#if defined(ENABLE_PLUGINS)
  Add(pdf_extension_util::GetManifest(),
      base::FilePath(FILE_PATH_LITERAL("pdf")));
#endif
}

void ComponentLoader::AddDefaultComponentExtensionsForKioskMode(
    bool skip_session_components) {
  // No component extension for kiosk app launch splash screen.
  if (skip_session_components)
    return;

  // Component extensions needed for kiosk apps.
  AddFileManagerExtension();

  // Add virtual keyboard.
  AddKeyboardApp();

#if defined(ENABLE_PLUGINS)
  Add(pdf_extension_util::GetManifest(),
      base::FilePath(FILE_PATH_LITERAL("pdf")));
#endif
}

void ComponentLoader::AddDefaultComponentExtensionsWithBackgroundPages(
    bool skip_session_components) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

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
  AddWithNameAndDescription(
      IDR_GENIUS_APP_MANIFEST, base::FilePath(FILE_PATH_LITERAL(
                                   "/usr/share/chromeos-assets/genius_app")),
      l10n_util::GetStringUTF8(IDS_GENIUS_APP_NAME),
      l10n_util::GetStringFUTF8(IDS_GENIUS_APP_DESCRIPTION,
                                ash::GetChromeOSDeviceName()));
#endif

  if (!skip_session_components) {
    AddVideoPlayerExtension();
    AddAudioPlayerExtension();
    AddFileManagerExtension();
    AddGalleryExtension();
    AddWebstoreWidgetExtension();

    AddHangoutServicesExtension();
    AddHotwordAudioVerificationApp();
    AddHotwordHelperExtension();
    AddImageLoaderExtension();
    AddGoogleNowExtension();

    bool install_feedback = enable_background_extensions_during_testing;
#if defined(GOOGLE_CHROME_BUILD)
    install_feedback = true;
#endif  // defined(GOOGLE_CHROME_BUILD)
    if (install_feedback)
      Add(IDR_FEEDBACK_MANIFEST, base::FilePath(FILE_PATH_LITERAL("feedback")));

#if defined(ENABLE_SETTINGS_APP)
    Add(IDR_SETTINGS_APP_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("settings_app")));
#endif
  }

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

#if defined(GOOGLE_CHROME_BUILD)
#if !defined(OS_CHROMEOS)  // http://crbug.com/314799
  AddNetworkSpeechSynthesisExtension();
#endif

#endif  // defined(GOOGLE_CHROME_BUILD)

  Add(IDR_CRYPTOTOKEN_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("cryptotoken")));
}

void ComponentLoader::DeleteData(int manifest_resource_id,
                                 const base::FilePath& root_directory) {
  std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          manifest_resource_id).as_string();
  base::DictionaryValue* manifest = ParseManifest(manifest_contents);
  if (!manifest)
    return;

  ComponentExtensionInfo info(manifest, root_directory);
  std::string error;
  scoped_refptr<const Extension> extension(CreateExtension(info, &error));
  if (!extension.get()) {
    LOG(ERROR) << error;
    return;
  }

  DataDeleter::StartDeleting(
      profile_, extension.get(), base::Bind(base::DoNothing));
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
  if (!IsNormalSession()) {
    // TODO(dpolukhin): Hack to enable HTML5 temporary file system for
    // the extension. Some component extensions don't work without temporary
    // file system access. Make sure temporary file system is enabled in the off
    // the record browser context (as that is the one used in guest session).
    content::BrowserContext* off_the_record_context =
        ExtensionsBrowserClient::Get()->GetOffTheRecordContext(profile_);
    GURL site = content::SiteInstance::GetSiteForURL(
        off_the_record_context, Extension::GetBaseURLFromExtensionId(id));
    storage::FileSystemContext* file_system_context =
        content::BrowserContext::GetStoragePartitionForSite(
            off_the_record_context, site)->GetFileSystemContext();
    file_system_context->EnableTemporaryFileSystemInIncognito();
  }
#endif
}

#if defined(OS_CHROMEOS)
void ComponentLoader::AddWithManifestFile(
    const base::FilePath::CharType* manifest_filename,
    const base::FilePath& root_directory,
    const char* extension_id,
    const base::Closure& done_cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&LoadManifestOnFileThread, root_directory, manifest_filename),
      base::Bind(&ComponentLoader::FinishAddWithManifestFile,
                 weak_factory_.GetWeakPtr(),
                 root_directory,
                 extension_id,
                 done_cb));
}

void ComponentLoader::FinishAddWithManifestFile(
    const base::FilePath& root_directory,
    const char* extension_id,
    const base::Closure& done_cb,
    scoped_ptr<base::DictionaryValue> manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!manifest)
    return;  // Error already logged.
  std::string actual_extension_id = Add(
      manifest.release(),
      root_directory,
      false);
  CHECK_EQ(extension_id, actual_extension_id);
  if (!done_cb.is_null())
    done_cb.Run();
}
#endif

}  // namespace extensions
