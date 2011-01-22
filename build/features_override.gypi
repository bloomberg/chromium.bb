# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is only included in full-chromium builds, and overrides the
# feature_defines variable in
# third_party/WebKit/Source/WebKit/chromium/features.gypi.
{
  'variables': {
    # WARNING: This list of strings completely replaces the list in
    # features.gypi. Therefore, if an enable is listed in features.gypi
    # but not listed below, it will revert to its hardcoded webkit value.
    'feature_defines': [
      'ENABLE_3D_CANVAS=1',
      'ENABLE_3D_PLUGIN=1',
      'ENABLE_BLOB=1',
      'ENABLE_BLOB_SLICE=1',
      'ENABLE_CHANNEL_MESSAGING=1',
      'ENABLE_CLIENT_BASED_GEOLOCATION=1',
      'ENABLE_DASHBOARD_SUPPORT=0',
      'ENABLE_DATABASE=1',
      'ENABLE_DATAGRID=0',
      'ENABLE_DEVICE_ORIENTATION=1',
      'ENABLE_DIRECTORY_UPLOAD=1',
      'ENABLE_DOM_STORAGE=1',
      'ENABLE_EVENTSOURCE=1',
      'ENABLE_FILE_SYSTEM=1',
      'ENABLE_FILTERS=1',
      'ENABLE_GEOLOCATION=1',
      'ENABLE_ICONDATABASE=0',
      'ENABLE_INDEXED_DATABASE=1',
      'ENABLE_INPUT_SPEECH=1',
      'ENABLE_JAVASCRIPT_DEBUGGER=1',
      'ENABLE_JSC_MULTIPLE_THREADS=0',
      'ENABLE_LINK_PREFETCH=1',
      'ENABLE_METER_TAG=1',
      'ENABLE_NOTIFICATIONS=1',
      'ENABLE_OFFLINE_WEB_APPLICATIONS=1',
      'ENABLE_OPENTYPE_SANITIZER=1',
      'ENABLE_ORIENTATION_EVENTS=0',
      'ENABLE_PROGRESS_TAG=1',
      'ENABLE_REQUEST_ANIMATION_FRAME=1',
      'ENABLE_RUBY=1',
      'ENABLE_SANDBOX=1',
      'ENABLE_SHARED_WORKERS=1',
      'ENABLE_SVG=<(enable_svg)',
      'ENABLE_SVG_ANIMATION=<(enable_svg)',
      'ENABLE_SVG_AS_IMAGE=<(enable_svg)',
      'ENABLE_SVG_FONTS=<(enable_svg)',
      'ENABLE_SVG_FOREIGN_OBJECT=<(enable_svg)',
      'ENABLE_SVG_USE=<(enable_svg)',
      'ENABLE_TOUCH_EVENTS=<(enable_touch_events)',
      'ENABLE_V8_SCRIPT_DEBUG_SERVER=1',
      'ENABLE_VIDEO=1',
      'ENABLE_WEB_SOCKETS=1',
      'ENABLE_WEB_AUDIO=1',
      'ENABLE_WEB_TIMING=1',
      'ENABLE_WORKERS=1',
      'ENABLE_XHR_RESPONSE_BLOB=1',
      'ENABLE_XPATH=1',
      'ENABLE_XSLT=1',
      'WTF_USE_WEBP=1',
      'WTF_USE_WEBKIT_IMAGE_DECODERS=1',
    ],
    # We have to nest variables inside variables so that they can be overridden
    # through GYP_DEFINES.
    'variables': {
      'use_accelerated_compositing%': 1,
      'enable_svg%': 1,
      'enable_touch_events%': 1,
    },
    'use_accelerated_compositing%': '<(use_accelerated_compositing)',
    'enable_svg%': '<(enable_svg)',
    'enable_touch_events%': '<(enable_touch_events)',
    'conditions': [
      ['(OS=="win" or OS=="linux" or OS=="mac") and use_accelerated_compositing==1', {
        'feature_defines': [
          'WTF_USE_ACCELERATED_COMPOSITING=1',
          'ENABLE_3D_RENDERING=1',
          'ENABLE_ACCELERATED_2D_CANVAS=1',
        ],
        'use_accelerated_compositing': 1,
      }],
    ],
    # TODO: If the need arises, create a mechanism that will intelligently
    # merge the lists rather than replace one with the other. This may
    # require changes in gyp.
  },

}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
