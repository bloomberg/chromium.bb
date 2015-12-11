# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'internet_page',
      'variables': {
        'depends': [
          '../../../../../third_party/closure_compiler/externs/networking_private_interface.js',
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../settings_page/settings_animated_pages.js'
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'internet_detail_page',
      'variables': {
        'depends': [
          '../../../../../third_party/closure_compiler/externs/networking_private_interface.js',
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/cr_elements/policy/cr_policy_network_behavior.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'internet_known_networks_page',
      'variables': {
        'depends': [
          '../../../../../third_party/closure_compiler/externs/networking_private_interface.js',
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'network_apnlist',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'network_ip_config',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'network_nameservers',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'network_property_list',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/cr_elements/policy/cr_policy_network_behavior.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'network_proxy',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/cr_elements/policy/cr_policy_network_behavior.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'network_proxy_input',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'network_proxy_exclusions',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'network_siminfo',
      'variables': {
        'depends': [
          '../../../../../third_party/closure_compiler/externs/networking_private_interface.js',
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'network_summary',
      'variables': {
        'depends': [
          '../../../../../third_party/closure_compiler/externs/networking_private_interface.js',
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'network_summary_item',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/cr_elements/network/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/networking_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
