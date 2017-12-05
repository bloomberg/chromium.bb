// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/exo/wayland/clients/client_helper.h"

// Client that retreives output related properties (modes, scales, etc.) from
// a compositor and prints them to standard output.

namespace {

// This struct contains globals and all the fields that will be set by
// interface listener callbacks.
struct Info {
  struct {
    int32_t x, y;
    int32_t physical_width, physical_height;
    int32_t subpixel;
    std::string make;
    std::string model;
    int32_t transform;
  } geometry;
  struct Mode {
    uint32_t flags;
    int32_t width, height;
    int32_t refresh;
  };
  // |next_modes| are swapped with |modes| after receiving output done event.
  std::vector<Mode> modes, next_modes;
  int32_t device_scale_factor;
  struct Scale {
    uint32_t flags;
    int32_t scale;
  };
  // |next_scales| are swapped with |scales| after receiving output done event.
  std::vector<Scale> scales, next_scales;
  std::unique_ptr<wl_output> output;
  std::unique_ptr<zaura_shell> aura_shell;
  wl_output_listener output_listener;
};

void RegistryHandler(void* data,
                     wl_registry* registry,
                     uint32_t id,
                     const char* interface,
                     uint32_t version) {
  Info* info = static_cast<Info*>(data);

  if (strcmp(interface, "wl_output") == 0) {
    info->output.reset(static_cast<wl_output*>(
        wl_registry_bind(registry, id, &wl_output_interface, 2)));
    wl_output_add_listener(info->output.get(), &info->output_listener, info);
  } else if (strcmp(interface, "zaura_shell") == 0) {
    if (version >= 2) {
      info->aura_shell.reset(static_cast<zaura_shell*>(
          wl_registry_bind(registry, id, &zaura_shell_interface, 2)));
    }
  }
}

void RegistryRemover(void* data, wl_registry* registry, uint32_t id) {
  LOG(WARNING) << "Got a registry losing event for " << id;
}

void OutputGeometry(void* data,
                    wl_output* output,
                    int x,
                    int y,
                    int physical_width,
                    int physical_height,
                    int subpixel,
                    const char* make,
                    const char* model,
                    int transform) {
  Info* info = static_cast<Info*>(data);

  info->geometry.x = x;
  info->geometry.y = y;
  info->geometry.physical_width = physical_width;
  info->geometry.physical_height = physical_height;
  info->geometry.subpixel = subpixel;
  info->geometry.make = make;
  info->geometry.model = model;
  info->geometry.transform = transform;
}

void OutputMode(void* data,
                wl_output* output,
                uint32_t flags,
                int width,
                int height,
                int refresh) {
  Info* info = static_cast<Info*>(data);

  info->next_modes.push_back({flags, width, height, refresh});
}

void OutputDone(void* data, wl_output* output) {
  Info* info = static_cast<Info*>(data);

  std::swap(info->modes, info->next_modes);
  info->next_modes.clear();
  std::swap(info->scales, info->next_scales);
  info->next_scales.clear();
}

void OutputScale(void* data, wl_output* output, int32_t scale) {
  Info* info = static_cast<Info*>(data);

  info->device_scale_factor = scale;
}

void AuraOutputScale(void* data,
                     zaura_output* output,
                     uint32_t flags,
                     uint32_t scale) {
  Info* info = static_cast<Info*>(data);

  info->next_scales.push_back({flags, scale});
}

std::string OutputSubpixelToString(int32_t subpixel) {
  switch (subpixel) {
    case WL_OUTPUT_SUBPIXEL_UNKNOWN:
      return "unknown";
    case WL_OUTPUT_SUBPIXEL_NONE:
      return "none";
    case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
      return "horizontal rgb";
    case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
      return "horizontal bgr";
    case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
      return "vertical rgb";
    case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
      return "vertical bgr";
    default:
      return base::StringPrintf("unknown (%d)", subpixel);
  }
}

std::string OutputTransformToString(int32_t transform) {
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
      return "normal";
    case WL_OUTPUT_TRANSFORM_90:
      return "90°";
    case WL_OUTPUT_TRANSFORM_180:
      return "180°";
    case WL_OUTPUT_TRANSFORM_270:
      return "270°";
    case WL_OUTPUT_TRANSFORM_FLIPPED:
      return "flipped";
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
      return "flipped 90°";
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
      return "flipped 180°";
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      return "flipped 270°";
    default:
      return base::StringPrintf("unknown (%d)", transform);
  }
}

std::string OutputModeFlagsToString(uint32_t flags) {
  std::string string;
  if (flags & WL_OUTPUT_MODE_CURRENT)
    string += "current       ";
  if (flags & WL_OUTPUT_MODE_PREFERRED)
    string += "preferred";
  base::TrimWhitespaceASCII(string, base::TRIM_TRAILING, &string);
  return string;
}

std::string AuraOutputScaleFlagsToString(uint32_t flags) {
  std::string string;
  if (flags & ZAURA_OUTPUT_SCALE_PROPERTY_CURRENT)
    string += "current       ";
  if (flags & ZAURA_OUTPUT_SCALE_PROPERTY_PREFERRED)
    string += "preferred";
  base::TrimWhitespaceASCII(string, base::TRIM_TRAILING, &string);
  return string;
}

std::string AuraOutputScaleFactorToString(int32_t scale) {
  switch (scale) {
    case ZAURA_OUTPUT_SCALE_FACTOR_0500:
    case ZAURA_OUTPUT_SCALE_FACTOR_0600:
    case ZAURA_OUTPUT_SCALE_FACTOR_0625:
    case ZAURA_OUTPUT_SCALE_FACTOR_0750:
    case ZAURA_OUTPUT_SCALE_FACTOR_0800:
    case ZAURA_OUTPUT_SCALE_FACTOR_1000:
    case ZAURA_OUTPUT_SCALE_FACTOR_1125:
    case ZAURA_OUTPUT_SCALE_FACTOR_1200:
    case ZAURA_OUTPUT_SCALE_FACTOR_1250:
    case ZAURA_OUTPUT_SCALE_FACTOR_1500:
    case ZAURA_OUTPUT_SCALE_FACTOR_1600:
    case ZAURA_OUTPUT_SCALE_FACTOR_2000:
      return base::StringPrintf("%.3f", scale / 1000.0);
    default:
      return base::StringPrintf("unknown (%g)", scale / 1000.0);
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  std::unique_ptr<wl_display> display(wl_display_connect(nullptr));
  if (!display) {
    LOG(ERROR) << "Failed to connect to display";
    return 1;
  }

  Info info = {
      .geometry = {.subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN,
                   .make = "unknown",
                   .model = "unknown",
                   .transform = WL_OUTPUT_TRANSFORM_NORMAL},
      .output_listener = {OutputGeometry, OutputMode, OutputDone, OutputScale}};

  wl_registry_listener registry_listener = {RegistryHandler, RegistryRemover};
  wl_registry* registry = wl_display_get_registry(display.get());
  wl_registry_add_listener(registry, &registry_listener, &info);

  wl_display_roundtrip(display.get());

  std::unique_ptr<zaura_output> aura_output;
  zaura_output_listener aura_output_listener = {AuraOutputScale};
  if (info.output && info.aura_shell) {
    aura_output.reset(static_cast<zaura_output*>(
        zaura_shell_get_aura_output(info.aura_shell.get(), info.output.get())));
    zaura_output_add_listener(aura_output.get(), &aura_output_listener, &info);
  }

  wl_display_roundtrip(display.get());

  std::cout << "geometry:" << std::endl
            << "   x:                 " << info.geometry.x << std::endl
            << "   y:                 " << info.geometry.y << std::endl
            << "   physical width:    " << info.geometry.physical_width << " mm"
            << std::endl
            << "   physical height:   " << info.geometry.physical_height
            << " mm" << std::endl
            << "   subpixel:          "
            << OutputSubpixelToString(info.geometry.subpixel) << std::endl
            << "   make:              " << info.geometry.make << std::endl
            << "   model:             " << info.geometry.model << std::endl
            << "   transform:         "
            << OutputTransformToString(info.geometry.transform) << std::endl
            << std::endl;
  std::cout << "modes:" << std::endl;
  for (auto& mode : info.modes) {
    std::cout << "   " << std::left << std::setw(19)
              << base::StringPrintf("%dx%d:", mode.width, mode.height)
              << std::left << std::setw(14)
              << base::StringPrintf("%.2f Hz", mode.refresh / 1000.0)
              << OutputModeFlagsToString(mode.flags) << std::endl;
  }
  std::cout << std::endl;
  std::cout << "device scale factor:  " << info.device_scale_factor
            << std::endl;
  if (!info.scales.empty()) {
    std::cout << std::endl;
    std::cout << "scales:" << std::endl;
    for (auto& scale : info.scales) {
      std::cout << "   " << std::left << std::setw(19)
                << (AuraOutputScaleFactorToString(scale.scale) + ":")
                << AuraOutputScaleFlagsToString(scale.flags) << std::endl;
    }
  }

  return 0;
}
