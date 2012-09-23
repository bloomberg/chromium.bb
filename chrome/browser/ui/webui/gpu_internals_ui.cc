// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/gpu_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/crash_upload_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/compositor_util.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_info.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "third_party/angle/src/common/version.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::GpuDataManager;
using content::GpuFeatureType;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

struct GpuFeatureInfo {
  std::string name;
  uint32 blocked;
  bool disabled;
  std::string disabled_description;
  bool fallback_to_software;
};

ChromeWebUIDataSource* CreateGpuHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIGpuInternalsHost);

  source->set_json_path("strings.js");
  source->add_resource_path("gpu_internals.js", IDR_GPU_INTERNALS_JS);
  source->set_default_resource(IDR_GPU_INTERNALS_HTML);
  return source;
}

DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    const std::string& value) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("description", desc);
  dict->SetString("value", value);
  return dict;
}

DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    Value* value) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("description", desc);
  dict->Set("value", value);
  return dict;
}

Value* NewStatusValue(const char* name, const char* status) {
  DictionaryValue* value = new DictionaryValue();
  value->SetString("name", name);
  value->SetString("status", status);
  return value;
}

// Output DxDiagNode tree as nested array of {description,value} pairs
ListValue* DxDiagNodeToList(const content::DxDiagNode& node) {
  ListValue* list = new ListValue();
  for (std::map<std::string, std::string>::const_iterator it =
      node.values.begin();
      it != node.values.end();
      ++it) {
    list->Append(NewDescriptionValuePair(it->first, it->second));
  }

  for (std::map<std::string, content::DxDiagNode>::const_iterator it =
      node.children.begin();
      it != node.children.end();
      ++it) {
    ListValue* sublist = DxDiagNodeToList(it->second);
    list->Append(NewDescriptionValuePair(it->first, sublist));
  }
  return list;
}

std::string GPUDeviceToString(const content::GPUInfo::GPUDevice& gpu) {
  std::string vendor = base::StringPrintf("0x%04x", gpu.vendor_id);
  if (!gpu.vendor_string.empty())
    vendor += " [" + gpu.vendor_string + "]";
  std::string device = base::StringPrintf("0x%04x", gpu.device_id);
  if (!gpu.device_string.empty())
    device += " [" + gpu.device_string + "]";
  return base::StringPrintf(
      "VENDOR = %s, DEVICE= %s", vendor.c_str(), device.c_str());
}

DictionaryValue* GpuInfoAsDictionaryValue() {
  content::GPUInfo gpu_info = GpuDataManager::GetInstance()->GetGPUInfo();
  ListValue* basic_info = new ListValue();
  basic_info->Append(NewDescriptionValuePair(
      "Initialization time",
      base::Int64ToString(gpu_info.initialization_time.InMilliseconds())));
  basic_info->Append(NewDescriptionValuePair(
      "Sandboxed",
      Value::CreateBooleanValue(gpu_info.sandboxed)));
  basic_info->Append(NewDescriptionValuePair(
      "GPU0", GPUDeviceToString(gpu_info.gpu)));
  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    basic_info->Append(NewDescriptionValuePair(
        base::StringPrintf("GPU%d", static_cast<int>(i + 1)),
        GPUDeviceToString(gpu_info.secondary_gpus[i])));
  }
  basic_info->Append(NewDescriptionValuePair(
      "Optimus", Value::CreateBooleanValue(gpu_info.optimus)));
  basic_info->Append(NewDescriptionValuePair(
      "AMD switchable", Value::CreateBooleanValue(gpu_info.amd_switchable)));
  basic_info->Append(NewDescriptionValuePair("Driver vendor",
                                             gpu_info.driver_vendor));
  basic_info->Append(NewDescriptionValuePair("Driver version",
                                             gpu_info.driver_version));
  basic_info->Append(NewDescriptionValuePair("Driver date",
                                             gpu_info.driver_date));
  basic_info->Append(NewDescriptionValuePair("Pixel shader version",
                                             gpu_info.pixel_shader_version));
  basic_info->Append(NewDescriptionValuePair("Vertex shader version",
                                             gpu_info.vertex_shader_version));
  basic_info->Append(NewDescriptionValuePair("GL version",
                                             gpu_info.gl_version));
  basic_info->Append(NewDescriptionValuePair("GL_VENDOR",
                                             gpu_info.gl_vendor));
  basic_info->Append(NewDescriptionValuePair("GL_RENDERER",
                                             gpu_info.gl_renderer));
  basic_info->Append(NewDescriptionValuePair("GL_VERSION",
                                             gpu_info.gl_version_string));
  basic_info->Append(NewDescriptionValuePair("GL_EXTENSIONS",
                                             gpu_info.gl_extensions));

  DictionaryValue* info = new DictionaryValue();
  info->Set("basic_info", basic_info);

#if defined(OS_WIN)
  ListValue* perf_info = new ListValue();
  perf_info->Append(NewDescriptionValuePair(
      "Graphics",
      base::StringPrintf("%.1f", gpu_info.performance_stats.graphics)));
  perf_info->Append(NewDescriptionValuePair(
      "Gaming",
      base::StringPrintf("%.1f", gpu_info.performance_stats.gaming)));
  perf_info->Append(NewDescriptionValuePair(
      "Overall",
      base::StringPrintf("%.1f", gpu_info.performance_stats.overall)));
  info->Set("performance_info", perf_info);

  Value* dx_info;
  if (gpu_info.dx_diagnostics.children.size())
    dx_info = DxDiagNodeToList(gpu_info.dx_diagnostics);
  else
    dx_info = Value::CreateNullValue();
  info->Set("diagnostics", dx_info);
#endif

  return info;
}

// Determine if accelerated-2d-canvas is supported, which depends on whether
// lose_context could happen and whether skia is the backend.
bool SupportsAccelerated2dCanvas() {
  if (GpuDataManager::GetInstance()->GetGPUInfo().can_lose_context)
    return false;
#if defined(USE_SKIA)
  return true;
#else
  return false;
#endif
}

Value* GetFeatureStatus() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool gpu_access_blocked = !GpuDataManager::GetInstance()->GpuAccessAllowed();

  uint32 flags = GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  DictionaryValue* status = new DictionaryValue();

  const GpuFeatureInfo kGpuFeatureInfo[] = {
      {
          "2d_canvas",
          flags & content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
          command_line.HasSwitch(switches::kDisableAccelerated2dCanvas) ||
          !SupportsAccelerated2dCanvas(),
          "Accelerated 2D canvas is unavailable: either disabled at the command"
          " line or not supported by the current system.",
          true
      },
      {
          "compositing",
          flags & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING,
          command_line.HasSwitch(switches::kDisableAcceleratedCompositing),
          "Accelerated compositing has been disabled, either via about:flags or"
          " command line. This adversely affects performance of all hardware"
          " accelerated features.",
          true
      },
      {
          "3d_css",
          flags & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING,
          command_line.HasSwitch(switches::kDisableAcceleratedLayers),
          "Accelerated layers have been disabled at the command line.",
          false
      },
      {
          "css_animation",
          flags & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING,
          command_line.HasSwitch(switches::kDisableThreadedAnimation) ||
          command_line.HasSwitch(switches::kDisableAcceleratedCompositing),
          "Accelerated CSS animation has been disabled at the command line.",
          true
      },
      {
          "webgl",
          flags & content::GPU_FEATURE_TYPE_WEBGL,
#if defined(OS_ANDROID)
          !command_line.HasSwitch(switches::kEnableExperimentalWebGL),
#else
          command_line.HasSwitch(switches::kDisableExperimentalWebGL),
#endif
          "WebGL has been disabled, either via about:flags or command line.",
          false
      },
      {
          "multisampling",
          flags & content::GPU_FEATURE_TYPE_MULTISAMPLING,
          command_line.HasSwitch(switches::kDisableGLMultisampling),
          "Multisampling has been disabled, either via about:flags or command"
          " line.",
          false
      },
      {
          "flash_3d",
          flags & content::GPU_FEATURE_TYPE_FLASH3D,
          command_line.HasSwitch(switches::kDisableFlash3d),
          "Using 3d in flash has been disabled, either via about:flags or"
          " command line.",
          false
      },
      {
          "flash_stage3d",
          flags & content::GPU_FEATURE_TYPE_FLASH_STAGE3D,
          command_line.HasSwitch(switches::kDisableFlashStage3d),
          "Using Stage3d in Flash has been disabled, either via about:flags or"
          " command line.",
          false
      },
      {
          "texture_sharing",
          flags & content::GPU_FEATURE_TYPE_TEXTURE_SHARING,
          command_line.HasSwitch(switches::kDisableImageTransportSurface),
          "Sharing textures between processes has been disabled, either via"
          " about:flags or command line.",
          false
      },
      {
          "video_decode",
          flags & content::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
          command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode),
          "Accelerated video decode has been disabled, either via about:flags"
          " or command line.",
          true
      }
  };
  const size_t kNumFeatures = sizeof(kGpuFeatureInfo) / sizeof(GpuFeatureInfo);

  // Build the feature_status field.
  {
    ListValue* feature_status_list = new ListValue();

    for (size_t i = 0; i < kNumFeatures; ++i) {
      std::string status;
      if (kGpuFeatureInfo[i].disabled) {
        status = "disabled";
        if (kGpuFeatureInfo[i].name == "css_animation") {
          status += "_software_animated";
        } else {
          if (kGpuFeatureInfo[i].fallback_to_software)
            status += "_software";
          else
            status += "_off";
        }
      } else if (GpuDataManager::GetInstance()->ShouldUseSoftwareRendering()) {
        status = "unavailable_software";
      } else if (kGpuFeatureInfo[i].blocked ||
                 gpu_access_blocked) {
        status = "unavailable";
        if (kGpuFeatureInfo[i].fallback_to_software)
          status += "_software";
        else
          status += "_off";
      } else {
        status = "enabled";
        if (kGpuFeatureInfo[i].name == "webgl" &&
            (command_line.HasSwitch(switches::kDisableAcceleratedCompositing) ||
             (flags & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING)))
          status += "_readback";
        bool has_thread = content::IsThreadedCompositingEnabled();
        if (kGpuFeatureInfo[i].name == "compositing") {
          bool force_compositing =
              content::IsForceCompositingModeEnabled();
          if (force_compositing)
            status += "_force";
          if (has_thread)
            status += "_threaded";
        }
        if (kGpuFeatureInfo[i].name == "css_animation") {
          if (has_thread)
            status = "accelerated_threaded";
          else
            status = "accelerated";
        }
      }
      feature_status_list->Append(
          NewStatusValue(kGpuFeatureInfo[i].name.c_str(), status.c_str()));
    }
    content::GPUInfo gpu_info = GpuDataManager::GetInstance()->GetGPUInfo();
    if (gpu_info.secondary_gpus.size() > 0 ||
        gpu_info.optimus || gpu_info.amd_switchable) {
      std::string gpu_switching;
      switch (GpuDataManager::GetInstance()->GetGpuSwitchingOption()) {
        case content::GPU_SWITCHING_AUTOMATIC:
          gpu_switching = "gpu_switching_automatic";
          break;
        case content::GPU_SWITCHING_FORCE_DISCRETE:
          gpu_switching = "gpu_switching_force_discrete";
          break;
        case content::GPU_SWITCHING_FORCE_INTEGRATED:
          gpu_switching = "gpu_switching_force_integrated";
          break;
        default:
          break;
      }
      feature_status_list->Append(
          NewStatusValue("gpu_switching", gpu_switching.c_str()));
    }
    status->Set("featureStatus", feature_status_list);
  }

  // Build the problems list.
  {
    ListValue* problem_list =
        GpuDataManager::GetInstance()->GetBlacklistReasons();

    if (gpu_access_blocked) {
      DictionaryValue* problem = new DictionaryValue();
      problem->SetString("description",
          "GPU process was unable to boot. Access to GPU disallowed.");
      problem->Set("crBugs", new ListValue());
      problem->Set("webkitBugs", new ListValue());
      problem_list->Append(problem);
    }

    for (size_t i = 0; i < kNumFeatures; ++i) {
      if (kGpuFeatureInfo[i].disabled) {
        DictionaryValue* problem = new DictionaryValue();
        problem->SetString(
            "description", kGpuFeatureInfo[i].disabled_description);
        problem->Set("crBugs", new ListValue());
        problem->Set("webkitBugs", new ListValue());
        problem_list->Append(problem);
      }
    }

    status->Set("problems", problem_list);
  }

  return status;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class GpuMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<GpuMessageHandler>,
      public content::GpuDataManagerObserver,
      public CrashUploadList::Delegate {
 public:
  GpuMessageHandler();
  virtual ~GpuMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // GpuDataManagerObserver implementation.
  virtual void OnGpuInfoUpdate() OVERRIDE;
  virtual void OnVideoMemoryUsageStatsUpdate(
      const content::GPUVideoMemoryUsageStats& video_memory_usage_stats)
          OVERRIDE {}

  // CrashUploadList::Delegate implemenation.
  virtual void OnCrashListAvailable() OVERRIDE;

  // Messages
  void OnBrowserBridgeInitialized(const ListValue* list);
  void OnCallAsync(const ListValue* list);

  // Submessages dispatched from OnCallAsync
  Value* OnRequestClientInfo(const ListValue* list);
  Value* OnRequestLogMessages(const ListValue* list);
  Value* OnRequestCrashList(const ListValue* list);

  // Executes the javascript function |function_name| in the renderer, passing
  // it the argument |value|.
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value* value);

 private:
  scoped_refptr<CrashUploadList> crash_list_;
  bool crash_list_available_;

  // True if observing the GpuDataManager (re-attaching as observer would
  // DCHECK).
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// GpuMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

GpuMessageHandler::GpuMessageHandler()
    : crash_list_available_(false),
      observing_(false) {
  crash_list_ = CrashUploadList::Create(this);
}

GpuMessageHandler::~GpuMessageHandler() {
  GpuDataManager::GetInstance()->RemoveObserver(this);
  crash_list_->ClearDelegate();
}

/* BrowserBridge.callAsync prepends a requestID to these messages. */
void GpuMessageHandler::RegisterMessages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  crash_list_->LoadCrashListAsynchronously();

  web_ui()->RegisterMessageCallback("browserBridgeInitialized",
      base::Bind(&GpuMessageHandler::OnBrowserBridgeInitialized,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("callAsync",
      base::Bind(&GpuMessageHandler::OnCallAsync,
                 base::Unretained(this)));
}

void GpuMessageHandler::OnCallAsync(const ListValue* args) {
  DCHECK_GE(args->GetSize(), static_cast<size_t>(2));
  // unpack args into requestId, submessage and submessageArgs
  bool ok;
  const Value* requestId;
  ok = args->Get(0, &requestId);
  DCHECK(ok);

  std::string submessage;
  ok = args->GetString(1, &submessage);
  DCHECK(ok);

  ListValue* submessageArgs = new ListValue();
  for (size_t i = 2; i < args->GetSize(); ++i) {
    const Value* arg;
    ok = args->Get(i, &arg);
    DCHECK(ok);

    Value* argCopy = arg->DeepCopy();
    submessageArgs->Append(argCopy);
  }

  // call the submessage handler
  Value* ret = NULL;
  if (submessage == "requestClientInfo") {
    ret = OnRequestClientInfo(submessageArgs);
  } else if (submessage == "requestLogMessages") {
    ret = OnRequestLogMessages(submessageArgs);
  } else if (submessage == "requestCrashList") {
    ret = OnRequestCrashList(submessageArgs);
  } else {  // unrecognized submessage
    NOTREACHED();
    delete submessageArgs;
    return;
  }
  delete submessageArgs;

  // call BrowserBridge.onCallAsyncReply with result
  if (ret) {
    web_ui()->CallJavascriptFunction("browserBridge.onCallAsyncReply",
        *requestId,
        *ret);
    delete ret;
  } else {
    web_ui()->CallJavascriptFunction("browserBridge.onCallAsyncReply",
        *requestId);
  }
}

void GpuMessageHandler::OnBrowserBridgeInitialized(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Watch for changes in GPUInfo
  if (!observing_)
    GpuDataManager::GetInstance()->AddObserver(this);
  observing_ = true;

  // Tell GpuDataManager it should have full GpuInfo. If the
  // Gpu process has not run yet, this will trigger its launch.
  GpuDataManager::GetInstance()->RequestCompleteGpuInfoIfNeeded();

  // Run callback immediately in case the info is ready and no update in the
  // future.
  OnGpuInfoUpdate();
}

Value* GpuMessageHandler::OnRequestClientInfo(const ListValue* list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DictionaryValue* dict = new DictionaryValue();

  chrome::VersionInfo version_info;

  if (!version_info.is_valid()) {
    DLOG(ERROR) << "Unable to create chrome::VersionInfo";
  } else {
    // We have everything we need to send the right values.
    dict->SetString("version", version_info.Version());
    dict->SetString("cl", version_info.LastChange());
    dict->SetString("version_mod",
        chrome::VersionInfo::GetVersionStringModifier());
    dict->SetString("official",
        l10n_util::GetStringUTF16(
            version_info.IsOfficialBuild() ?
            IDS_ABOUT_VERSION_OFFICIAL :
            IDS_ABOUT_VERSION_UNOFFICIAL));

    dict->SetString("command_line",
        CommandLine::ForCurrentProcess()->GetCommandLineString());
  }

  dict->SetString("operating_system",
                  base::SysInfo::OperatingSystemName() + " " +
                  base::SysInfo::OperatingSystemVersion());
  dict->SetString("angle_revision", base::UintToString(BUILD_REVISION));
#if defined(USE_SKIA)
  dict->SetString("graphics_backend", "Skia");
#else
  dict->SetString("graphics_backend", "Core Graphics");
#endif
  dict->SetString("blacklist_version",
      GpuDataManager::GetInstance()->GetBlacklistVersion());

  return dict;
}

Value* GpuMessageHandler::OnRequestLogMessages(const ListValue*) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return GpuDataManager::GetInstance()->GetLogMessages();
}

Value* GpuMessageHandler::OnRequestCrashList(const ListValue*) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!CrashesUI::CrashReportingEnabled()) {
    // We need to return an empty list instead of NULL.
    return new ListValue;
  }
  if (!crash_list_available_) {
    // If we are still obtaining crash list, then return null so another
    // request will be scheduled.
    return NULL;
  }

  ListValue* list_value = new ListValue;
  std::vector<CrashUploadList::CrashInfo> crashes;
  crash_list_->GetUploadedCrashes(50, &crashes);
  for (std::vector<CrashUploadList::CrashInfo>::iterator i = crashes.begin();
       i != crashes.end(); ++i) {
    DictionaryValue* crash = new DictionaryValue();
    crash->SetString("id", i->crash_id);
    crash->SetString("time",
                     base::TimeFormatFriendlyDateAndTime(i->crash_time));
    list_value->Append(crash);
  }
  return list_value;
}

void GpuMessageHandler::OnGpuInfoUpdate() {
  // Get GPU Info.
  scoped_ptr<base::DictionaryValue> gpu_info_val(
      GpuInfoAsDictionaryValue());

  // Add in blacklisting features
  Value* feature_status = GetFeatureStatus();
  if (feature_status)
    gpu_info_val->Set("featureStatus", feature_status);

  // Send GPU Info to javascript.
  web_ui()->CallJavascriptFunction("browserBridge.onGpuInfoUpdate",
      *(gpu_info_val.get()));
}

void GpuMessageHandler::OnCrashListAvailable() {
  crash_list_available_ = true;
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// GpuInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

GpuInternalsUI::GpuInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new GpuMessageHandler());

  // Set up the chrome://gpu-internals/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, CreateGpuHTMLSource());
}
