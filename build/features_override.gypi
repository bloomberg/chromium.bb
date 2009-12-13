# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is only included in full-chromium builds, and overrides the
# feature_defines variable in third_party/WebKit/WebKit/chromium/features.gypi.
{
  'variables': {
    # WARNING: This list of strings completely replaces the list in
    # features.gypi. Therefore, if an enable is listed in features.gypi
    # but not listed below, it will revert to its hardcoded webkit value.
    'feature_defines': [
      'ENABLE_3D_CANVAS=1',
      'ENABLE_CHANNEL_MESSAGING=1',
      'ENABLE_DATABASE=1',
      'ENABLE_DATAGRID=0',
      'ENABLE_OFFLINE_WEB_APPLICATIONS=1',
      'ENABLE_DASHBOARD_SUPPORT=0',
      'ENABLE_DOM_STORAGE=1',
      'ENABLE_JAVASCRIPT_DEBUGGER=0',
      'ENABLE_JSC_MULTIPLE_THREADS=0',
      'ENABLE_ICONDATABASE=0',
      'ENABLE_NOTIFICATIONS=1',
      'ENABLE_OPENTYPE_SANITIZER=1',
      'ENABLE_ORIENTATION_EVENTS=0',
      'ENABLE_XSLT=1',
      'ENABLE_XPATH=1',
      'ENABLE_SHARED_WORKERS=1',
      'ENABLE_SVG=1',
      'ENABLE_SVG_ANIMATION=1',
      'ENABLE_SVG_AS_IMAGE=1',
      'ENABLE_SVG_USE=1',
      'ENABLE_SVG_FOREIGN_OBJECT=1',
      'ENABLE_SVG_FONTS=1',
      'ENABLE_VIDEO=1',
      'ENABLE_WEB_SOCKETS=1',
      'ENABLE_WORKERS=1',
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
