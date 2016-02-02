// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/voice_search_ui.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/browser/ui/webui/version_handler.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/user_agent.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "grit/browser_resources.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using base::ASCIIToUTF16;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateVoiceSearchUiHtmlSource() {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIVoiceSearchHost);

  html_source->AddLocalizedString("loadingMessage",
                                  IDS_VOICESEARCH_LOADING_MESSAGE);
  html_source->AddLocalizedString("voiceSearchLongTitle",
                                  IDS_VOICESEARCH_TITLE_MESSAGE);

  html_source->SetJsonPath("strings.js");
  html_source->AddResourcePath("about_voicesearch.js",
                               IDR_ABOUT_VOICESEARCH_JS);
  html_source->SetDefaultResource(IDR_ABOUT_VOICESEARCH_HTML);
  return html_source;
}

// Helper functions for collecting a list of key-value pairs that will
// be displayed.
void AddPair16(base::ListValue* list,
               const base::string16& key,
               const base::string16& value) {
  scoped_ptr<base::DictionaryValue> results(new base::DictionaryValue());
  results->SetString("key", key);
  results->SetString("value", value);
  list->Append(results.release());
}

void AddPair(base::ListValue* list,
             const base::StringPiece& key,
             const base::StringPiece& value) {
  AddPair16(list, UTF8ToUTF16(key), UTF8ToUTF16(value));
}

void AddPairBool(base::ListValue* list,
                 const base::StringPiece& key,
                 bool value) {
  AddPair(list, key, value ? "Yes" : "No");
}

// Generate an empty data-pair which acts as a line break.
void AddLineBreak(base::ListValue* list) {
  AddPair(list, "", "");
}

void AddSharedModulePlatformsOnFileThread(base::ListValue* list,
                                          const base::FilePath& path,
                                          base::Closure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  if (!path.empty()) {
    // Display available platforms for shared module.
    base::FilePath platforms_path = path.AppendASCII("_platform_specific");
    base::FileEnumerator enumerator(
        platforms_path, false, base::FileEnumerator::DIRECTORIES);
    base::string16 files;
    for (base::FilePath name = enumerator.Next();
         !name.empty();
         name = enumerator.Next()) {
      files += name.BaseName().LossyDisplayName() + ASCIIToUTF16(" ");
    }
    AddPair16(list,
              ASCIIToUTF16("Shared Module Platforms"),
              files.empty() ? ASCIIToUTF16("undefined") : files);
    AddLineBreak(list);
  }

  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   callback);
}

////////////////////////////////////////////////////////////////////////////////
//
// VoiceSearchDomHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the about:flags page.
class VoiceSearchDomHandler : public WebUIMessageHandler {
 public:
  explicit VoiceSearchDomHandler(Profile* profile)
      : profile_(profile),
        weak_factory_(this) {}

  ~VoiceSearchDomHandler() override {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "requestVoiceSearchInfo",
        base::Bind(&VoiceSearchDomHandler::HandleRequestVoiceSearchInfo,
                   base::Unretained(this)));
  }

 private:
  // Callback for the "requestVoiceSearchInfo" message. No arguments.
  void HandleRequestVoiceSearchInfo(const base::ListValue* args) {
    PopulatePageInformation();
  }

  void ReturnVoiceSearchInfo(scoped_ptr<base::ListValue> info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(info);
    base::DictionaryValue voiceSearchInfo;
    voiceSearchInfo.Set("voiceSearchInfo", info.release());
    web_ui()->CallJavascriptFunction("returnVoiceSearchInfo", voiceSearchInfo);
  }

  // Fill in the data to be displayed on the page.
  void PopulatePageInformation() {
    // Store Key-Value pairs of about-information.
    scoped_ptr<base::ListValue> list(new base::ListValue());

    // Populate information.
    AddOperatingSystemInfo(list.get());
    AddAudioInfo(list.get());
    AddLanguageInfo(list.get());
    AddHotwordInfo(list.get());
    AddAppListInfo(list.get());

    AddExtensionInfo(extension_misc::kHotwordNewExtensionId,
                     "Extension",
                     list.get());

    AddExtensionInfo(extension_misc::kHotwordSharedModuleId,
                     "Shared Module",
                     list.get());

    base::FilePath path;
    extensions::ExtensionSystem* extension_system =
        extensions::ExtensionSystem::Get(profile_);
    if (extension_system) {
      ExtensionService* extension_service =
          extension_system->extension_service();
      const extensions::Extension* extension =
          extension_service->GetExtensionById(
              extension_misc::kHotwordSharedModuleId, true);
      if (extension)
        path = extension->path();
    }
    base::ListValue* raw_list = list.get();
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&AddSharedModulePlatformsOnFileThread, raw_list, path,
                   base::Bind(&VoiceSearchDomHandler::ReturnVoiceSearchInfo,
                              weak_factory_.GetWeakPtr(),
                              base::Passed(std::move(list)))));
  }

  // Adds information regarding the system and chrome version info to list.
  void AddOperatingSystemInfo(base::ListValue* list)  {
    // Obtain the Chrome version info.
    AddPair(list,
            l10n_util::GetStringUTF8(IDS_PRODUCT_NAME),
            version_info::GetVersionNumber() + " (" +
            chrome::GetChannelString() + ")");

    // OS version information.
    std::string os_label = version_info::GetOSType();
#if defined(OS_WIN)
    base::win::OSInfo* os = base::win::OSInfo::GetInstance();
    switch (os->version()) {
      case base::win::VERSION_XP:
        os_label += " XP";
        break;
      case base::win::VERSION_SERVER_2003:
        os_label += " Server 2003 or XP Pro 64 bit";
        break;
      case base::win::VERSION_VISTA:
        os_label += " Vista or Server 2008";
        break;
      case base::win::VERSION_WIN7:
        os_label += " 7 or Server 2008 R2";
        break;
      case base::win::VERSION_WIN8:
        os_label += " 8 or Server 2012";
        break;
      default:
        os_label += " UNKNOWN";
        break;
    }
    os_label += " SP" + base::IntToString(os->service_pack().major);

    if (os->service_pack().minor > 0)
      os_label += "." + base::IntToString(os->service_pack().minor);

    if (os->architecture() == base::win::OSInfo::X64_ARCHITECTURE)
      os_label += " 64 bit";
#endif
    AddPair(list, l10n_util::GetStringUTF8(IDS_VERSION_UI_OS), os_label);

    AddLineBreak(list);
  }

  // Adds information regarding audio to the list.
  void AddAudioInfo(base::ListValue* list) {
    // NaCl and its associated functions are not available on most mobile
    // platforms. ENABLE_EXTENSIONS covers those platforms and hey would not
    // allow Hotwording anyways since it is an extension.
    std::string nacl_enabled = "not available";
#if defined(ENABLE_EXTENSIONS)
    nacl_enabled = "No";
    // Determine if NaCl is available.
    base::FilePath path;
    if (PathService::Get(chrome::FILE_NACL_PLUGIN, &path)) {
      content::WebPluginInfo info;
      PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile_).get();
      if (content::PluginService::GetInstance()->GetPluginInfoByPath(path,
                                                                     &info) &&
          plugin_prefs->IsPluginEnabled(info)) {
        nacl_enabled = "Yes";
      }
    }
#endif

    AddPair(list, "NaCl Enabled", nacl_enabled);

    HotwordService* hotword_service =
        HotwordServiceFactory::GetForProfile(profile_);
    AddPairBool(list, "Microphone Present",
                hotword_service && hotword_service->microphone_available());

    AddPairBool(list, "Audio Capture Allowed",
                profile_->GetPrefs()->GetBoolean(prefs::kAudioCaptureAllowed));

    AddLineBreak(list);
  }

  // Adds information regarding languages to the list.
  void AddLanguageInfo(base::ListValue* list) {
    std::string locale =
#if defined(OS_CHROMEOS)
        // On ChromeOS locale is per-profile.
        profile_->GetPrefs()->GetString(prefs::kApplicationLocale);
#else
        g_browser_process->GetApplicationLocale();
#endif
    AddPair(list, "Current Language", locale);

    AddPair(list,
            "Hotword Previous Language",
            profile_->GetPrefs()->GetString(prefs::kHotwordPreviousLanguage));

    AddLineBreak(list);
  }

  // Adds information specific to the hotword configuration to the list.
  void AddHotwordInfo(base::ListValue* list)  {
    HotwordService* hotword_service =
        HotwordServiceFactory::GetForProfile(profile_);
    AddPairBool(list, "Hotword Module Installable",
                hotword_service && hotword_service->IsHotwordAllowed());

    AddPairBool(list, "Hotword Search Enabled",
                profile_->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled));

    AddPairBool(
        list, "Always-on Hotword Search Enabled",
        profile_->GetPrefs()->GetBoolean(prefs::kHotwordAlwaysOnSearchEnabled));

    AddPairBool(list, "Hotword Audio Logging Enabled",
                hotword_service && hotword_service->IsOptedIntoAudioLogging());

    AddLineBreak(list);
  }

  // Adds information specific to an extension to the list.
  void AddExtensionInfo(const std::string& extension_id,
                        const std::string& name_prefix,
                        base::ListValue* list) {
    DCHECK(!name_prefix.empty());
    std::string version("undefined");
    std::string id("undefined");
    base::FilePath path;

    extensions::ExtensionSystem* extension_system =
        extensions::ExtensionSystem::Get(profile_);
    if (extension_system) {
      ExtensionService* extension_service =
          extension_system->extension_service();
      const extensions::Extension* extension =
          extension_service->GetExtensionById(extension_id, true);
      if (extension) {
        id = extension->id();
        version = extension->VersionString();
        path = extension->path();
      }
    }
    AddPair(list, name_prefix + " Id", id);
    AddPair(list, name_prefix + " Version", version);
    AddPair16(list,
              ASCIIToUTF16(name_prefix + " Path"),
              path.empty() ?
              ASCIIToUTF16("undefined") : path.LossyDisplayName());

    extensions::ExtensionPrefs* extension_prefs =
        extensions::ExtensionPrefs::Get(profile_);
    int pref_state = -1;
    extension_prefs->ReadPrefAsInteger(extension_id, "state", &pref_state);
    std::string state;
    switch (pref_state) {
      case extensions::Extension::DISABLED:
        state = "DISABLED";
        break;
      case extensions::Extension::ENABLED:
        state = "ENABLED";
        break;
      case extensions::Extension::EXTERNAL_EXTENSION_UNINSTALLED:
        state = "EXTERNAL_EXTENSION_UNINSTALLED";
        break;
      default:
        state = "undefined";
    }

    AddPair(list, name_prefix + " State", state);

    AddLineBreak(list);
  }

  // Adds information specific to voice search in the app launcher to the list.
  void AddAppListInfo(base::ListValue* list) {
#if defined (ENABLE_APP_LIST)
    std::string state = "No Start Page Service";
    app_list::StartPageService* start_page_service =
        app_list::StartPageService::Get(profile_);
    if (start_page_service) {
      app_list::SpeechRecognitionState speech_state =
          start_page_service->state();
      switch (speech_state) {
        case app_list::SPEECH_RECOGNITION_OFF:
          state = "SPEECH_RECOGNITION_OFF";
          break;
        case app_list::SPEECH_RECOGNITION_READY:
          state = "SPEECH_RECOGNITION_READY";
          break;
        case app_list::SPEECH_RECOGNITION_HOTWORD_LISTENING:
          state = "SPEECH_RECOGNITION_HOTWORD_LISTENING";
          break;
        case app_list::SPEECH_RECOGNITION_RECOGNIZING:
          state = "SPEECH_RECOGNITION_RECOGNIZING";
          break;
        case app_list::SPEECH_RECOGNITION_IN_SPEECH:
          state = "SPEECH_RECOGNITION_IN_SPEECH";
          break;
        case app_list::SPEECH_RECOGNITION_STOPPING:
          state = "SPEECH_RECOGNITION_STOPPING";
          break;
        case app_list::SPEECH_RECOGNITION_NETWORK_ERROR:
          state = "SPEECH_RECOGNITION_NETWORK_ERROR";
          break;
        default:
          state = "undefined";
      }
    }
    AddPair(list, "Start Page State", state);
    AddLineBreak(list);
#endif
  }

  Profile* profile_;
  base::WeakPtrFactory<VoiceSearchDomHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VoiceSearchDomHandler);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// VoiceSearchUI
//
///////////////////////////////////////////////////////////////////////////////

VoiceSearchUI::VoiceSearchUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(new VoiceSearchDomHandler(profile));

  // Set up the about:voicesearch source.
  content::WebUIDataSource::Add(profile, CreateVoiceSearchUiHtmlSource());
}

VoiceSearchUI::~VoiceSearchUI() {}
