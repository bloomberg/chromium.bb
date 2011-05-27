// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flash_ui.h"

#include "base/i18n/time_formatting.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/crash_upload_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/plugins/npapi/webplugininfo.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

////////////////////////////////////////////////////////////////////////////////
//
// FlashUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

class FlashUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  FlashUIHTMLSource()
      : DataSource(chrome::kChromeUIFlashHost, MessageLoop::current()) {}

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);

  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FlashUIHTMLSource);
};

void FlashUIHTMLSource::StartDataRequest(const std::string& path,
                                             bool is_incognito,
                                             int request_id) {
  // Strings used in the JsTemplate file.
  DictionaryValue localized_strings;
  localized_strings.SetString("loadingMessage",
      l10n_util::GetStringUTF16(IDS_CONFLICTS_LOADING_MESSAGE));
  localized_strings.SetString("flashLongTitle", "About Flash");

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ABOUT_FLASH_HTML));
  std::string full_html(html.data(), html.size());
  jstemplate_builder::AppendJsonHtml(&localized_strings, &full_html);
  jstemplate_builder::AppendI18nTemplateSourceHtml(&full_html);
  jstemplate_builder::AppendI18nTemplateProcessHtml(&full_html);
  jstemplate_builder::AppendJsTemplateSourceHtml(&full_html);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// FlashDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for JavaScript messages for the about:flags page.
class FlashDOMHandler : public WebUIMessageHandler,
                        public CrashUploadList::Delegate {
 public:
  FlashDOMHandler();
  virtual ~FlashDOMHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // CrashUploadList::Delegate implementation.
  virtual void OnCrashListAvailable() OVERRIDE;

  // Callback for the "requestFlashInfo" message.
  void HandleRequestFlashInfo(const ListValue* args);

  // Callback for the GPU information update.
  void OnGpuInfoUpdate();

 private:
  // Called when we think we might have enough information to return data back
  // to the page.
  void MaybeRespondToPage();

  // GPU variables.
  GpuDataManager* gpu_data_manager_;
  Callback0::Type* gpu_info_update_callback_;

  // Crash list.
  scoped_refptr<CrashUploadList> upload_list_;

  // Whether the list of all crashes is available.
  bool crash_list_available_;
  // Whether the page has requested data.
  bool page_has_requested_data_;
  // Whether the GPU data has been collected.
  bool has_gpu_info_;

  DISALLOW_COPY_AND_ASSIGN(FlashDOMHandler);
};

FlashDOMHandler::FlashDOMHandler()
    : crash_list_available_(false),
      page_has_requested_data_(false),
      has_gpu_info_(false) {
  // Request Crash data asynchronously.
  upload_list_ = CrashUploadList::Create(this);
  upload_list_->LoadCrashListAsynchronously();

  // Watch for changes in GPUInfo.
  gpu_data_manager_ = GpuDataManager::GetInstance();
  gpu_info_update_callback_ =
      NewCallback(this, &FlashDOMHandler::OnGpuInfoUpdate);
  gpu_data_manager_->AddGpuInfoUpdateCallback(gpu_info_update_callback_);

  // Tell GpuDataManager it should have full GpuInfo. If the
  // GPU process has not run yet, this will trigger its launch.
  gpu_data_manager_->RequestCompleteGpuInfoIfNeeded();

  const GPUInfo& gpu_info = gpu_data_manager_->gpu_info();
  if (gpu_info.finalized)
    OnGpuInfoUpdate();
}

void FlashDOMHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("requestFlashInfo",
      NewCallback(this, &FlashDOMHandler::HandleRequestFlashInfo));
}

void FlashDOMHandler::OnCrashListAvailable() {
  crash_list_available_ = true;
  MaybeRespondToPage();
}

void AddPair(ListValue* list, const string16& key, const string16& value) {
  DictionaryValue* results = new DictionaryValue();
  results->SetString("key", key);
  results->SetString("value", value);
  list->Append(results);
}

void AddPair(ListValue* list, const string16& key, const std::string& value) {
  AddPair(list, key, ASCIIToUTF16(value));
}

void FlashDOMHandler::HandleRequestFlashInfo(const ListValue* args) {
  page_has_requested_data_ = true;
  MaybeRespondToPage();
}

void FlashDOMHandler::OnGpuInfoUpdate() {
  has_gpu_info_ = true;
  MaybeRespondToPage();
}

void FlashDOMHandler::MaybeRespondToPage() {
  // We don't reply until everything is ready. The page is showing a 'loading'
  // message until then.
  if (!page_has_requested_data_ || !crash_list_available_ || !has_gpu_info_)
    return;

  // This is code that runs only when the user types in about:flash. We don't
  // need to jump through hoops to offload this to the IO thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Obtain the Chrome version info.
  chrome::VersionInfo version_info;

  ListValue* list = new ListValue();

  // Chrome version information.
  AddPair(list,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          version_info.Version() + " (" +
              platform_util::GetVersionStringModifier() + ")");

  // OS version information.
  std::string os_label = version_info.OSType();
#if defined(OS_WIN)
  base::win::OSInfo* os = base::win::OSInfo::GetInstance();
  switch (os->version()) {
    case base::win::VERSION_XP: os_label += " XP"; break;
    case base::win::VERSION_SERVER_2003:
      os_label += " Server 2003 or XP Pro 64 bit";
      break;
    case base::win::VERSION_VISTA: os_label += " Vista"; break;
    case base::win::VERSION_SERVER_2008: os_label += " Server 2008"; break;
    case base::win::VERSION_WIN7: os_label += " 7"; break;
    default:  os_label += " UNKNOWN"; break;
  }
  os_label += " SP" + base::IntToString(os->service_pack().major);
  if (os->service_pack().minor > 0)
    os_label += "." + base::IntToString(os->service_pack().minor);
  if (os->architecture() == base::win::OSInfo::X64_ARCHITECTURE)
    os_label += " 64 bit";
#endif
  AddPair(list, l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_OS), os_label);

  // Obtain the version of the Flash plugins.
  std::vector<webkit::npapi::WebPluginInfo> info_array;
  webkit::npapi::PluginList::Singleton()->GetPluginInfoArray(
      GURL(), "application/x-shockwave-flash", false, &info_array, NULL);
  string16 flash_version;
  if (info_array.empty()) {
    AddPair(list, ASCIIToUTF16("Flash plugin"), "Disabled");
  } else {
    for (size_t i = 0; i < info_array.size(); ++i) {
      if (webkit::npapi::IsPluginEnabled(info_array[i])) {
        flash_version = info_array[i].version + ASCIIToUTF16(" ") +
                        info_array[i].path.LossyDisplayName();
        if (i != 0)
          flash_version += ASCIIToUTF16(" (not used)");
        AddPair(list, ASCIIToUTF16("Flash plugin"), flash_version);
      }
    }
  }

  // Crash information.
  AddPair(list, ASCIIToUTF16(""), "--- Crash data ---");
  bool crash_reporting_enabled = CrashesUI::CrashReportingEnabled();
  if (crash_reporting_enabled) {
    std::vector<CrashUploadList::CrashInfo> crashes;
    upload_list_->GetUploadedCrashes(10, &crashes);

    for (std::vector<CrashUploadList::CrashInfo>::iterator i = crashes.begin();
         i != crashes.end(); ++i) {
      string16 crash_string(ASCIIToUTF16(i->crash_id));
      crash_string += ASCIIToUTF16(" ");
      crash_string += base::TimeFormatFriendlyDateAndTime(i->crash_time);
      AddPair(list, ASCIIToUTF16("crash id"), crash_string);
    }
  } else {
    AddPair(list, ASCIIToUTF16("Crash Reporting"),
                  "Enable crash reporting to see crash IDs");
    AddPair(list, ASCIIToUTF16("For more details"),
                  chrome::kLearnMoreReportingURL);
  }

  // GPU information.
  AddPair(list, ASCIIToUTF16(""), "--- GPU information ---");
  const GPUInfo& gpu_info = gpu_data_manager_->gpu_info();

#if defined(OS_WIN)
  const DxDiagNode& node = gpu_info.dx_diagnostics;
  for (std::map<std::string, DxDiagNode>::const_iterator it =
           node.children.begin();
       it != node.children.end();
       ++it) {
    for (std::map<std::string, std::string>::const_iterator it2 =
             it->second.values.begin();
         it2 != it->second.values.end();
         ++it2) {
      if (!it2->second.empty()) {
        if (it2->first == "szDescription") {
          AddPair(list, ASCIIToUTF16("Graphics card"), it2->second);
        } else if (it2->first == "szDriverNodeStrongName") {
          AddPair(list, ASCIIToUTF16("Driver name (strong)"), it2->second);
        } else if (it2->first == "szDriverName") {
          AddPair(list, ASCIIToUTF16("Driver display name"), it2->second);
        }
      }
    }
  }
#endif

  AddPair(list, ASCIIToUTF16(""), "--- GPU driver, more information ---");
  AddPair(list,
          ASCIIToUTF16("Vendor Id"),
          base::StringPrintf("0x%04x", gpu_info.vendor_id));
  AddPair(list,
          ASCIIToUTF16("Device Id"),
          base::StringPrintf("0x%04x", gpu_info.device_id));
  AddPair(list, ASCIIToUTF16("Driver vendor"), gpu_info.driver_vendor);
  AddPair(list, ASCIIToUTF16("Driver version"), gpu_info.driver_version);
  AddPair(list, ASCIIToUTF16("Driver date"), gpu_info.driver_date);
  AddPair(list,
          ASCIIToUTF16("Pixel shader version"),
          gpu_info.pixel_shader_version);
  AddPair(list,
          ASCIIToUTF16("Vertex shader version"),
          gpu_info.vertex_shader_version);
  AddPair(list, ASCIIToUTF16("GL version"), gpu_info.gl_version);
  AddPair(list, ASCIIToUTF16("GL_VENDOR"), gpu_info.gl_vendor);
  AddPair(list, ASCIIToUTF16("GL_RENDERER"), gpu_info.gl_renderer);
  AddPair(list, ASCIIToUTF16("GL_VERSION"), gpu_info.gl_version_string);
  AddPair(list, ASCIIToUTF16("GL_EXTENSIONS"), gpu_info.gl_extensions);

  DictionaryValue flashInfo;
  flashInfo.Set("flashInfo", list);
  web_ui_->CallJavascriptFunction("returnFlashInfo", flashInfo);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// FlashUI
//
///////////////////////////////////////////////////////////////////////////////

FlashUI::FlashUI(TabContents* contents) : WebUI(contents) {
  UserMetrics::RecordAction(
      UserMetricsAction("ViewAboutFlash"));

  AddMessageHandler((new FlashDOMHandler())->Attach(this));

  FlashUIHTMLSource* html_source = new FlashUIHTMLSource();

  // Set up the about:flash source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

// static
RefCountedMemory* FlashUI::GetFaviconResourceBytes() {
  // Use the default icon for now.
  return NULL;
}
