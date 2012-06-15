// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_startup_flags.h"

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "ui/base/ui_base_switches.h"

namespace {

void SetCommandLineSwitch(const std::string& switch_string) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_string))
    command_line->AppendSwitch(switch_string);
}

void SetPepperCommandLineFlags(std::string plugin_descriptor) {
  CommandLine* parsed_command_line = CommandLine::ForCurrentProcess();

  // Get the plugin info from the Java side. kRegisterPepperPlugins needs to be
  // added to the CommandLine before either PluginService::GetInstance() or
  // BrowserRenderProcessHost::AppendRendererCommandLine() is called to ensure
  // the plugin is available.
  // TODO(klobag) with the current implementation, the plugin can only be added
  // in the process start up time. Need to look into whether we can add the
  // plugin when the process is running.
  if (!plugin_descriptor.empty()) {
    // Usually the plugins are parsed by pepper_plugin_registry, but
    // flash needs two seperate command-line flags in order to work
    // when no mime-type is specified. The plugin string will look like this:
    // flash|others
    // Each plugin is specified like this:
    // path<#name><#description><#version>;mimetype
    std::vector<std::string> other_flash;
    base::SplitString(plugin_descriptor, '|', &other_flash);
    if (other_flash.size() == 2) {
      const std::string& other = other_flash[0];
      const std::string& flash = other_flash[1];
      parsed_command_line->AppendSwitchNative(
          switches::kRegisterPepperPlugins, other);
      std::vector<std::string> parts;
      base::SplitString(flash, ';', &parts);
      if (parts.size() >= 2) {
        std::vector<std::string> info;
        base::SplitString(parts[0], '#', &info);
        if (info.size() >= 4) {
          parsed_command_line->AppendSwitchNative(
              switches::kPpapiFlashPath, info[0]);
          parsed_command_line->AppendSwitchNative(
              switches::kPpapiFlashVersion, info[3]);
        }
      }
    }
  }
}

} // namespace

namespace content {

void SetContentCommandLineFlags(int max_render_process_count,
                                const std::string& plugin_descriptor) {
  // May be called multiple times, to cover all possible program entry points.
  static bool already_initialized = false;
  if (already_initialized) return;
  already_initialized = true;

  CommandLine* parsed_command_line = CommandLine::ForCurrentProcess();

#if !defined(ANDROID_UPSTREAM_BRINGUP)
  // TODO(yfriedman): Upstream this when bringing up rendering code and
  // rendering modes. b/6668088
  // Set subflags for the --graphics-mode=XYZ omnibus flag.
  std::string graphics_mode;
  if (parsed_command_line->HasSwitch(switches::kGraphicsMode)) {
    graphics_mode = parsed_command_line->GetSwitchValueASCII(
        switches::kGraphicsMode);
  } else {
    // Default mode is threaded compositor mode
    graphics_mode = switches::kGraphicsModeValueCompositor;
    parsed_command_line->AppendSwitchNative(
        switches::kGraphicsMode, graphics_mode.c_str());
  }

  if (graphics_mode == switches::kGraphicsModeValueBasic) {
    // Intentionally blank.
  } else if (graphics_mode == switches::kGraphicsModeValueCompositor) {
    SetCommandLineSwitch(switches::kForceCompositingMode);
    SetCommandLineSwitch(switches::kEnableAcceleratedPlugins);
    SetCommandLineSwitch(switches::kEnableCompositingForFixedPosition);
    SetCommandLineSwitch(switches::kEnableThreadedCompositing);
    // Per tile painting saves memory in background tabs (http://b/5669228).
    SetCommandLineSwitch(switches::kEnablePerTilePainting);
  } else {
    LOG(FATAL) << "Invalid --graphics-mode flag: " << graphics_mode;
  }
#endif

  if (parsed_command_line->HasSwitch(switches::kRendererProcessLimit)) {
    std::string limit = parsed_command_line->GetSwitchValueASCII(
        switches::kRendererProcessLimit);
    int value;
    if (base::StringToInt(limit, &value))
      max_render_process_count = value;
  }

  if (max_render_process_count <= 0) {
    // Need to ensure the command line flag is consistent as a lot of chrome
    // internal code checks this directly, but it wouldn't normally get set when
    // we are implementing an embedded WebView.
    SetCommandLineSwitch(switches::kSingleProcess);
  } else {
    max_render_process_count =
        std::min(max_render_process_count,
                 static_cast<int>(content::kMaxRendererProcessCount));
    content::RenderProcessHost::SetMaxRendererProcessCount(
        max_render_process_count);
  }

  // Load plugins out-of-process by default.
  // We always want flash out-of-process.
  parsed_command_line->AppendSwitch(switches::kPpapiOutOfProcess);

  // Run the GPU service as a thread in the browser instead of as a
  // standalone process.
  parsed_command_line->AppendSwitch(switches::kInProcessGPU);

  // Disable WebGL for now (See http://b/5634125)
  parsed_command_line->AppendSwitch(switches::kDisableExperimentalWebGL);

  // Always use fixed layout and viewport tag.
  parsed_command_line->AppendSwitch(switches::kEnableFixedLayout);
  parsed_command_line->AppendSwitch(switches::kEnableViewport);

  // TODO(aelias): switch this to true value of deviceScaleFactor once floats
  //               are supported
  parsed_command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                         "1");

  // TODO(aelias): Enable these flags once they're merged in from upstream.
  //  parsed_command_line->AppendSwitch(switches::kEnableTouchEvents);
  //  parsed_command_line->AppendSwitch(switches::kEnablePinch);

  SetPepperCommandLineFlags(plugin_descriptor);
}

}  // namespace content
