# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //components/test_runner:test_runner
      'target_name': 'test_runner',
      'type': '<(component)',
      'defines': [
        'TEST_RUNNER_IMPLEMENTATION',
      ],
      'dependencies': [
        'resources',
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../cc/cc.gyp:cc',
        '../../cc/blink/cc_blink.gyp:cc_blink',
        '../../gin/gin.gyp:gin',
        '../../gpu/gpu.gyp:gpu',
        '../../net/net.gyp:net',
        '../../skia/skia.gyp:skia',
        '../../third_party/WebKit/public/blink.gyp:blink',
        '../../ui/events/events.gyp:dom_keycode_converter',
        '../../ui/events/events.gyp:events_base',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../ui/gfx/gfx.gyp:gfx_test_support',
        '../../url/url.gyp:url_lib',
        '../../v8/tools/gyp/v8.gyp:v8',
      ],
      'include_dirs': [
        '..',
      ],
      # Note: sources list duplicated in GN build.
      'sources': [
        'accessibility_controller.cc',
        'accessibility_controller.h',
        'app_banner_client.cc',
        'app_banner_client.h',
        'blink_test_platform_support.h',
        'blink_test_platform_support_android.cc',
        'blink_test_platform_support_linux.cc',
        'blink_test_platform_support_mac.mm',
        'blink_test_platform_support_win.cc',
        'event_sender.cc',
        'event_sender.h',
        'gamepad_controller.cc',
        'gamepad_controller.h',
        'gc_controller.cc',
        'gc_controller.h',
        'layout_dump.cc',
        'layout_dump.h',
        'layout_dump_flags.h',
        'mock_color_chooser.cc',
        'mock_color_chooser.h',
        'mock_credential_manager_client.cc',
        'mock_credential_manager_client.h',
        'mock_grammar_check.cc',
        'mock_grammar_check.h',
        'mock_screen_orientation_client.cc',
        'mock_screen_orientation_client.h',
        'mock_spell_check.cc',
        'mock_spell_check.h',
        'mock_web_audio_device.cc',
        'mock_web_audio_device.h',
        'mock_web_media_stream_center.cc',
        'mock_web_media_stream_center.h',
        'mock_web_midi_accessor.cc',
        'mock_web_midi_accessor.h',
        'mock_web_speech_recognizer.cc',
        'mock_web_speech_recognizer.h',
        'mock_web_theme_engine.cc',
        'mock_web_theme_engine.h',
        'mock_web_user_media_client.cc',
        'mock_web_user_media_client.h',
        'mock_webrtc_data_channel_handler.cc',
        'mock_webrtc_data_channel_handler.h',
        'mock_webrtc_dtmf_sender_handler.cc',
        'mock_webrtc_dtmf_sender_handler.h',
        'mock_webrtc_peer_connection_handler.cc',
        'mock_webrtc_peer_connection_handler.h',
        'spell_check_client.cc',
        'spell_check_client.h',
        'test_common.cc',
        'test_common.h',
        'test_info_extractor.cc',
        'test_info_extractor.h',
        'test_interfaces.cc',
        'test_interfaces.h',
        'test_plugin.cc',
        'test_plugin.h',
        'test_runner.cc',
        'test_runner.h',
        'test_runner_export.h',
        'test_preferences.cc',
        'test_preferences.h',
        'text_input_controller.cc',
        'text_input_controller.h',
        'web_ax_object_proxy.cc',
        'web_ax_object_proxy.h',
        'web_content_settings.cc',
        'web_content_settings.h',
        'web_frame_test_proxy.h',
        'web_task.cc',
        'web_task.h',
        'web_test_delegate.h',
        'web_test_interfaces.cc',
        'web_test_interfaces.h',
        'web_test_proxy.cc',
        'web_test_proxy.h',
        'web_test_runner.h',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '../../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],  # OS=="win"
      ],
    },
    {
      # GN version: //components/test_runner:resources
      'target_name': 'resources',
      'type': 'none',
      'conditions': [
        ['OS=="win"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)',
            'files': ['resources/fonts/AHEM____.TTF'],
          }],
        }],
        ['OS=="mac"', {
          'all_dependent_settings': {
            'mac_bundle_resources': [
              'resources/fonts/AHEM____.TTF',
              'resources/fonts/ChromiumAATTest.ttf',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/missingImage.png',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/textAreaResizeCorner.png',
            ],
          },
        }],
        ['use_x11 == 1', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)',
            'files': [
              'resources/fonts/AHEM____.TTF',
              'resources/fonts/fonts.conf',
              '../../third_party/gardiner_mod/GardinerModBug.ttf',
              '../../third_party/gardiner_mod/GardinerModCat.ttf',
            ]
          }],
        }],
        ['OS=="android"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)',
            'files': [
              'resources/fonts/AHEM____.TTF',
              'resources/fonts/android_main_fonts.xml',
              'resources/fonts/android_fallback_fonts.xml',
            ]
          }],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="mac" or OS=="win"', {
      'targets': [
        {
          # GN version: //components/test_runner:layout_test_helper
          'target_name': 'layout_test_helper',
          'type': 'executable',
          'sources': [
            'helper/layout_test_helper_mac.mm',
            'helper/layout_test_helper_win.cc',
          ],
          'conditions': [
            ['OS=="mac"', {
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
                ],
              },
            }],
          ],
        },
      ],
    }],  # OS=="mac" or OS=="win"
  ]
}
