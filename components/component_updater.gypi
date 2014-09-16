# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/component_updater
      'target_name': 'component_updater',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../courgette/courgette.gyp:courgette_lib',
        '../crypto/crypto.gyp:crypto',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/zlib/google/zip.gyp:zip',
        '../net/net.gyp:net',
        '../ui/base/ui_base.gyp:ui_base',
        'crx_file',
        'omaha_query_params',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'component_updater/background_downloader_win.cc',
        'component_updater/background_downloader_win.h',
        'component_updater/component_patcher.cc',
        'component_updater/component_patcher.h',
        'component_updater/component_patcher_operation.cc',
        'component_updater/component_patcher_operation.h',
        'component_updater/component_updater_configurator.h',
        'component_updater/component_unpacker.cc',
        'component_updater/component_unpacker.h',
        'component_updater/component_updater_paths.cc',
        'component_updater/component_updater_paths.h',
        'component_updater/component_updater_ping_manager.cc',
        'component_updater/component_updater_ping_manager.h',
        'component_updater/component_updater_service.cc',
        'component_updater/component_updater_service.h',
        'component_updater/component_updater_switches.cc',
        'component_updater/component_updater_switches.h',
        'component_updater/component_updater_utils.cc',
        'component_updater/component_updater_utils.h',
        'component_updater/crx_update_item.h',
        'component_updater/crx_downloader.cc',
        'component_updater/crx_downloader.h',
        'component_updater/default_component_installer.cc',
        'component_updater/default_component_installer.h',
        'component_updater/pref_names.cc',
        'component_updater/pref_names.h',
        'component_updater/request_sender.cc',
        'component_updater/request_sender.h',
        'component_updater/update_checker.cc',
        'component_updater/update_checker.h',
        'component_updater/update_response.cc',
        'component_updater/update_response.h',
        'component_updater/url_fetcher_downloader.cc',
        'component_updater/url_fetcher_downloader.h',
      ],
    },
    {
      # GN version: //components/component_updater:test_support
      'target_name': 'component_updater_test_support',
      'type': 'static_library',
      'dependencies': [
        'component_updater',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'component_updater/test/test_configurator.cc',
        'component_updater/test/test_configurator.h',
        'component_updater/test/test_installer.cc',
        'component_updater/test/test_installer.h',
        'component_updater/test/url_request_post_interceptor.cc',
        'component_updater/test/url_request_post_interceptor.h',
      ],
    },
  ],
}
