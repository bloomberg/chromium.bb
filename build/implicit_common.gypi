# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'variables': {
      'variables': {
        # To do a shared build on linux we need to be able to choose between
        # type static_library and shared_library. We default to doing a static
        # build but you can override this with "gyp -Dlibrary=shared_library"
        # or you can add the following line (without the #) to
        # ~/.gyp/include.gypi {'variables': {'library': 'shared_library'}}
        # to compile as shared by default
        'library%': 'static_library',
      },
      'conditions': [
        # A flag for POSIX platforms
        ['OS=="win"', {
          'os_posix%': 0,
        }, {
          'os_posix%': 1,
        }],
      ],
      'library%': '<(library)',

      # Variable 'component' is for cases where we would like to build some
      # components as dynamic shared libraries but still need variable
      # 'library' for static libraries.
      # By default, component is set to whatever library is set to and
      # it can be overriden by the GYP command line or by ~/.gyp/include.gypi.
      'component%': '<(library)',
    },
    'os_posix%': '<(os_posix)',
    'library%': '<(library)',
    'component%': '<(component)',

    # TODO(sgk): eliminate this if possible.
    # It would be nicer to support this via a setting in 'target_defaults'
    # in chrome/app/locales/locales.gypi overriding the setting in the
    # 'Debug' configuration in the 'target_defaults' dict below,
    # but that doesn't work as we'd like.
    'msvs_debug_link_incremental%': '2',

    'p2p_apis%': 1,
  },
}
