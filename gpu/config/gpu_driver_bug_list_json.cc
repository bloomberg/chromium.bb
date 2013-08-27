// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Determines whether a certain driver bug exists in the current system.
// A valid gpu_driver_bug_list.json file are in the format of
// {
//   "version": "x.y",
//   "entries": [
//     { // entry 1
//     },
//     ...
//     { // entry n
//     }
//   ]
// }
//
// Each entry contains the following fields (fields are optional unless
// specifically described as mandatory below):
// 1. "id" is an integer.  0 is reserved.  This field is mandatory.
// 2. "os" contains "type" and an optional "version". "type" could be "macosx",
//    "linux", "win", "chromeos", or "any".  "any" is the same as not specifying
//    "os".
//    "version" is a VERSION structure (defined below).
// 3. "vendor_id" is a string.  0 is reserved.
// 4. "device_id" is an array of strings.  0 is reserved.
// 5. "multi_gpu_style" is a string, valid values include "optimus", and
//    "amd_switchable".
// 6. "multi_gpu_category" is a string, valid values include "any", "primary",
//    and "secondary".  If unspecified, the default value is "primary".
// 7. "driver_vendor" is a STRING structure (defined below).
// 8. "driver_version" is a VERSION structure (defined below).
// 9. "driver_date" is a VERSION structure (defined below).
//    The version is interpreted as "year.month.day".
// 10. "gl_vendor" is a STRING structure (defined below).
// 11. "gl_renderer" is a STRING structure (defined below).
// 12. "gl_extensions" is a STRING structure (defined below).
// 13. "perf_graphics" is a FLOAT structure (defined below).
// 14. "perf_gaming" is a FLOAT structure (defined below).
// 15. "perf_overall" is a FLOAT structure (defined below).
// 16. "machine_model" contains "name" and an optional "version".  "name" is a
//     STRING structure and "version" is a VERSION structure (defined below).
// 17. "gpu_count" is a INT structure (defined below).
// 18  "cpu_info" is a STRING structure (defined below).
// 19. "exceptions" is a list of entries.
// 20. "features" is a list of driver bug types. For a list of supported types,
//     see src/gpu/command_buffer/service/gpu_driver_bug_workaround_type.h
//     This field is mandatory.
// 21. "description" has the description of the entry.
// 22. "webkit_bugs" is an array of associated webkit bug numbers.
// 23. "cr_bugs" is an array of associated chromium bug numbers.
// 24. "browser_version" is a VERSION structure (defined below).  If this
//     condition is not satisfied, the entry will be ignored.  If it is not
//     present, then the entry applies to all versions of the browser.
// 25. "disabled" is a boolean. If it is present, the entry will be skipped.
//     This can not be used in exceptions.
//
// VERSION includes "op", "style", "number", and "number2".  "op" can be any of
// the following values: "=", "<", "<=", ">", ">=", "any", "between".  "style"
// is optional and can be "lexical" or "numerical"; if it's not specified, it
// defaults to "numerical".  "number2" is only used if "op" is "between".
// "between" is "number <= * <= number2".
// "number" is used for all "op" values except "any". "number" and "number2"
// are in the format of x, x.x, x.x.x, etc.
// Only "driver_version" supports lexical style if the format is major.minor;
// in that case, major is still numerical, but minor is lexical. 
//
// STRING includes "op" and "value".  "op" can be any of the following values:
// "contains", "beginwith", "endwith", "=".  "value" is a string.
//
// FLOAT includes "op" "value", and "value2".  "op" can be any of the
// following values: "=", "<", "<=", ">", ">=", "any", "between".  "value2" is
// only used if "op" is "between".  "value" is used for all "op" values except
// "any". "value" and "value2" are valid float numbers.
// INT is very much like FLOAT, except that the values need to be integers.

#include "gpu/config/gpu_control_list_jsons.h"

#define LONG_STRING_CONST(...) #__VA_ARGS__

namespace gpu {

const char kGpuDriverBugListJson[] = LONG_STRING_CONST(

{
  "name": "gpu driver bug list",
  // Please update the version number whenever you change this file.
  "version": "2.6",
  "entries": [
    {
      "id": 1,
      "description": "Imagination driver doesn't like uploading lots of buffer data constantly",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Imagination"
      },
      "features": [
        "use_client_side_arrays_for_stream_buffers"
      ]
    },
    {
      "id": 2,
      "description": "ARM driver doesn't like uploading lots of buffer data constantly",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "ARM"
      },
      "features": [
        "use_client_side_arrays_for_stream_buffers"
      ]
    },
    {
      "id": 3,
      "features": [
        "set_texture_filter_before_generating_mipmap"
      ]
    },
    {
      "id": 4,
      "description": "Need to set the alpha to 255",
      "features": [
        "clear_alpha_in_readpixels"
      ]
    },
    {
      "id": 5,
      "vendor_id": "0x10de",
      "features": [
        "use_current_program_after_successful_link"
      ]
    },
    {
      "id": 6,
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Qualcomm"
      },
      "features": [
        "restore_scissor_on_fbo_change",
        "delete_instead_of_resize_fbo"  // Only need this on the ICS driver.
      ]
    },
    {
      "id": 7,
      "cr_bugs": [89557],
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x10de",
      "features": [
        "needs_offscreen_buffer_workaround"
      ]
    },
    {
      "id": 8,
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x1002",
      "features": [
        "needs_glsl_built_in_function_emulation"
      ]
    },
    {
      "id": 9,
      "description": "Mac AMD drivers get gl_PointCoord backward on OSX 10.8 or earlier",
      "cr_bugs": [256349],
      "os": {
        "type": "macosx",
        "version": {
          "op": "<",
          "number": "10.9"
        }
      },
      "vendor_id": "0x1002",
      "features": [
        "reverse_point_sprite_coord_origin"
      ]
    },
    {
      "id": 10,
      "description": "Mac Intel drivers get gl_PointCoord backward on OSX 10.8 or earlier",
      "cr_bugs": [256349],
      "os": {
        "type": "macosx",
        "version": {
          "op": "<",
          "number": "10.9"
        }
      },
      "vendor_id": "0x8086",
      "features": [
        "reverse_point_sprite_coord_origin"
      ]
    },
    {
      "id": 11,
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x8086",
      "features": [
        "max_texture_size_limit_4096"
      ]
    },
    {
      "id": 12,
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x8086",
      "features": [
        "max_cube_map_texture_size_limit_1024"
      ]
    },
    {
      "id": 13,
      "os": {
        "type": "macosx",
        "version": {
          "op": "<",
          "number": "10.7.3"
        }
      },
      "vendor_id": "0x8086",
      "features": [
        "max_cube_map_texture_size_limit_512"
      ]
    },
    {
      "id": 14,
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x1002",
      "features": [
        "max_texture_size_limit_4096",
        "max_cube_map_texture_size_limit_4096"
      ]
    },
    {
      "id": 15,
      "description": "Some Android Qualcomm drivers falsely report GL_ANGLE_framebuffer_multisample",
      "cr_bugs": [165736],
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Qualcomm"
      },
      "features": [
        "disable_angle_framebuffer_multisample"
      ]
    },
    {
      "id": 16,
      "description": "Intel drivers on Linux appear to be buggy",
      "os": {
        "type": "linux"
      },
      "vendor_id": "0x8086",
      "features": [
        "disable_ext_occlusion_query"
      ]
    },
    {
      "id": 17,
      "description": "Some drivers are unable to reset the D3D device in the GPU process sandbox",
      "os": {
        "type": "win"
      },
      "features": [
        "exit_on_context_lost"
      ]
    },
    {
      "id": 18,
      "description": "Everything except async + NPOT + multiple-of-8 textures are brutally slow for Imagination drivers",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Imagination"
      },
      "features": [
        "enable_chromium_fast_npot_mo8_textures"
      ]
    },
    {
      "id": 19,
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Qualcomm"
      },
      "features": [
        "disable_depth_texture"
      ]
    },
    {
      "id": 20,
      "description": "Disable EXT_draw_buffers on GeForce GT 650M on Mac OS X due to driver bugs.",
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x10de",
      "device_id": ["0x0fd5"],
      "multi_gpu_category": "any",
      "features": [
        "disable_ext_draw_buffers"
      ]
    },
    {
      "id": 21,
      "description": "Vivante GPUs are buggy with context switching.",
      "cr_bugs": [179250, 235935],
      "os": {
        "type": "android"
      },
      "gl_extensions": {
        "op": "contains",
        "value": "GL_VIV_shader_binary"
      },
      "features": [
        "unbind_fbo_on_context_switch"
      ]
    },
    {
      "id": 22,
      "description": "Imagination drivers are buggy with context switching.",
      "cr_bugs": [230896],
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "Imagination"
      },
      "features": [
        "unbind_fbo_on_context_switch"
      ]
    },
    {
      "id": 23,
      "cr_bugs": [243038],
      "description": "Disable OES_standard_derivative on Intel Pineview M Gallium drivers.",
      "os": {
        "type": "chromeos"
      },
      "vendor_id": "0x8086",
      "device_id": ["0xa011", "0xa012"],
      "features": [
        "disable_oes_standard_derivatives"
      ]
    },
    {
      "id": 24,
      "cr_bugs": [231082],
      "description": "Mali-400 drivers throw an error when a buffer object's size is set to 0.",
      "os": {
        "type": "android"
      },
      "gl_vendor": {
        "op": "beginwith",
        "value": "ARM"
      },
      "gl_renderer": {
        "op": "contains",
        "value": "Mali-400"
      },
      "features": [
        "use_non_zero_size_for_client_side_stream_buffers"
      ]
    },
    {
      "id": 25,
      "cr_bugs": [152225],
      "description":
          "PBO + Readpixels + intel gpu doesn't work on OSX 10.7.",
      "os": {
        "type": "macosx",
        "version": {
          "op": "<",
          "number": "10.8"
        }
      },
      "vendor_id": "0x8086",
      "features": [
        "disable_async_readpixels"
      ]
    },
    {
      "id": 26,
      "description": "Disable use of Direct3D 11 on Windows",
      "os": {
        "type": "win"
      },
      "features": [
        "disable_d3d11"
      ]
    },
    {
      "id": 27,
      "cr_bugs": [265115],
      "description": "Async Readpixels with GL_BGRA format is broken on Haswell chipset on Mac.",
      "os": {
        "type": "macosx"
      },
      "vendor_id": "0x8086",
      "device_id": ["0x0402", "0x0406", "0x040a", "0x0412", "0x0416", "0x041a",
                    "0x0a04", "0x0a16", "0x0a22", "0x0a26", "0x0a2a"],
      "features": [
        "swizzle_rgba_for_async_readpixels"
      ]
    },
    {
      "id": 28,
      "cr_bugs": [277817],
      "description": "Disable use of ANGLE_instanced_arrays on Windows",
      "os": {
        "type": "win"
      },
      "features": [
        "disable_angle_instanced_arrays"
      ]
    }
  ]
}

);  // LONG_STRING_CONST macro

}  // namespace gpu
