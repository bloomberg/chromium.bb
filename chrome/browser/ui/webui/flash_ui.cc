// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flash_ui.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/crash_upload_list.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/gpu_info.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using content::GpuDataManager;
using content::PluginService;
using content::UserMetricsAction;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

ChromeWebUIDataSource* CreateFlashUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIFlashHost);

  source->AddLocalizedString("loadingMessage", IDS_FLASH_LOADING_MESSAGE);
  source->AddLocalizedString("flashLongTitle", IDS_FLASH_TITLE_MESSAGE);
  source->set_json_path("strings.js");
  source->add_resource_path("about_flash.js", IDR_ABOUT_FLASH_JS);
  source->set_default_resource(IDR_ABOUT_FLASH_HTML);
  return source;
}

const int kTimeout = 8 * 1000;  // 8 seconds.

////////////////////////////////////////////////////////////////////////////////
//
// FlashDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for JavaScript messages for the about:flags page.
class FlashDOMHandler : public WebUIMessageHandler,
                        public CrashUploadList::Delegate,
                        public content::GpuDataManagerObserver {
 public:
  FlashDOMHandler();
  virtual ~FlashDOMHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // CrashUploadList::Delegate implementation.
  virtual void OnCrashListAvailable() OVERRIDE;

  // GpuDataManager::Observer implementation.
  virtual void OnGpuInfoUpdate() OVERRIDE;
  virtual void OnVideoMemoryUsageStatsUpdate(
      const content::GPUVideoMemoryUsageStats& video_memory_usage_stats)
          OVERRIDE {}

  // Callback for the "requestFlashInfo" message.
  void HandleRequestFlashInfo(const ListValue* args);

  // Callback for the Flash plugin information.
  void OnGotPlugins(const std::vector<webkit::WebPluginInfo>& plugins);

 private:
  // Called when we think we might have enough information to return data back
  // to the page.
  void MaybeRespondToPage();

  // In certain cases we might not get called back from the GPU process so we
  // set an upper limit on the time we wait. This function gets called when the
  // time has passed. This actually doesn't prevent the rest of the information
  // to appear later, the page will just reflow when more information becomes
  // available.
  void OnTimeout();

  // A timer to keep track of when the data fetching times out.
  base::OneShotTimer<FlashDOMHandler> timeout_;

  // Crash list.
  scoped_refptr<CrashUploadList> upload_list_;

  // Factory for the creating refs in callbacks.
  base::WeakPtrFactory<FlashDOMHandler> weak_ptr_factory_;

  // Whether the list of all crashes is available.
  bool crash_list_available_;
  // Whether the page has requested data.
  bool page_has_requested_data_;
  // Whether the GPU data has been collected.
  bool has_gpu_info_;
  // Whether the plugin information is ready.
  bool has_plugin_info_;

  DISALLOW_COPY_AND_ASSIGN(FlashDOMHandler);
};

FlashDOMHandler::FlashDOMHandler()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      crash_list_available_(false),
      page_has_requested_data_(false),
      has_gpu_info_(false),
      has_plugin_info_(false) {
  // Request Crash data asynchronously.
  upload_list_ = CrashUploadList::Create(this);
  upload_list_->LoadCrashListAsynchronously();

  // Watch for changes in GPUInfo.
  GpuDataManager::GetInstance()->AddObserver(this);

  // Tell GpuDataManager it should have full GpuInfo. If the
  // GPU process has not run yet, this will trigger its launch.
  GpuDataManager::GetInstance()->RequestCompleteGpuInfoIfNeeded();

  // GPU access might not be allowed at all, which will cause us not to get a
  // call back.
  if (!GpuDataManager::GetInstance()->GpuAccessAllowed())
    OnGpuInfoUpdate();

  PluginService::GetInstance()->GetPlugins(base::Bind(
      &FlashDOMHandler::OnGotPlugins, weak_ptr_factory_.GetWeakPtr()));

  // And lastly, we fire off a timer to make sure we never get stuck at the
  // "Loading..." message.
  timeout_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeout),
                 this, &FlashDOMHandler::OnTimeout);
}

FlashDOMHandler::~FlashDOMHandler() {
  GpuDataManager::GetInstance()->RemoveObserver(this);
}

void FlashDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestFlashInfo",
      base::Bind(&FlashDOMHandler::HandleRequestFlashInfo,
                 base::Unretained(this)));
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

void FlashDOMHandler::OnGotPlugins(
    const std::vector<webkit::WebPluginInfo>& plugins) {
  has_plugin_info_ = true;
  MaybeRespondToPage();
}

void FlashDOMHandler::OnTimeout() {
  // We don't set page_has_requested_data_ because that is guaranteed to appear
  // and we shouldn't be responding to the page before then.
  has_gpu_info_ = true;
  crash_list_available_ = true;
  has_plugin_info_ = true;
  MaybeRespondToPage();
}

void FlashDOMHandler::MaybeRespondToPage() {
  // We don't reply until everything is ready. The page is showing a 'loading'
  // message until then. If you add criteria to this list, please update the
  // function OnTimeout() as well.
  if (!page_has_requested_data_ || !crash_list_available_ || !has_gpu_info_ ||
      !has_plugin_info_) {
    return;
  }

  timeout_.Stop();

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
          chrome::VersionInfo::GetVersionStringModifier() + ")");

  // OS version information.
  std::string os_label = version_info.OSType();
#if defined(OS_WIN)
  base::win::OSInfo* os = base::win::OSInfo::GetInstance();
  switch (os->version()) {
    case base::win::VERSION_XP: os_label += " XP"; break;
    case base::win::VERSION_SERVER_2003:
      os_label += " Server 2003 or XP Pro 64 bit";
      break;
    case base::win::VERSION_VISTA: os_label += " Vista or Server 2008"; break;
    case base::win::VERSION_WIN7: os_label += " 7 or Server 2008 R2"; break;
    case base::win::VERSION_WIN8: os_label += " 8 or Server 2012"; break;
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
  std::vector<webkit::WebPluginInfo> info_array;
  PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), "application/x-shockwave-flash", false, &info_array, NULL);
  string16 flash_version;
  if (info_array.empty()) {
    AddPair(list, ASCIIToUTF16("Flash plugin"), "Disabled");
  } else {
    PluginPrefs* plugin_prefs =
        PluginPrefs::GetForProfile(Profile::FromWebUI(web_ui()));
    for (size_t i = 0; i < info_array.size(); ++i) {
      if (plugin_prefs->IsPluginEnabled(info_array[i])) {
        flash_version = info_array[i].version + ASCIIToUTF16(" ") +
                        info_array[i].path.LossyDisplayName();
        if (i != 0)
          flash_version += ASCIIToUTF16(" (not used)");
        AddPair(list, ASCIIToUTF16("Flash plugin"), flash_version);
      }
    }
  }

  // Crash information.
  AddPair(list, string16(), "--- Crash data ---");
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
  AddPair(list, string16(), "--- GPU information ---");
  content::GPUInfo gpu_info = GpuDataManager::GetInstance()->GetGPUInfo();

  if (!GpuDataManager::GetInstance()->GpuAccessAllowed())
    AddPair(list, ASCIIToUTF16("WARNING:"), "GPU access is not allowed");
#if defined(OS_WIN)
  const content::DxDiagNode& node = gpu_info.dx_diagnostics;
  for (std::map<std::string, content::DxDiagNode>::const_iterator it =
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

  AddPair(list, string16(), "--- GPU driver, more information ---");
  AddPair(list,
          ASCIIToUTF16("Vendor Id"),
          base::StringPrintf("0x%04x", gpu_info.gpu.vendor_id));
  AddPair(list,
          ASCIIToUTF16("Device Id"),
          base::StringPrintf("0x%04x", gpu_info.gpu.device_id));
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
  web_ui()->CallJavascriptFunction("returnFlashInfo", flashInfo);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// FlashUI
//
///////////////////////////////////////////////////////////////////////////////

FlashUI::FlashUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::RecordAction(
      UserMetricsAction("ViewAboutFlash"));

  web_ui->AddMessageHandler(new FlashDOMHandler());

  // Set up the about:flash source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, CreateFlashUIHTMLSource());
}

// static
base::RefCountedMemory* FlashUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  // Use the default icon for now.
  return NULL;
}
