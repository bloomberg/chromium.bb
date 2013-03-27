# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'chromium_code': 1,
      'enable_wexit_time_destructors': 1,
    },
    'include_dirs': [
      '<(DEPTH)',
      # To allow including "version.h"
      '<(SHARED_INTERMEDIATE_DIR)',
    ],
    'defines' : [
      'SECURITY_WIN32',
      'STRICT',
      '_ATL_APARTMENT_THREADED',
      '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
      '_ATL_NO_COM_SUPPORT',
      '_ATL_NO_AUTOMATIC_NAMESPACE',
      '_ATL_NO_EXCEPTIONS',
    ],
    'conditions': [
      ['OS=="win"', {
        # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
        'msvs_disabled_warnings': [ 4267, ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'cloud_print_service_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/cloud_print',
      },
      'actions': [
        {
          'action_name': 'cloud_print_service_resources',
          'variables': {
            'grit_grd_file': 'win/cloud_print_service_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },
    {
      'target_name': 'cloud_print_service_lib',
      'type': 'static_library',
      'defines': ['COMPILE_CONTENT_STATICALLY'],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/printing/printing.gyp:printing',
        'cloud_print_service_resources',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
            '<(DEPTH)/chrome/chrome.gyp:launcher_support',
            '<(DEPTH)/chrome/common_constants.gyp:common_constants',
          ],
        }],
      ],
      'sources': [
        '<(DEPTH)/content/public/common/content_switches.cc',
        'service_state.cc',
        'service_state.h',
        'service_switches.cc',
        'service_switches.h',
        'win/chrome_launcher.cc',
        'win/chrome_launcher.h',
        'win/local_security_policy.cc',
        'win/local_security_policy.h',
        'win/service_controller.cc',
        'win/service_controller.h',
        'win/service_listener.cc',
        'win/service_listener.h',
        'win/service_utils.cc',
        'win/service_utils.h',
        'win/setup_listener.cc',
        'win/setup_listener.h',
      ],
    },
    {
      'target_name': 'cloud_print_service',
      'type': 'executable',
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_exe_version.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_ar.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_bg.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_bn.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_ca.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_cs.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_da.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_de.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_el.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_en.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_en-GB.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_es.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_es-419.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_et.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_fa.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_fi.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_fil.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_fr.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_gu.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_he.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_hi.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_hr.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_hu.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_id.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_it.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_ja.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_kn.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_ko.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_lt.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_lv.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_ml.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_mr.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_ms.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_nb.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_nl.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_pl.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_pt-BR.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_pt-PT.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_ro.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_ru.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_sk.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_sl.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_sr.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_sv.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_ta.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_te.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_th.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_tr.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_uk.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_vi.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_zh-CN.rc',
        '<(SHARED_INTERMEDIATE_DIR)/cloud_print/cloud_print_service_resources_zh-TW.rc',
        'win/cloud_print_service.cc',
      ],
      'dependencies': [
        'cloud_print_service_lib',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1',         # Set /SUBSYSTEM:CONSOLE
          'UACExecutionLevel': '2', # /level='requireAdministrator'
          'AdditionalDependencies': [
              'secur32.lib',
          ],
        },
      },
    },
  ],
}
