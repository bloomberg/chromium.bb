# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file defines three targets that we are using to
# track the progress of the GYP->GN migration:
#
# If you run 'ninja gn_build gyp_remaining gyp_groups', and then
# run 'ninja', the second ninja invocation should do nothing. This
# indicates that everything built by a ninja build is in fact
# listed in one of these targets.
#
# 'gn_all' lists what GN is currently capable of building and should
#          match the 'gn_all' target in //BUILD.gn.
#
# 'gyp_remaining' lists all of the targets that still need to be converted,
#          i.e., all of the other (non-empty) targets that a GYP build
#          will build.
#
# 'gyp_groups' lists any empty (group) targets in the GYP build that
#          are not picked up by gn_all or gyp_remaining; this is a
#          separate target to ensure that when we build it, only
#          stamp targets file are we don't accidentally pick up something
#          not listed in one of the other two targets.
#
# TODO(GYP), TODO(dpranke) Add a build step to the bot that enforces the
#          above contracts.

{
  'targets': [
    {
      # This target should mirror the structure of //:gn_all
      # as closely as possible, for ease of comparison.
      'target_name': 'gn_all',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_i18n_perftests',
        '../base/base.gyp:base_perftests',
        '../base/base.gyp:base_unittests',
        '../base/base.gyp:build_utf8_validator_tables#host',
        '../base/base.gyp:check_example',
        '../base/base.gyp:protect_file_posix',
        '../breakpad/breakpad.gyp:core-2-minidump',
        '../breakpad/breakpad.gyp:microdump_stackwalk',
        '../breakpad/breakpad.gyp:minidump_dump',
        '../breakpad/breakpad.gyp:minidump_stackwalk',
        '../build/sanitizers/sanitizers.gyp:llvm-symbolizer',
        '../cc/cc_tests.gyp:cc_perftests',
        '../cc/cc_tests.gyp:cc_unittests',
        '../cc/blink/cc_blink_tests.gyp:cc_blink_unittests',
        '../chrome/chrome.gyp:chrome',
        '../chrome/chrome.gyp:browser_tests',
        '../chrome/chrome.gyp:chrome_app_unittests',
        '../chrome/chrome.gyp:chromedriver',
        '../chrome/chrome.gyp:chromedriver_tests',
        '../chrome/chrome.gyp:chromedriver_unittests',
        '../chrome/chrome.gyp:interactive_ui_tests',
        '../chrome/chrome.gyp:load_library_perf_tests',
        '../chrome/chrome.gyp:performance_browser_tests',
        '../chrome/chrome.gyp:service_discovery_sniffer',
        '../chrome/chrome.gyp:sync_integration_tests',
        '../chrome/chrome.gyp:sync_performance_tests',
        '../chrome/chrome.gyp:unit_tests',
        '../chrome/tools/profile_reset/jtl_compiler.gyp:jtl_compiler',
        '../cloud_print/cloud_print.gyp:cloud_print_unittests',
        '../components/components.gyp:network_hints_browser',
        '../components/components.gyp:policy_templates',
        '../components/components.gyp:webui_generator',
        '../components/components_tests.gyp:components_browsertests',
        '../components/components_tests.gyp:components_perftests',
        '../components/components_tests.gyp:components_unittests',
        '../content/content.gyp:content_app_browser',
        '../content/content.gyp:content_app_child',
        '../content/content_shell_and_tests.gyp:content_browsertests',
        '../content/content_shell_and_tests.gyp:content_gl_benchmark',
        '../content/content_shell_and_tests.gyp:content_gl_tests',
        '../content/content_shell_and_tests.gyp:content_perftests',
        '../content/content_shell_and_tests.gyp:content_shell',
        '../content/content_shell_and_tests.gyp:content_unittests',
        '../courgette/courgette.gyp:courgette',
        '../courgette/courgette.gyp:courgette_fuzz',
        '../courgette/courgette.gyp:courgette_minimal_tool',
        '../courgette/courgette.gyp:courgette_unittests',
        '../crypto/crypto.gyp:crypto_unittests',
        '../extensions/extensions_tests.gyp:extensions_browsertests',
        '../extensions/extensions_tests.gyp:extensions_unittests',
        '../dbus/dbus.gyp:dbus_test_server',
        '../device/device_tests.gyp:device_unittests',
        '../gin/gin.gyp:gin_v8_snapshot_fingerprint',
        '../gin/gin.gyp:gin_shell',
        '../gin/gin.gyp:gin_unittests',
        '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
        '../google_apis/gcm/gcm.gyp:mcs_probe',
        '../google_apis/google_apis.gyp:google_apis_unittests',
        '../gpu/gpu.gyp:angle_unittests',
        '../gpu/gpu.gyp:gl_tests',
        '../gpu/gpu.gyp:gpu_perftests',
        '../gpu/gpu.gyp:gpu_unittests',
        '../gpu/gles2_conform_support/gles2_conform_support.gyp:gles2_conform_support',  # TODO(GYP) crbug.com/471920
        '../gpu/gles2_conform_support/gles2_conform_test.gyp:gles2_conform_test',  # TODO(GYP) crbug.com/471920
        '../gpu/khronos_glcts_support/khronos_glcts_test.gyp:khronos_glcts_test',  # TODO(GYP) crbug.com/471903 to make this complete.
        '../ipc/ipc.gyp:ipc_perftests',
        '../ipc/ipc.gyp:ipc_tests',
        '../ipc/mojo/ipc_mojo.gyp:ipc_mojo_unittests',
        '../jingle/jingle.gyp:jingle_unittests',
        '../media/media.gyp:ffmpeg_regression_tests',  # TODO(GYP) this should be conditional on media_use_ffmpeg
        '../media/media.gyp:media_perftests',
        '../media/media.gyp:media_unittests',
        '../media/cast/cast.gyp:cast_benchmarks',
        '../media/cast/cast.gyp:cast_unittests',
        '../media/cast/cast.gyp:generate_barcode_video',
        '../media/cast/cast.gyp:generate_timecode_audio',
        '../media/cast/cast.gyp:tap_proxy',
        '../mojo/mojo.gyp:mojo',
        '../mojo/mojo_base.gyp:mojo_application_chromium',
        '../mojo/mojo_base.gyp:mojo_common_unittests',
        '../net/net.gyp:crash_cache',
        '../net/net.gyp:crl_set_dump',
        '../net/net.gyp:dns_fuzz_stub',
        '../net/net.gyp:dump_cache',
        '../net/net.gyp:gdig',
        '../net/net.gyp:get_server_time',
        '../net/net.gyp:hpack_example_generator',
        '../net/net.gyp:hpack_fuzz_mutator',
        '../net/net.gyp:hpack_fuzz_wrapper',
        '../net/net.gyp:net_perftests',
        '../net/net.gyp:net_unittests',
        '../net/net.gyp:net_watcher',  # TODO(GYP): This should be conditional on use_v8_in_net
        '../net/net.gyp:run_testserver',
        '../net/net.gyp:stress_cache',
        '../net/net.gyp:tld_cleanup',
        '../ppapi/ppapi_internal.gyp:ppapi_example_audio',
        '../ppapi/ppapi_internal.gyp:ppapi_example_audio_input',
        '../ppapi/ppapi_internal.gyp:ppapi_example_c_stub',
        '../ppapi/ppapi_internal.gyp:ppapi_example_cc_stub',
        '../ppapi/ppapi_internal.gyp:ppapi_example_compositor',
        '../ppapi/ppapi_internal.gyp:ppapi_example_crxfs',
        '../ppapi/ppapi_internal.gyp:ppapi_example_enumerate_devices',
        '../ppapi/ppapi_internal.gyp:ppapi_example_file_chooser',
        '../ppapi/ppapi_internal.gyp:ppapi_example_flash_topmost',
        '../ppapi/ppapi_internal.gyp:ppapi_example_gamepad',
        '../ppapi/ppapi_internal.gyp:ppapi_example_gles2',
        '../ppapi/ppapi_internal.gyp:ppapi_example_gles2_spinning_cube',
        '../ppapi/ppapi_internal.gyp:ppapi_example_graphics_2d',
        '../ppapi/ppapi_internal.gyp:ppapi_example_ime',
        '../ppapi/ppapi_internal.gyp:ppapi_example_input',
        '../ppapi/ppapi_internal.gyp:ppapi_example_media_stream_audio',
        '../ppapi/ppapi_internal.gyp:ppapi_example_media_stream_video',
        '../ppapi/ppapi_internal.gyp:ppapi_example_mouse_cursor',
        '../ppapi/ppapi_internal.gyp:ppapi_example_mouse_lock',
        '../ppapi/ppapi_internal.gyp:ppapi_example_paint_manager',
        '../ppapi/ppapi_internal.gyp:ppapi_example_post_message',
        '../ppapi/ppapi_internal.gyp:ppapi_example_printing',
        '../ppapi/ppapi_internal.gyp:ppapi_example_scaling',
        '../ppapi/ppapi_internal.gyp:ppapi_example_scroll',
        '../ppapi/ppapi_internal.gyp:ppapi_example_simple_font',
        '../ppapi/ppapi_internal.gyp:ppapi_example_threading',
        '../ppapi/ppapi_internal.gyp:ppapi_example_url_loader',
        '../ppapi/ppapi_internal.gyp:ppapi_example_url_loader_file',
        '../ppapi/ppapi_internal.gyp:ppapi_example_vc',
        '../ppapi/ppapi_internal.gyp:ppapi_example_video_decode',
        '../ppapi/ppapi_internal.gyp:ppapi_example_video_decode_dev',
        '../ppapi/ppapi_internal.gyp:ppapi_example_video_effects',
        '../ppapi/ppapi_internal.gyp:ppapi_example_video_encode',
        '../ppapi/ppapi_internal.gyp:ppapi_tests',  # TODO(GYP): Split out the examples and tests
        '../ppapi/ppapi_internal.gyp:ppapi_unittests',
        '../ppapi/tools/ppapi_tools.gyp:pepper_hash_for_uma',
        '../printing/printing.gyp:printing_unittests',
        '../skia/skia_tests.gyp:skia_unittests',
        '../skia/skia.gyp:filter_fuzz_stub',
        '../skia/skia.gyp:image_operations_bench',
        '../sql/sql.gyp:sql_unittests',
        '../sync/sync.gyp:run_sync_testserver',
        '../sync/sync.gyp:sync_endtoend_tests',
        '../sync/sync.gyp:sync_unit_tests',
        '../sync/tools/sync_tools.gyp:sync_client',
        '../sync/tools/sync_tools.gyp:sync_listen_notifications',
        '../testing/gmock.gyp:gmock_main',
        '../third_party/WebKit/public/all.gyp:blink_tests',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
        '../third_party/codesighs/codesighs.gyp:codesighs',
        '../third_party/codesighs/codesighs.gyp:maptsvdifftool',
        '../third_party/leveldatabase/leveldatabase.gyp:env_chromium_unittests',
        '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
        '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
        '../third_party/mojo/mojo_edk_tests.gyp:mojo_system_unittests',
        '../third_party/mojo/mojo_edk_tests.gyp:mojo_public_bindings_unittests',
        '../third_party/mojo/mojo_edk_tests.gyp:mojo_public_environment_unittests',
        '../third_party/mojo/mojo_edk_tests.gyp:mojo_public_system_perftests',
        '../third_party/mojo/mojo_edk_tests.gyp:mojo_public_system_unittests',
        '../third_party/mojo/mojo_edk_tests.gyp:mojo_public_utility_unittests',
        '../third_party/pdfium/samples/samples.gyp:pdfium_test',
        '../third_party/smhasher/smhasher.gyp:pmurhash',
        '../third_party/sqlite/sqlite.gyp:sqlite_shell',
        '../tools/gn/gn.gyp:gn',
        '../tools/gn/gn.gyp:generate_test_gn_data',
        '../tools/gn/gn.gyp:gn_unittests',
        '../tools/imagediff/image_diff.gyp:image_diff',
        '../tools/perf/clear_system_cache/clear_system_cache.gyp:clear_system_cache',
        '../tools/telemetry/telemetry.gyp:bitmaptools#host',
        '../ui/accessibility/accessibility.gyp:accessibility_unittests',
        '../ui/app_list/app_list.gyp:app_list_unittests',
        '../ui/base/ui_base_tests.gyp:ui_base_unittests',
        '../ui/compositor/compositor.gyp:compositor_unittests',
        '../ui/display/display.gyp:display_unittests',
        '../ui/events/events.gyp:events_unittests',
        '../ui/gfx/gfx_tests.gyp:gfx_unittests',
        '../ui/keyboard/keyboard.gyp:keyboard_unittests',
        '../ui/message_center/message_center.gyp:message_center_unittests',
        '../ui/snapshot/snapshot.gyp:snapshot_unittests',
        '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection_unittests',
        '../ui/views/examples/examples.gyp:views_examples_with_content_exe',
        '../url/url.gyp:url_unittests',
        '../v8/tools/gyp/v8.gyp:v8_snapshot',
        '../v8/tools/gyp/v8.gyp:postmortem-metadata',
      ],
      'conditions': [
        ['chromeos== 1 or use_ash==1', {
          'dependencies': [
            '../components/components.gyp:session_manager_component',
          ]
        }],
        ['disable_nacl==0 and disable_nacl_untrusted==0', {
          'dependencies': [
             '../remoting/remoting.gyp:remoting_key_tester',
          ],
        }],
        ['disable_nacl==0 and disable_nacl_untrusted==0', {
          'dependencies': [
            '../components/nacl.gyp:nacl_loader_unittests',
          ]
        }],
        ['enable_extensions==1 and OS!="mac"', {
          'dependencies': [
            '../extensions/shell/app_shell.gyp:app_shell',
            '../extensions/shell/app_shell.gyp:app_shell_unittests',
          ],
        }],
        ['OS!="win"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:symupload',
          ],
        }],
        ['remoting==1', {
          'dependencies': [
            '../remoting/remoting.gyp:remoting_host',
            '../remoting/remoting.gyp:remoting_it2me_native_messaging_host',
            '../remoting/remoting.gyp:remoting_me2me_native_messaging_host',
            '../remoting/remoting.gyp:remoting_me2me_host',
            '../remoting/remoting.gyp:remoting_native_messaging_manifests',
            '../remoting/remoting.gyp:remoting_perftests',
            '../remoting/remoting.gyp:remoting_start_host',
            '../remoting/remoting.gyp:remoting_unittests',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../media/media.gyp:player_x11',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'conditions': [
            ['target_arch!="arm"', {
              'dependencies': [
                '../gpu/tools/tools.gyp:compositor_model_bench',
              ],
            }],
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/app_list/app_list.gyp:app_list_demo',
            '../ui/views/views.gyp:views_unittests',
          ],
        }],
        ['use_ash==1', {
          'dependencies': [
            '../ash/ash.gyp:ash_shell',
            '../ash/ash.gyp:ash_shell_unittests',
            '../ash/ash.gyp:ash_unittests',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../ui/aura/aura.gyp:aura_bench',
            '../ui/aura/aura.gyp:aura_demo',
            '../ui/aura/aura.gyp:aura_unittests',
            '../ui/wm/wm.gyp:wm_unittests',
          ],
        }],
        ['use_ozone==1', {
          'dependencies': [
            '../ui/ozone/ozone.gyp:ozone',
          ],
        }],
        ['OS=="win" or OS=="mac" or chromeos==1', {
          'dependencies': [
            '../rlz/rlz.gyp:rlz_lib',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '../base/base.gyp:chromium_android_linker',
            '../breakpad/breakpad.gyp:dump_syms',
            '../build/android/rezip.gyp:rezip_apk_jar',
            '../chrome/chrome.gyp:chrome_shell_apk',
            '../chrome/chrome.gyp:chromedriver_webview_shell_apk',
            #"//clank" TODO(GYP) - conditional somehow?
            '../tools/imagediff/image_diff.gyp:image_diff#host',
            '../tools/telemetry/telemetry.gyp:bitmaptools#host',

            # TODO(GYP): Remove these when the components_unittests work.
            #"//components/history/core/test:test",
            #"//components/policy:policy_component_test_support",
            #"//components/policy:test_support",
            #"//components/rappor:test_support",
            #"//components/signin/core/browser:test_support",
            #"//components/sync_driver:test_support",
            #"//components/user_manager",
            #"//components/wallpaper",

            '../content/content_shell_and_tests.gyp:content_shell_apk',

            # TODO(GYP): Are these needed, or will they be pulled in automatically?
            #"//third_party/android_tools:android_gcm_java",
            #"//third_party/android_tools:uiautomator_java",
            #"//third_party/android_tools:android_support_v13_java",
            #"//third_party/android_tools:android_support_v7_appcompat_java",
            #"//third_party/android_tools:android_support_v7_mediarouter_java",
            #"//third_party/mesa",
            #"//third_party/mockito:mockito_java",
            #"//third_party/openmax_dl/dl",
            #"//third_party/speex",
            #"//ui/android:ui_java",

            # TODO(GYP): Are these needed?
            #"//chrome/test:test_support_unit",
            #"//third_party/smhasher:murmurhash3",
            #"//ui/message_center:test_support",
          ],
          'dependencies!': [
            '../breakpad/breakpad.gyp:symupload',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:chromedriver',
            '../chrome/chrome.gyp:chromedriver_unitests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../extensions/extensions_tests.gyp:extensions_browsertests',
            '../extensions/extensions_tests.gyp:extensions_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../net/net.gyp:net_unittests',
            #"//ppapi/examples",
            '../third_party/pdfium/samples/samples.gyp:pdfium_test',
            '../tools/gn/gn.gyp:gn',
            '../tools/gn/gn.gyp:gn_unittests',
            '../tools/imagediff/image_diff.gyp:image_diff',
            '../tools/gn/gn.gyp:gn',
            '../tools/gn/gn.gyp:gn_unittests',
            '../ui/app_list/app_list.gyp:app_list_unittests',
            '../url/url.gyp:url_unittests',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:breakpad_unittests',
            '../breakpad/breakpad.gyp:dump_syms',
            '../breakpad/breakpad.gyp:generate_test_dump',
            '../breakpad/breakpad.gyp:minidump-2-core',
            '../dbus/dbus.gyp:dbus_test_server',
            '../dbus/dbus.gyp:dbus_unittests',
            '../media/cast/cast.gyp:tap_proxy',
            '../net/net.gyp:disk_cache_memory_test',
            '../net/net.gyp:flip_in_mem_edsm_server',
            '../net/net.gyp:flip_in_mem_edsm_server_unittests',
            '../net/net.gyp:hpack_example_generator',
            '../net/net.gyp:hpack_fuzz_mutator',
            '../net/net.gyp:hpack_fuzz_wrapper',
            '../net/net.gyp:net_perftests',
            '../net/net.gyp:quic_client',
            '../net/net.gyp:quic_server',
            '../sandbox/sandbox.gyp:chrome_sandbox',
            '../sandbox/sandbox.gyp:sandbox_linux_unittests',
            '../sandbox/sandbox.gyp:sandbox_linux_jni_unittests',
         ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:crash_inspector',
            '../breakpad/breakpad.gyp:dump_syms',
            '../breakpad/breakpad.gyp:symupload',
            '../third_party/apple_sample_code/apple_sample_code.gyp:apple_sample_code',
            '../third_party/molokocacao/molokocacao.gyp:molokocacao',

            # TODO(GYP): remove these when the corresponding root targets work.
            #"//cc/blink",
            #"//components/ui/zoom:ui_zoom",
            #"//content",
            #"//content/test:test_support",
            #"//device/battery",
            #"//device/bluetooth",
            #"//device/nfc",
            #"//device/usb",
            #"//device/vibration",
            #"//media/blink",
            #"//pdf",
            #"//storage/browser",
            #"//third_party/brotli",
            #"//third_party/flac",
            #"//third_party/hunspell",
            #//third_party/iccjpeg",
            #"//third_party/libphonenumber",
            #"//third_party/ots",
            #"//third_party/qcms",
            #"//third_party/smhasher:murmurhash3",
            #"//third_party/speex",
            #"//third_party/webrtc/system_wrappers",
            #"//ui/native_theme",
            #"//ui/snapshot",
            #"//ui/surface",
          ],
          'dependencies!': [
            #"//chrome",  # TODO(GYP)
            #"//chrome/test:browser_tests",  # TODO(GYP)
            #"//chrome/test:interactive_ui_tests",  # TODO(GYP)
            #"//chrome/test:sync_integration_tests",  # TODO(GYP)
            #"//chrome/test:unit_tests",  # TODO(GYP)
            #"//components:components_unittests",  # TODO(GYP)
            #"//content/test:content_browsertests",  # TODO(GYP)
            #"//content/test:content_perftests",  # TODO(GYP)
            #"//content/test:content_unittests",  # TODO(GYP)
            #"//extensions:extensions_browsertests",  # TODO(GYP)
            #"//extensions:extensions_unittests",  # TODO(GYP)
            #"//net:net_unittests",  # TODO(GYP)
            #"//third_party/usrsctp",  # TODO(GYP)
            #"//ui/app_list:app_list_unittests",  # TODO(GYP)
            #"//ui/gfx:gfx_unittests",  # TODO(GYP)
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            "//ui/metro_viewer",
            '../third_party/codesighs/codesighs.gyp:msdump2symdb',
          ],
          'dependencies!': [
            "//crypto:crypto_unittests",  # TODO(GYP)
            "//net:net_unittests",  # TODO(GYP)
          ],
        }, {
          'dependencies': [
            '../third_party/codesighs/codesighs.gyp:nm2tsv',
          ],
        }],
        ['OS=="android" or OS=="linux"', {
          'dependencies': [
            '../net/net.gyp:disk_cache_memory_test',
          ],
        }],
      ],
    },
    {
      'target_name': 'gyp_only',
      'type': 'none',
      'conditions': [
        ['OS=="linux"', {
          'conditions': [
            ['disable_nacl==0 and disable_nacl_untrusted==0', {
              'dependencies': [
                '../mojo/mojo_nacl.gyp:monacl_shell',  # This should not be built in chromium.
              ]
            }],
          ]
        }],
      ],
    },
    {
      'target_name': 'gyp_remaining',
      'type': 'none',
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../third_party/webrtc/tools/tools.gyp:frame_analyzer',
            '../third_party/webrtc/tools/tools.gyp:rgba_to_i420_converter',
          ],
          'conditions': [
            ['remoting==1', {
              'dependencies': [
                '../remoting/app_remoting_webapp.gyp:ar_sample_app',  # crbug.com/471916
              ],
              'conditions': [
                ['disable_nacl==0 and disable_nacl_untrusted==0', {
                  'dependencies': [
                    '../remoting/remoting.gyp:remoting_key_tester',
                  ]
                }],
              ],
            }],
            ['test_isolation_mode!="noop"', {
              'dependencies': [
                '../base/base.gyp:base_unittests_run',
                '../cc/cc_tests.gyp:cc_unittests_run',
                '../chrome/chrome.gyp:browser_tests_run',
                '../chrome/chrome.gyp:chrome_run',
                '../chrome/chrome.gyp:interactive_ui_tests_run',
                '../chrome/chrome.gyp:sync_integration_tests_run',
                '../chrome/chrome.gyp:unit_tests_run',
                '../components/components_tests.gyp:components_browsertests_run',
                '../components/components_tests.gyp:components_unittests_run',
                '../content/content_shell_and_tests.gyp:content_browsertests_run',
                '../content/content_shell_and_tests.gyp:content_unittests_run',
                '../crypto/crypto.gyp:crypto_unittests_run',
                '../courgette/courgette.gyp:courgette_unittests_run',
                '../gpu/gpu.gyp:gpu_unittests_run',
                '../media/cast/cast.gyp:cast_unittests_run',
                '../media/media.gyp:media_unittests_run',
                '../net/net.gyp:net_unittests_run',
                '../sandbox/sandbox.gyp:sandbox_linux_unittests_run',
                '../sql/sql.gyp:sql_unittests_run',
                '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests_run',
                '../ui/accessibility/accessibility.gyp:accessibility_unittests_run',
                '../ui/app_list/app_list.gyp:app_list_unittests_run',
                '../ui/events/events.gyp:events_unittests_run',
                '../ui/message_center/message_center.gyp:message_center_unittests_run',
                '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection_unittests_run',
              ],
              'conditions': [
                ['use_ash==1', {
                  'dependencies': [
                    '../ash/ash.gyp:ash_unittests_run',
                  ],
                }],
              ],
            }],
          ],
        }],
      ],
    },
    {
      # This target, when built, should cause no actual work
      # to be done, just update a bunch of stamp files.
      'target_name': 'gyp_groups',
      'type': 'none',
      'dependencies': [
        'All',
        'aura_builder',
        'blink_tests',
        'chromium_builder_asan',
        'chromium_builder_chromedriver',
        'chromium_builder_perf',
        'chromium_builder_tests',
        'chromium_builder_webrtc',
        'chromium_gpu_builder',
        'chromium_gpu_debug_builder',
      ],
    },
  ]
}

