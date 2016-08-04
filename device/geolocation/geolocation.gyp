# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which platforms to
    # include source files on (e.g. files ending in _mac.h or _mac.cc are only
    # compiled on MacOSX).
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/geolocation:device_geolocation
      'target_name': 'device_geolocation',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/google_apis/google_apis.gyp:google_apis',
        '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
        '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
        # TODO(mcasas): move geolocation.mojom to public/interfaces.
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:mojo_bindings',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
      ],
      'defines': [
        'DEVICE_GEOLOCATION_IMPLEMENTATION',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'access_token_store.h',
        'android/geolocation_jni_registrar.cc',
        'android/geolocation_jni_registrar.h',
        'empty_wifi_data_provider.cc',
        'empty_wifi_data_provider.h',
        'geolocation_delegate.cc',
        'geolocation_delegate.h',
        'geolocation_export.h',
        'geolocation_provider.h',
        'geolocation_provider_impl.cc',
        'geolocation_provider_impl.h',
        'geolocation_service_context.h',
        'geolocation_service_context.cc',
        'geolocation_service_impl.cc',
        'geolocation_service_impl.h',
        'geoposition.cc',
        'geoposition.h',
        'location_api_adapter_android.cc',
        'location_api_adapter_android.h',
        'location_arbitrator.h',
        'location_arbitrator_impl.cc',
        'location_arbitrator_impl.h',
        'location_provider.h',
        'location_provider_android.cc',
        'location_provider_android.h',
        'location_provider_base.cc',
        'location_provider_base.h',
        'network_location_provider.cc',
        'network_location_provider.h',
        'network_location_request.cc',
        'network_location_request.h',
        'wifi_data.cc',
        'wifi_data.h',
        'wifi_data_provider.cc',
        'wifi_data_provider.h',
        'wifi_data_provider_chromeos.cc',
        'wifi_data_provider_chromeos.h',
        'wifi_data_provider_common.cc',
        'wifi_data_provider_common.h',
        'wifi_data_provider_common_win.cc',
        'wifi_data_provider_common_win.h',
        'wifi_data_provider_corewlan_mac.mm',
        'wifi_data_provider_linux.cc',
        'wifi_data_provider_linux.h',
        'wifi_data_provider_mac.cc',
        'wifi_data_provider_mac.h',
        'wifi_data_provider_manager.cc',
        'wifi_data_provider_manager.h',
        'wifi_data_provider_win.cc',
        'wifi_data_provider_win.h',
        'wifi_polling_policy.h',
      ],

      'conditions': [
        ['OS=="android"', {
          'sources!': [
            'network_location_provider.cc',
            'network_location_provider.h',
            'network_location_request.cc',
            'network_location_request.h',
          ],
        }],

        # Dealing with *wifi_data_provider_*.cc is also a bit complicated given
        # android, chromeos, linux and use_dbus.
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chromeos/chromeos.gyp:chromeos',
          ],
          'sources!': [
            'wifi_data_provider_linux.cc'
          ],
        }],

        ['OS=="linux"', {
          'conditions': [
            ["use_dbus==1", {
              'dependencies': [
                '<(DEPTH)/build/linux/system.gyp:dbus',
                '<(DEPTH)/dbus/dbus.gyp:dbus',
              ],
              'sources!': [
                'empty_wifi_data_provider.cc',
              ],
            }, {  # use_dbus==0
              'geolocation_unittest_sources!': [
                'wifi_data_provider_linux.cc',
              ],
            }]
          ],
        }],

        ['OS=="win"', {
          # TODO(jschuh): http://crbug.com/167187 fix size_t to int truncations.
         'msvs_disabled_warnings': [ 4267, ],
        }],

        ['OS=="mac" or OS=="win"', {
          'sources!': [
            'empty_wifi_data_provider.cc',
            'empty_wifi_data_provider.h',
          ],
        }],
      ],
    },
  ],
}
