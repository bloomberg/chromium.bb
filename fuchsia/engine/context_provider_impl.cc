// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/context_provider_impl.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/io.h>
#include <lib/zx/job.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zircon/processargs.h>

#include <string>
#include <utility>
#include <vector>

#include "base/base_paths_fuchsia.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "content/public/common/content_switches.h"
#include "fuchsia/base/config_reader.h"
#include "fuchsia/base/string_util.h"
#include "fuchsia/engine/common/web_engine_content_client.h"
#include "fuchsia/engine/switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/key_system_names.h"
#include "media/base/media_switches.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/sandbox/fuchsia/sandbox_policy_fuchsia.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

// Returns the underlying channel if |directory| is a client endpoint for a
// |fuchsia::io::Directory| protocol. Otherwise, returns an empty channel.
zx::channel ValidateDirectoryAndTakeChannel(
    fidl::InterfaceHandle<fuchsia::io::Directory> directory_handle) {
  fidl::SynchronousInterfacePtr<fuchsia::io::Directory> directory =
      directory_handle.BindSync();
  zx_status_t status = ZX_ERR_INTERNAL;
  std::vector<uint8_t> entries;

  directory->ReadDirents(0, &status, &entries);
  if (status == ZX_OK) {
    return directory.Unbind().TakeChannel();
  }

  // Not a directory.
  return zx::channel();
}

// Populates a CommandLine with content directory name/handle pairs.
bool SetContentDirectoriesInCommandLine(
    std::vector<fuchsia::web::ContentDirectoryProvider> directories,
    base::CommandLine* command_line,
    base::LaunchOptions* launch_options) {
  DCHECK(command_line);
  DCHECK(launch_options);

  std::vector<std::string> directory_pairs;
  for (size_t i = 0; i < directories.size(); ++i) {
    fuchsia::web::ContentDirectoryProvider& directory = directories[i];

    if (directory.name().find('=') != std::string::npos ||
        directory.name().find(',') != std::string::npos) {
      DLOG(ERROR) << "Invalid character in directory name: "
                  << directory.name();
      return false;
    }

    if (!directory.directory().is_valid()) {
      DLOG(ERROR) << "Service directory handle not valid for directory: "
                  << directory.name();
      return false;
    }

    uint32_t directory_handle_id = base::LaunchOptions::AddHandleToTransfer(
        &launch_options->handles_to_transfer,
        directory.mutable_directory()->TakeChannel().release());
    directory_pairs.emplace_back(
        base::StrCat({directory.name().c_str(), "=",
                      base::NumberToString(directory_handle_id)}));
  }

  command_line->AppendSwitchASCII(switches::kContentDirectories,
                                  base::JoinString(directory_pairs, ","));

  return true;
}

base::Value LoadConfig() {
  const base::Optional<base::Value>& config = cr_fuchsia::LoadPackageConfig();
  if (!config) {
    DLOG(WARNING) << "Configuration data not found. Using default "
                     "WebEngine configuration.";
    return base::Value(base::Value::Type::DICTIONARY);
  }

  return config->Clone();
}

void AppendFeature(base::StringPiece features_flag,
                   base::StringPiece feature_string,
                   base::CommandLine* command_line) {
  if (!command_line->HasSwitch(features_flag)) {
    command_line->AppendSwitchNative(features_flag.as_string(),
                                     feature_string.as_string());
    return;
  }

  std::string new_feature_string =
      command_line->GetSwitchValueASCII(features_flag);
  new_feature_string.append(",").append(feature_string.as_string());
  command_line->RemoveSwitch(features_flag);
  command_line->AppendSwitchNative(features_flag.as_string(),
                                   new_feature_string);
}

// Returns false if the config is present but has invalid contents.
bool MaybeAddCommandLineArgsFromConfig(const base::Value& config,
                                       base::CommandLine* command_line) {
  const base::Value* args = config.FindDictKey("command-line-args");
  if (!args)
    return true;

  static const base::StringPiece kAllowedArgs[] = {
      cc::switches::kEnableGpuBenchmarking,
      switches::kAcceleratedCanvas2dMSAASampleCount,
      switches::kDisableFeatures,
      switches::kDisableGpuWatchdog,
      switches::kEnableFeatures,
      switches::kEnableLowEndDeviceMode,
      switches::kForceGpuMemAvailableMb,
      switches::kForceGpuMemDiscardableLimitMb,
      switches::kForceMaxTextureSize,
      switches::kGpuRasterizationMSAASampleCount,
      switches::kMinHeightForGpuRasterTile,
      switches::kRendererProcessLimit,
  };

  for (const auto& arg : args->DictItems()) {
    if (!base::Contains(kAllowedArgs, arg.first)) {
      LOG(ERROR) << "Unknown command-line arg: " << arg.first;
      // TODO(https://crbug.com/1032439): Return false here once we are done
      // experimenting with memory-related command-line options.
      continue;
    }

    DCHECK(!command_line->HasSwitch(arg.first));
    if (arg.second.is_none()) {
      command_line->AppendSwitch(arg.first);
    } else if (arg.second.is_string()) {
      command_line->AppendSwitchNative(arg.first, arg.second.GetString());
    } else {
      LOG(ERROR) << "Config command-line arg must be a string: " << arg.first;
      return false;
    }

    // TODO(https://crbug.com/1023012): enable-low-end-device-mode currently
    // fakes 512MB total physical memory, which triggers RGB4444 textures,
    // which
    // we don't yet support.
    if (arg.first == switches::kEnableLowEndDeviceMode)
      command_line->AppendSwitch(switches::kDisableRGBA4444Textures);
  }

  return true;
}

// Returns true if DRM is supported in current configuration. Currently we
// assume that it is supported on ARM64, but not on x64.
//
// TODO(crbug.com/1013412): Detect support for all features required for
// FuchsiaCdm. Specifically we need to verify that protected memory is supported
// and that mediacodec API provides hardware video decoders.
bool IsFuchsiaCdmSupported() {
#if defined(ARCH_CPU_ARM64)
  return true;
#else
  return false;
#endif
}

// Use the most significant bit to enable cast streaming receiver features.
// TODO(crbug.com/1078919): Remove this when we have a better way of enabling
// this feature.
constexpr auto kCastStreamingFeatureFlag =
    static_cast<fuchsia::web::ContextFeatureFlags>(1ULL << 63);

}  // namespace

const uint32_t ContextProviderImpl::kContextRequestHandleId =
    PA_HND(PA_USER0, 0);

ContextProviderImpl::ContextProviderImpl() = default;

ContextProviderImpl::~ContextProviderImpl() = default;

void ContextProviderImpl::Create(
    fuchsia::web::CreateContextParams params,
    fidl::InterfaceRequest<fuchsia::web::Context> context_request) {
  if (!context_request.is_valid()) {
    DLOG(ERROR) << "Invalid |context_request|.";
    return;
  }
  if (!params.has_service_directory()) {
    DLOG(ERROR)
        << "Missing argument |service_directory| in CreateContextParams.";
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory =
      std::move(*params.mutable_service_directory());
  if (!service_directory) {
    DLOG(ERROR) << "Invalid |service_directory| in CreateContextParams.";
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  base::LaunchOptions launch_options;
  launch_options.process_name_suffix = ":context";

  service_manager::SandboxPolicyFuchsia sandbox_policy(
      service_manager::SandboxType::kWebContext);
  sandbox_policy.SetServiceDirectory(std::move(service_directory));
  sandbox_policy.UpdateLaunchOptionsForSandbox(&launch_options);

  // SandboxPolicyFuchsia should isolate each Context in its own job.
  DCHECK_NE(launch_options.job_handle, ZX_HANDLE_INVALID);

  // Transfer the ContextRequest handle to a well-known location in the child
  // process' handle table.
  launch_options.handles_to_transfer.push_back(
      {kContextRequestHandleId, context_request.channel().get()});

  // Bind |data_directory| to /data directory, if provided.
  if (params.has_data_directory()) {
    zx::channel data_directory_channel = ValidateDirectoryAndTakeChannel(
        std::move(*params.mutable_data_directory()));
    if (data_directory_channel.get() == ZX_HANDLE_INVALID) {
      DLOG(ERROR)
          << "Invalid argument |data_directory| in CreateContextParams.";
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }

    base::FilePath data_path;
    if (!base::PathService::Get(base::DIR_APP_DATA, &data_path)) {
      DLOG(ERROR) << "Failed to get data directory service path.";
      context_request.Close(ZX_ERR_INTERNAL);
      return;
    }
    launch_options.paths_to_transfer.push_back(
        base::PathToTransfer{data_path, data_directory_channel.release()});
  }

  base::CommandLine launch_command = *base::CommandLine::ForCurrentProcess();
  std::vector<zx::channel> devtools_listener_channels;

  base::Value web_engine_config =
      config_for_test_.is_none() ? LoadConfig() : std::move(config_for_test_);

  if (!MaybeAddCommandLineArgsFromConfig(web_engine_config, &launch_command)) {
    context_request.Close(ZX_ERR_INTERNAL);
    return;
  }

  if (params.has_remote_debugging_port()) {
    launch_command.AppendSwitchNative(
        switches::kRemoteDebuggingPort,
        base::NumberToString(params.remote_debugging_port()));
  }

  if (devtools_listeners_.size() != 0) {
    // Connect DevTools listeners to the new Context process.
    std::vector<std::string> handles_ids;
    for (auto& devtools_listener : devtools_listeners_.ptrs()) {
      fidl::InterfaceHandle<fuchsia::web::DevToolsPerContextListener>
          client_listener;
      devtools_listener.get()->get()->OnContextDevToolsAvailable(
          client_listener.NewRequest());
      devtools_listener_channels.emplace_back(client_listener.TakeChannel());
      handles_ids.push_back(
          base::NumberToString(base::LaunchOptions::AddHandleToTransfer(
              &launch_options.handles_to_transfer,
              devtools_listener_channels.back().get())));
    }
    launch_command.AppendSwitchNative(switches::kRemoteDebuggerHandles,
                                      base::JoinString(handles_ids, ","));
  }

  fuchsia::web::ContextFeatureFlags features = {};
  if (params.has_features())
    features = params.features();

  const bool is_headless =
      (features & fuchsia::web::ContextFeatureFlags::HEADLESS) ==
      fuchsia::web::ContextFeatureFlags::HEADLESS;
  if (is_headless) {
    launch_command.AppendSwitchNative(switches::kOzonePlatform,
                                      switches::kHeadless);
    launch_command.AppendSwitch(switches::kHeadless);
  }

  if ((features & fuchsia::web::ContextFeatureFlags::LEGACYMETRICS) ==
      fuchsia::web::ContextFeatureFlags::LEGACYMETRICS) {
    launch_command.AppendSwitch(switches::kUseLegacyMetricsService);
  }

  const bool enable_vulkan =
      (features & fuchsia::web::ContextFeatureFlags::VULKAN) ==
      fuchsia::web::ContextFeatureFlags::VULKAN;
  bool enable_widevine =
      (features & fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM) ==
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;
  bool enable_playready = params.has_playready_key_system();

  // VULKAN is required for DRM-protected video playback. Allow DRM to also be
  // enabled for HEADLESS Contexts, since Vulkan is never required for audio.
  if ((enable_widevine || enable_playready) && !enable_vulkan && !is_headless) {
    DLOG(ERROR) << "WIDEVINE_CDM and PLAYREADY_CDM features require VULKAN or "
                   "HEADLESS.";
    context_request.Close(ZX_ERR_NOT_SUPPORTED);
    return;
  }

  // If the system doesn't actually support DRM then disable it. This may result
  // in the Context being able to run without using protected buffers.
  if (enable_playready && !IsFuchsiaCdmSupported()) {
    LOG(WARNING) << "PlayReady is not supported on this device.";
    enable_playready = false;
  }
  if (enable_widevine && !IsFuchsiaCdmSupported()) {
    LOG(WARNING) << "Widevine is not supported on this device.";
    enable_widevine = false;
  }

  bool allow_protected_graphics =
      web_engine_config.FindBoolPath("allow-protected-graphics")
          .value_or(false);
  bool force_protected_graphics =
      web_engine_config.FindBoolPath("force-protected-graphics")
          .value_or(false);
  bool enable_protected_graphics =
      ((enable_playready || enable_widevine) && allow_protected_graphics) ||
      force_protected_graphics;

  if (enable_protected_graphics) {
    launch_command.AppendSwitch(switches::kEnforceVulkanProtectedMemory);
    launch_command.AppendSwitch(switches::kEnableProtectedVideoBuffers);
    bool force_protected_video_buffers =
        web_engine_config.FindBoolPath("force-protected-video-buffers")
            .value_or(false);
    if (force_protected_video_buffers) {
      launch_command.AppendSwitch(switches::kForceProtectedVideoOutputBuffers);
    }
  }

  if (enable_vulkan) {
    if (is_headless) {
      DLOG(ERROR) << "VULKAN and HEADLESS features cannot be used together.";
      context_request.Close(ZX_ERR_NOT_SUPPORTED);
      return;
    }

    VLOG(1) << "Enabling Vulkan GPU acceleration.";
    // Vulkan requires use of SkiaRenderer, configured to a use Vulkan context.
    launch_command.AppendSwitch(switches::kUseVulkan);
    const std::vector<base::StringPiece> enabled_features = {
        features::kUseSkiaRenderer.name, features::kVulkan.name};
    AppendFeature(switches::kEnableFeatures,
                  base::JoinString(enabled_features, ","), &launch_command);

    // SkiaRenderer requires out-of-process rasterization be enabled.
    launch_command.AppendSwitch(switches::kEnableOopRasterization);

    if (!enable_protected_graphics) {
      launch_command.AppendSwitchASCII(switches::kUseGL,
                                       gl::kGLImplementationANGLEName);
    } else {
      DLOG(WARNING) << "ANGLE is not compatible with "
                    << switches::kEnforceVulkanProtectedMemory
                    << ", disabling GL";
      // TODO(crbug.com/1059010): Fix this; probably don't protect canvas
      // resources.
      launch_command.AppendSwitchASCII(switches::kUseGL,
                                       gl::kGLImplementationStubName);
    }
  } else {
    VLOG(1) << "Disabling GPU acceleration.";
    // Disable use of Vulkan GPU, and use of the software-GL rasterizer. The
    // Context will still run a GPU process, but will not support WebGL.
    launch_command.AppendSwitch(switches::kDisableGpu);
    launch_command.AppendSwitch(switches::kDisableSoftwareRasterizer);
  }

  if (enable_widevine) {
    launch_command.AppendSwitch(switches::kEnableWidevine);
  }

  if (enable_playready) {
    const std::string& key_system = params.playready_key_system();
    if (key_system == kWidevineKeySystem || media::IsClearKey(key_system)) {
      LOG(ERROR)
          << "Invalid value for CreateContextParams/playready_key_system: "
          << key_system;
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }
    launch_command.AppendSwitchNative(switches::kPlayreadyKeySystem,
                                      key_system);
  }

  bool disable_software_video_decoder =
      (features &
       fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY;
  bool enable_hardware_video_decoder =
      (features & fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER;
  if (disable_software_video_decoder) {
    if (!enable_hardware_video_decoder) {
      LOG(ERROR) << "Software video decoding may only be disabled if hardware "
                    "video decoding is enabled.";
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }

    launch_command.AppendSwitch(switches::kDisableSoftwareVideoDecoders);
  }

  const bool enable_cast_streaming_receiver =
      (features & kCastStreamingFeatureFlag) == kCastStreamingFeatureFlag;
  if (enable_cast_streaming_receiver)
    launch_command.AppendSwitch(switches::kEnableCastStreamingReceiver);

  // Validate embedder-supplied product, and optional version, and pass it to
  // the Context to include in the UserAgent.
  if (params.has_user_agent_product()) {
    if (!net::HttpUtil::IsToken(params.user_agent_product())) {
      LOG(ERROR) << "Invalid embedder product.";
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }
    std::string product_tag(params.user_agent_product());
    if (params.has_user_agent_version()) {
      if (!net::HttpUtil::IsToken(params.user_agent_version())) {
        LOG(ERROR) << "Invalid embedder version.";
        context_request.Close(ZX_ERR_INVALID_ARGS);
        return;
      }
      product_tag += "/" + params.user_agent_version();
    }
    launch_command.AppendSwitchNative(switches::kUserAgentProductAndVersion,
                                      std::move(product_tag));
  } else if (params.has_user_agent_version()) {
    LOG(ERROR) << "Embedder version without product.";
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  if (params.has_content_directories() &&
      !SetContentDirectoriesInCommandLine(
          std::move(*params.mutable_content_directories()), &launch_command,
          &launch_options)) {
    LOG(ERROR) << "Invalid content directories specified.";
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  if (params.has_unsafely_treat_insecure_origins_as_secure()) {
    const std::vector<std::string>& insecure_origins =
        params.unsafely_treat_insecure_origins_as_secure();
    if (std::find(insecure_origins.begin(), insecure_origins.end(),
                  switches::kAllowRunningInsecureContent) !=
        insecure_origins.end()) {
      launch_command.AppendSwitch(switches::kAllowRunningInsecureContent);
    }
    // TODO(crbug.com/1023510): Pass the rest of the list to the Context
    // process.
  }

  if (params.has_cors_exempt_headers()) {
    std::vector<base::StringPiece> cors_exempt_headers;
    for (const auto& header : params.cors_exempt_headers()) {
      cors_exempt_headers.push_back(cr_fuchsia::BytesAsString(header));
    }

    launch_command.AppendSwitchNative(
        switches::kCorsExemptHeaders,
        base::JoinString(cors_exempt_headers, ","));
  }

  if (launch_for_test_)
    launch_for_test_.Run(launch_command, launch_options);
  else
    base::LaunchProcess(launch_command, launch_options);

  // |context_request| and any DevTools channels were transferred (not copied)
  // to the Context process.
  ignore_result(context_request.TakeChannel().release());
  for (auto& channel : devtools_listener_channels)
    ignore_result(channel.release());
}

void ContextProviderImpl::SetLaunchCallbackForTest(
    LaunchCallbackForTest launch) {
  launch_for_test_ = std::move(launch);
}

void ContextProviderImpl::EnableDevTools(
    fidl::InterfaceHandle<fuchsia::web::DevToolsListener> listener,
    EnableDevToolsCallback callback) {
  devtools_listeners_.AddInterfacePtr(listener.Bind());
  callback();
}
