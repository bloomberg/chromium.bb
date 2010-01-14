# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'renderer',
      'type': '<(library)',
      'msvs_guid': '9301A569-5D2B-4D11-9332-B1E30AEACB8D',
      'dependencies': [
        'common',
        'plugin',
        'chrome_resources',
        'chrome_strings',
        '../printing/printing.gyp:printing',
        '../skia/skia.gyp:skia',
        '../third_party/hunspell/hunspell.gyp:hunspell',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/npapi/npapi.gyp:npapi',
        # TODO(yaar): Should only stop depending on webcore and depend
        # only on webkit.
        '../third_party/WebKit/WebCore/WebCore.gyp/WebCore.gyp:webcore',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
        '../webkit/webkit.gyp:glue',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        '<@(nacl_defines)',
      ],
      'sources': [
        # TODO(jrg): to link ipc_tests, these files need to be in renderer.a.
        # But app/ is the wrong directory for them.
        # Better is to remove the dep of *_tests on renderer, but in the
        # short term I'd like the build to work.
        'renderer/automation/dom_automation_controller.cc',
        'renderer/automation/dom_automation_controller.h',
        'renderer/extensions/bindings_utils.cc',
        'renderer/extensions/bindings_utils.h',
        'renderer/extensions/event_bindings.cc',
        'renderer/extensions/event_bindings.h',
        'renderer/extensions/extension_process_bindings.cc',
        'renderer/extensions/extension_process_bindings.h',
        'renderer/extensions/js_only_v8_extensions.cc',
        'renderer/extensions/js_only_v8_extensions.h',
        'renderer/extensions/renderer_extension_bindings.cc',
        'renderer/extensions/renderer_extension_bindings.h',
        'renderer/loadtimes_extension_bindings.h',
        'renderer/loadtimes_extension_bindings.cc',
        'renderer/media/audio_renderer_impl.cc',
        'renderer/media/audio_renderer_impl.h',
        'renderer/net/render_dns_master.cc',
        'renderer/net/render_dns_master.h',
        'renderer/net/render_dns_queue.cc',
        'renderer/net/render_dns_queue.h',
        'renderer/resources/event_bindings.js',
        'renderer/resources/extension_apitest.js',
        'renderer/resources/extension_process_bindings.js',
        'renderer/resources/greasemonkey_api.js',
        'renderer/resources/json_schema.js',
        'renderer/resources/renderer_extension_bindings.js',
        'renderer/about_handler.cc',
        'renderer/about_handler.h',
        'renderer/audio_message_filter.cc',
        'renderer/audio_message_filter.h',
        'renderer/devtools_agent.cc',
        'renderer/devtools_agent.h',
        'renderer/devtools_agent_filter.cc',
        'renderer/devtools_agent_filter.h',
        'renderer/devtools_client.cc',
        'renderer/devtools_client.h',
        'renderer/dom_ui_bindings.cc',
        'renderer/dom_ui_bindings.h',
        'renderer/extension_groups.h',
        'renderer/external_host_bindings.cc',
        'renderer/external_host_bindings.h',
        'renderer/external_extension.cc',
        'renderer/external_extension.h',
        'renderer/localized_error.cc',
        'renderer/localized_error.h',
        'renderer/navigation_state.h',
        'renderer/notification_provider.cc',
        'renderer/notification_provider.h',
        'renderer/paint_aggregator.cc',
        'renderer/plugin_channel_host.cc',
        'renderer/plugin_channel_host.h',
        'renderer/print_web_view_helper.cc',
        'renderer/print_web_view_helper.h',
        'renderer/print_web_view_helper_linux.cc',
        'renderer/print_web_view_helper_mac.mm',
        'renderer/print_web_view_helper_win.cc',
        'renderer/render_process.cc',
        'renderer/render_process.h',
        'renderer/render_thread.cc',
        'renderer/render_thread.h',
        'renderer/render_view.cc',
        'renderer/render_view_linux.cc',
        'renderer/render_view.h',
        'renderer/render_widget.cc',
        'renderer/render_widget.h',
        'renderer/renderer_glue.cc',
        'renderer/renderer_histogram_snapshots.cc',
        'renderer/renderer_histogram_snapshots.h',
        'renderer/renderer_main.cc',
        'renderer/renderer_main_platform_delegate.h',
        'renderer/renderer_main_platform_delegate_linux.cc',
        'renderer/renderer_main_platform_delegate_mac.mm',
        'renderer/renderer_main_platform_delegate_win.cc',
        'renderer/renderer_sandbox_support_linux.cc',
        'renderer/renderer_sandbox_support_linux.h',
        'renderer/renderer_webkitclient_impl.cc',
        'renderer/renderer_webkitclient_impl.h',
        'renderer/renderer_webstoragearea_impl.cc',
        'renderer/renderer_webstoragearea_impl.h',
        'renderer/renderer_webstoragenamespace_impl.cc',
        'renderer/renderer_webstoragenamespace_impl.h',
        'renderer/renderer_web_database_observer.cc',
        'renderer/renderer_web_database_observer.h',
        'renderer/spellchecker/spellcheck.cc',
        'renderer/spellchecker/spellcheck.h',
        'renderer/spellchecker/spellcheck_worditerator.cc',
        'renderer/spellchecker/spellcheck_worditerator.h',
        'renderer/translate/text_translator.h',
        'renderer/translate/text_translator_impl.cc',
        'renderer/translate/text_translator_impl.h',
        'renderer/translate/page_translator.cc',
        'renderer/translate/page_translator.h',
        'renderer/user_script_idle_scheduler.cc',
        'renderer/user_script_idle_scheduler.h',
        'renderer/user_script_slave.cc',
        'renderer/user_script_slave.h',
        'renderer/visitedlink_slave.cc',
        'renderer/visitedlink_slave.h',
        'renderer/webplugin_delegate_proxy.cc',
        'renderer/webplugin_delegate_proxy.h',
        'renderer/webplugin_delegate_pepper.cc',
        'renderer/webplugin_delegate_pepper.h',
        'renderer/websharedworker_proxy.cc',
        'renderer/websharedworker_proxy.h',
        'renderer/websharedworkerrepository_impl.cc',
        'renderer/websharedworkerrepository_impl.h',
        'renderer/webworker_base.cc',
        'renderer/webworker_base.h',
        'renderer/webworker_proxy.cc',
        'renderer/webworker_proxy.h',
      ],
      'link_settings': {
        'mac_bundle_resources': [
          'renderer/renderer.sb',
        ],
      },
      'conditions': [
        ['disable_nacl!=1', {
          'dependencies': [
            'nacl',
          ],
        }],
        # Linux-specific rules.
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../sandbox/sandbox.gyp:sandbox',
          ],
        }],
        # Windows-specific rules.
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/cld',
            'third_party/wtl/include',
          ],
          'dependencies': [
            '../third_party/cld/cld.gyp:cld',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
              'export_dependent_settings': [
                '<(allocator_target)',
              ],
            }],
          ],
        }],
        ['OS=="win" or (OS=="linux" and target_arch!="arm")', {
          'sources': [
            'renderer/command_buffer_proxy.cc',
            'renderer/command_buffer_proxy.h',
          ],
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
