# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/network_hints/common
      'target_name': 'network_hints_common',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../url/ipc/url_ipc.gyp:url_ipc',
      ],
      'sources': [
        'network_hints/common/network_hints_common.cc',
        'network_hints/common/network_hints_common.h',
      ],
    },
    {
      # GN version: //components/network_hints/browser
      'target_name': 'network_hints_browser',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'network_hints_mojom',
        'network_hints_public_cpp',
        '../content/content.gyp:content_browser',
        '../net/net.gyp:net',
      ],
      'sources': [
        'network_hints/browser/network_hints_impl.cc',
        'network_hints/browser/network_hints_impl.h',
      ],
    },
    {
      # GN version: //components/network_hints/public/cpp
      'target_name': 'network_hints_public_cpp',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'network_hints_common',
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
        '../url/ipc/url_ipc.gyp:url_ipc',
      ],
      'sources': [
        'network_hints/public/cpp/network_hints_param_traits.cc',
        'network_hints/public/cpp/network_hints_param_traits.h',
      ],
    },
  ],
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          # GN version: //components/network_hints/renderer
          'target_name': 'network_hints_renderer',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'network_hints_common',
            'network_hints_mojom',
            'network_hints_public_cpp',
            '../content/content.gyp:content_renderer',
            '../services/shell/shell_public.gyp:shell_public',
            '../third_party/WebKit/public/blink.gyp:blink',
          ],
          'sources': [
            'network_hints/renderer/dns_prefetch_queue.cc',
            'network_hints/renderer/dns_prefetch_queue.h',
            'network_hints/renderer/prescient_networking_dispatcher.cc',
            'network_hints/renderer/prescient_networking_dispatcher.h',
            'network_hints/renderer/renderer_dns_prefetch.cc',
            'network_hints/renderer/renderer_dns_prefetch.h',
            'network_hints/renderer/renderer_preconnect.cc',
            'network_hints/renderer/renderer_preconnect.h',
          ],
        },
        {
          # GN version: //components/network_hints/public/interfaces:network_hints_mojom
          'target_name': 'network_hints_mojom',
          'type': 'static_library',
          'dependencies': [
            '../mojo/mojo_public.gyp:mojo_cpp_bindings',
            '../url/url.gyp:url_mojom',
          ],
          'sources': [
            'network_hints/public/interfaces/network_hints.mojom',
          ],
          'includes': [ '../mojo/mojom_bindings_generator.gypi' ],
          'variables': {
            'mojom_typemaps': [
              '../url/mojo/gurl.typemap',
              'network_hints/public/cpp/network_hints.typemap',
            ],
          },
        },
      ],
    }],
  ],
}
