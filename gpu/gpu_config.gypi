# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
   ],
  'include_dirs': [
    '..',
  ],
  'sources': [
    'config/dx_diag_node.cc',
    'config/dx_diag_node.h',
    'config/gpu_blacklist.cc',
    'config/gpu_blacklist.h',
    'config/gpu_control_list_jsons.h',
    'config/gpu_control_list.cc',
    'config/gpu_control_list.h',
    'config/gpu_driver_bug_list_json.cc',
    'config/gpu_driver_bug_list.cc',
    'config/gpu_driver_bug_list.h',
    'config/gpu_driver_bug_workaround_type.h',
    'config/gpu_feature_type.h',
    'config/gpu_info.cc',
    'config/gpu_info.h',
    'config/gpu_performance_stats.h',
    'config/gpu_switching_list_json.cc',
    'config/gpu_switching_list.cc',
    'config/gpu_switching_list.h',
    'config/gpu_switching_option.h',
    'config/gpu_util.cc',
    'config/gpu_util.h',
    'config/software_rendering_list_json.cc',
  ],
}
