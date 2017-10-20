// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_internals_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_lists_version.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "skia/ext/skia_commit_hash.h"
#include "third_party/angle/src/common/version.h"
#include "third_party/skia/include/core/SkMilestone.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gl/gpu_switching_manager.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#include "ui/gfx/win/physical_size.h"
#endif

#if defined(OS_LINUX) && defined(USE_X11)
#include "ui/base/x/x11_util.h"       // nogncheck
#include "ui/gfx/x/x11_atom_cache.h"  // nogncheck
#endif

namespace content {
namespace {

WebUIDataSource* CreateGpuHTMLSource() {
  WebUIDataSource* source = WebUIDataSource::Create(kChromeUIGpuHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("gpu_internals.js", IDR_GPU_INTERNALS_JS);
  source->SetDefaultResource(IDR_GPU_INTERNALS_HTML);
  source->UseGzip();
  return source;
}

std::unique_ptr<base::DictionaryValue> NewDescriptionValuePair(
    const std::string& desc,
    const std::string& value) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("description", desc);
  dict->SetString("value", value);
  return dict;
}

std::unique_ptr<base::DictionaryValue> NewDescriptionValuePair(
    const std::string& desc,
    std::unique_ptr<base::Value> value) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("description", desc);
  dict->Set("value", std::move(value));
  return dict;
}

#if defined(OS_WIN)
// Output DxDiagNode tree as nested array of {description,value} pairs
std::unique_ptr<base::ListValue> DxDiagNodeToList(const gpu::DxDiagNode& node) {
  auto list = base::MakeUnique<base::ListValue>();
  for (std::map<std::string, std::string>::const_iterator it =
      node.values.begin();
      it != node.values.end();
      ++it) {
    list->Append(NewDescriptionValuePair(it->first, it->second));
  }

  for (std::map<std::string, gpu::DxDiagNode>::const_iterator it =
      node.children.begin();
      it != node.children.end();
      ++it) {
    std::unique_ptr<base::ListValue> sublist = DxDiagNodeToList(it->second);
    list->Append(NewDescriptionValuePair(it->first, std::move(sublist)));
  }
  return list;
}
#endif

std::string GPUDeviceToString(const gpu::GPUInfo::GPUDevice& gpu) {
  std::string vendor = base::StringPrintf("0x%04x", gpu.vendor_id);
  if (!gpu.vendor_string.empty())
    vendor += " [" + gpu.vendor_string + "]";
  std::string device = base::StringPrintf("0x%04x", gpu.device_id);
  if (!gpu.device_string.empty())
    device += " [" + gpu.device_string + "]";
  return base::StringPrintf("VENDOR = %s, DEVICE= %s%s",
      vendor.c_str(), device.c_str(), gpu.active ? " *ACTIVE*" : "");
}

std::unique_ptr<base::DictionaryValue> GpuInfoAsDictionaryValue() {
  gpu::GPUInfo gpu_info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  auto basic_info = base::MakeUnique<base::ListValue>();
  basic_info->Append(NewDescriptionValuePair(
      "Initialization time",
      base::Int64ToString(gpu_info.initialization_time.InMilliseconds())));
  basic_info->Append(NewDescriptionValuePair(
      "In-process GPU",
      base::MakeUnique<base::Value>(gpu_info.in_process_gpu)));
  basic_info->Append(NewDescriptionValuePair(
      "Passthrough Command Decoder",
      base::MakeUnique<base::Value>(gpu_info.passthrough_cmd_decoder)));
  basic_info->Append(NewDescriptionValuePair(
      "Supports overlays",
      base::MakeUnique<base::Value>(gpu_info.supports_overlays)));
  basic_info->Append(NewDescriptionValuePair(
      "Sandboxed", base::MakeUnique<base::Value>(gpu_info.sandboxed)));
  basic_info->Append(NewDescriptionValuePair(
      "GPU0", GPUDeviceToString(gpu_info.gpu)));
  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    basic_info->Append(NewDescriptionValuePair(
        base::StringPrintf("GPU%d", static_cast<int>(i + 1)),
        GPUDeviceToString(gpu_info.secondary_gpus[i])));
  }
  basic_info->Append(NewDescriptionValuePair(
      "Optimus", base::MakeUnique<base::Value>(gpu_info.optimus)));
  basic_info->Append(NewDescriptionValuePair(
      "Optimus", base::MakeUnique<base::Value>(gpu_info.optimus)));
  basic_info->Append(NewDescriptionValuePair(
      "AMD switchable",
      base::MakeUnique<base::Value>(gpu_info.amd_switchable)));
#if defined(OS_WIN)
  std::string compositor =
      ui::win::IsAeroGlassEnabled() ? "Aero Glass" : "none";
  basic_info->Append(
      NewDescriptionValuePair("Desktop compositing", compositor));

  std::vector<gfx::PhysicalDisplaySize> display_sizes =
      gfx::GetPhysicalSizeForDisplays();
  for (const auto& display_size : display_sizes) {
    const int w = display_size.width_mm;
    const int h = display_size.height_mm;
    const double size_mm = sqrt(w * w + h * h);
    const double size_inches = 0.0393701 * size_mm;
    const double rounded_size_inches = floor(10.0 * size_inches) / 10.0;
    std::string size_string = base::StringPrintf("%.1f\"", rounded_size_inches);
    std::string description_string = base::StringPrintf(
        "Diagonal Monitor Size of %s", display_size.display_name.c_str());
    basic_info->Append(
        NewDescriptionValuePair(description_string, size_string));
  }
#endif

  std::string disabled_extensions;
  GpuDataManagerImpl::GetInstance()->GetDisabledExtensions(
      &disabled_extensions);

  basic_info->Append(
      NewDescriptionValuePair("Driver vendor", gpu_info.driver_vendor));
  basic_info->Append(NewDescriptionValuePair("Driver version",
                                             gpu_info.driver_version));
  basic_info->Append(NewDescriptionValuePair("Driver date",
                                             gpu_info.driver_date));
  basic_info->Append(NewDescriptionValuePair("Pixel shader version",
                                             gpu_info.pixel_shader_version));
  basic_info->Append(NewDescriptionValuePair("Vertex shader version",
                                             gpu_info.vertex_shader_version));
  basic_info->Append(NewDescriptionValuePair("Max. MSAA samples",
                                             gpu_info.max_msaa_samples));
  basic_info->Append(NewDescriptionValuePair("Machine model name",
                                             gpu_info.machine_model_name));
  basic_info->Append(NewDescriptionValuePair("Machine model version",
                                             gpu_info.machine_model_version));
  basic_info->Append(NewDescriptionValuePair("GL_VENDOR",
                                             gpu_info.gl_vendor));
  basic_info->Append(NewDescriptionValuePair("GL_RENDERER",
                                             gpu_info.gl_renderer));
  basic_info->Append(NewDescriptionValuePair("GL_VERSION",
                                             gpu_info.gl_version));
  basic_info->Append(NewDescriptionValuePair("GL_EXTENSIONS",
                                             gpu_info.gl_extensions));
  basic_info->Append(NewDescriptionValuePair("Disabled Extensions",
                                             disabled_extensions));
  basic_info->Append(NewDescriptionValuePair("Window system binding vendor",
                                             gpu_info.gl_ws_vendor));
  basic_info->Append(NewDescriptionValuePair("Window system binding version",
                                             gpu_info.gl_ws_version));
  basic_info->Append(NewDescriptionValuePair("Window system binding extensions",
                                             gpu_info.gl_ws_extensions));
#if defined(OS_LINUX) && defined(USE_X11)
  basic_info->Append(NewDescriptionValuePair("Window manager",
                                             ui::GuessWindowManagerName()));
  {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    std::string value;
    const char kXDGCurrentDesktop[] = "XDG_CURRENT_DESKTOP";
    if (env->GetVar(kXDGCurrentDesktop, &value))
      basic_info->Append(NewDescriptionValuePair(kXDGCurrentDesktop, value));
    const char kGDMSession[] = "GDMSESSION";
    if (env->GetVar(kGDMSession, &value))
      basic_info->Append(NewDescriptionValuePair(kGDMSession, value));
    basic_info->Append(NewDescriptionValuePair(
        "Compositing manager",
        ui::IsCompositingManagerPresent() ? "Yes" : "No"));
  }
#endif
  std::string direct_rendering = gpu_info.direct_rendering ? "Yes" : "No";
  basic_info->Append(
      NewDescriptionValuePair("Direct rendering", direct_rendering));

  std::string reset_strategy =
      base::StringPrintf("0x%04x", gpu_info.gl_reset_notification_strategy);
  basic_info->Append(NewDescriptionValuePair(
      "Reset notification strategy", reset_strategy));

  basic_info->Append(NewDescriptionValuePair(
      "GPU process crash count",
      base::MakeUnique<base::Value>(gpu_info.process_crash_count)));

  auto info = base::MakeUnique<base::DictionaryValue>();

#if defined(OS_WIN)
  auto dx_info = base::MakeUnique<base::Value>();
  if (gpu_info.dx_diagnostics.children.size())
    dx_info = DxDiagNodeToList(gpu_info.dx_diagnostics);
  info->Set("diagnostics", std::move(dx_info));
#endif

#if defined(USE_X11)
  basic_info->Append(NewDescriptionValuePair(
      "System visual ID", base::Uint64ToString(gpu_info.system_visual)));
  basic_info->Append(NewDescriptionValuePair(
      "RGBA visual ID", base::Uint64ToString(gpu_info.rgba_visual)));
#endif

  info->Set("basic_info", std::move(basic_info));
  return info;
}

const char* BufferFormatToString(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::ATC:
      return "ATC";
    case gfx::BufferFormat::ATCIA:
      return "ATCIA";
    case gfx::BufferFormat::DXT1:
      return "DXT1";
    case gfx::BufferFormat::DXT5:
      return "DXT5";
    case gfx::BufferFormat::ETC1:
      return "ETC1";
    case gfx::BufferFormat::R_8:
      return "R_8";
    case gfx::BufferFormat::R_16:
      return "R_16";
    case gfx::BufferFormat::RG_88:
      return "RG_88";
    case gfx::BufferFormat::BGR_565:
      return "BGR_565";
    case gfx::BufferFormat::RGBA_4444:
      return "RGBA_4444";
    case gfx::BufferFormat::RGBX_8888:
      return "RGBX_8888";
    case gfx::BufferFormat::RGBA_8888:
      return "RGBA_8888";
    case gfx::BufferFormat::BGRX_8888:
      return "BGRX_8888";
    case gfx::BufferFormat::BGRA_8888:
      return "BGRA_8888";
    case gfx::BufferFormat::RGBA_F16:
      return "RGBA_F16";
    case gfx::BufferFormat::YVU_420:
      return "YVU_420";
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return "YUV_420_BIPLANAR";
    case gfx::BufferFormat::UYVY_422:
      return "UYVY_422";
  }
  NOTREACHED();
  return nullptr;
}

const char* BufferUsageToString(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      return "GPU_READ";
    case gfx::BufferUsage::SCANOUT:
      return "SCANOUT";
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      return "SCANOUT_CAMERA_READ_WRITE";
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      return "SCANOUT_CPU_READ_WRITE";
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      return "SCANOUT_VDA_WRITE";
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      return "GPU_READ_CPU_READ_WRITE";
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      return "GPU_READ_CPU_READ_WRITE_PERSISTENT";
  }
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<base::ListValue> CompositorInfo() {
  auto compositor_info = base::MakeUnique<base::ListValue>();

  compositor_info->Append(NewDescriptionValuePair(
      "Tile Update Mode",
      IsZeroCopyUploadEnabled() ? "Zero-copy" : "One-copy"));

  compositor_info->Append(NewDescriptionValuePair(
      "Partial Raster", IsPartialRasterEnabled() ? "Enabled" : "Disabled"));
  return compositor_info;
}

std::unique_ptr<base::ListValue> GpuMemoryBufferInfo() {
  auto gpu_memory_buffer_info = base::MakeUnique<base::ListValue>();

  const auto native_configurations =
      gpu::GetNativeGpuMemoryBufferConfigurations();
  for (size_t format = 0;
       format < static_cast<size_t>(gfx::BufferFormat::LAST) + 1; format++) {
    std::string native_usage_support;
    for (size_t usage = 0;
         usage < static_cast<size_t>(gfx::BufferUsage::LAST) + 1; usage++) {
      if (base::ContainsKey(
              native_configurations,
              std::make_pair(static_cast<gfx::BufferFormat>(format),
                             static_cast<gfx::BufferUsage>(usage)))) {
        native_usage_support = base::StringPrintf(
            "%s%s %s", native_usage_support.c_str(),
            native_usage_support.empty() ? "" : ",",
            BufferUsageToString(static_cast<gfx::BufferUsage>(usage)));
      }
    }
    if (native_usage_support.empty())
      native_usage_support = base::StringPrintf("Software only");

    gpu_memory_buffer_info->Append(NewDescriptionValuePair(
        BufferFormatToString(static_cast<gfx::BufferFormat>(format)),
        native_usage_support));
  }
  return gpu_memory_buffer_info;
}

std::unique_ptr<base::ListValue> getDisplayInfo() {
  auto display_info = base::MakeUnique<base::ListValue>();
  const std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  for (const auto& display : displays) {
    display_info->Append(NewDescriptionValuePair("Info ", display.ToString()));
    display_info->Append(NewDescriptionValuePair(
        "Color space information", display.color_space().ToString()));
    display_info->Append(NewDescriptionValuePair(
        "Bits per color component",
        base::Uint64ToString(display.depth_per_component())));
    display_info->Append(NewDescriptionValuePair(
        "Bits per pixel", base::Uint64ToString(display.color_depth())));
  }
  return display_info;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class GpuMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<GpuMessageHandler>,
      public GpuDataManagerObserver,
      public ui::GpuSwitchingObserver {
 public:
  GpuMessageHandler();
  ~GpuMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // GpuDataManagerObserver implementation.
  void OnGpuInfoUpdate() override;

  // ui::GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

  // Messages
  void OnBrowserBridgeInitialized(const base::ListValue* list);
  void OnCallAsync(const base::ListValue* list);

  // Submessages dispatched from OnCallAsync
  std::unique_ptr<base::DictionaryValue> OnRequestClientInfo(
      const base::ListValue* list);
  std::unique_ptr<base::ListValue> OnRequestLogMessages(
      const base::ListValue* list);

 private:
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
    : observing_(false) {
}

GpuMessageHandler::~GpuMessageHandler() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  GpuDataManagerImpl::GetInstance()->RemoveObserver(this);
}

/* BrowserBridge.callAsync prepends a requestID to these messages. */
void GpuMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback("browserBridgeInitialized",
      base::Bind(&GpuMessageHandler::OnBrowserBridgeInitialized,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("callAsync",
      base::Bind(&GpuMessageHandler::OnCallAsync,
                 base::Unretained(this)));
}

void GpuMessageHandler::OnCallAsync(const base::ListValue* args) {
  DCHECK_GE(args->GetSize(), static_cast<size_t>(2));
  // unpack args into requestId, submessage and submessageArgs
  bool ok;
  const base::Value* requestId;
  ok = args->Get(0, &requestId);
  DCHECK(ok);

  std::string submessage;
  ok = args->GetString(1, &submessage);
  DCHECK(ok);

  auto submessageArgs = base::MakeUnique<base::ListValue>();
  for (size_t i = 2; i < args->GetSize(); ++i) {
    const base::Value* arg;
    ok = args->Get(i, &arg);
    DCHECK(ok);

    submessageArgs->Append(arg->CreateDeepCopy());
  }

  // call the submessage handler
  std::unique_ptr<base::Value> ret;
  if (submessage == "requestClientInfo") {
    ret = OnRequestClientInfo(submessageArgs.get());
  } else if (submessage == "requestLogMessages") {
    ret = OnRequestLogMessages(submessageArgs.get());
  } else {  // unrecognized submessage
    NOTREACHED();
    return;
  }

  // call BrowserBridge.onCallAsyncReply with result
  if (ret) {
    web_ui()->CallJavascriptFunctionUnsafe("browserBridge.onCallAsyncReply",
                                           *requestId, *ret);
  } else {
    web_ui()->CallJavascriptFunctionUnsafe("browserBridge.onCallAsyncReply",
                                           *requestId);
  }
}

void GpuMessageHandler::OnBrowserBridgeInitialized(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Watch for changes in GPUInfo
  if (!observing_) {
    GpuDataManagerImpl::GetInstance()->AddObserver(this);
    ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
  }
  observing_ = true;

  // Tell GpuDataManager it should have full GpuInfo. If the
  // Gpu process has not run yet, this will trigger its launch.
  GpuDataManagerImpl::GetInstance()->RequestCompleteGpuInfoIfNeeded();

  // Run callback immediately in case the info is ready and no update in the
  // future.
  OnGpuInfoUpdate();
}

std::unique_ptr<base::DictionaryValue> GpuMessageHandler::OnRequestClientInfo(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto dict = base::MakeUnique<base::DictionaryValue>();

  dict->SetString("version", GetContentClient()->GetProduct());
  dict->SetString("command_line",
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());
  dict->SetString("operating_system",
                  base::SysInfo::OperatingSystemName() + " " +
                  base::SysInfo::OperatingSystemVersion());
  dict->SetString("angle_commit_id", ANGLE_COMMIT_HASH);
  dict->SetString("graphics_backend",
                  std::string("Skia/" STRINGIZE(SK_MILESTONE)
                              " " SKIA_COMMIT_HASH));
  dict->SetString("revision_identifier", GPU_LISTS_VERSION);

  return dict;
}

std::unique_ptr<base::ListValue> GpuMessageHandler::OnRequestLogMessages(
    const base::ListValue*) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return GpuDataManagerImpl::GetInstance()->GetLogMessages();
}

void GpuMessageHandler::OnGpuInfoUpdate() {
  // Get GPU Info.
  std::unique_ptr<base::DictionaryValue> gpu_info_val(
      GpuInfoAsDictionaryValue());

  // Add in blacklisting features
  auto feature_status = base::MakeUnique<base::DictionaryValue>();
  feature_status->Set("featureStatus", GetFeatureStatus());
  feature_status->Set("problems", GetProblems());
  auto workarounds = base::MakeUnique<base::ListValue>();
  for (const std::string& workaround : GetDriverBugWorkarounds())
    workarounds->AppendString(workaround);
  feature_status->Set("workarounds", std::move(workarounds));
  gpu_info_val->Set("featureStatus", std::move(feature_status));
  gpu_info_val->Set("compositorInfo", CompositorInfo());
  gpu_info_val->Set("gpuMemoryBufferInfo", GpuMemoryBufferInfo());
  gpu_info_val->Set("displayInfo", getDisplayInfo());

  // Send GPU Info to javascript.
  web_ui()->CallJavascriptFunctionUnsafe("browserBridge.onGpuInfoUpdate",
                                         *(gpu_info_val.get()));
}

void GpuMessageHandler::OnGpuSwitched() {
  GpuDataManagerImpl::GetInstance()->RequestCompleteGpuInfoIfNeeded();
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// GpuInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

GpuInternalsUI::GpuInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<GpuMessageHandler>());

  // Set up the chrome://gpu/ source.
  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, CreateGpuHTMLSource());
}

}  // namespace content
