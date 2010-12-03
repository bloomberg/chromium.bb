# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This gypi lists all the files that need to be included in the CEEE
# xpi.  This is used by the firefox gyp file used to build the xpi as well
# as the installer gyp to extract the xpi.
#
# The gyp file that includes this file can pre-define the xpi_file_root
# variable, which acts as a root directory that will be prefixed to each
# file name.
#
# xpi_files lists all the files needed for production.
# xpi_files_test lists additional files needed for testing.
{
  'variables': {
    'xpi_file_root': '',

    # Comma-separated list of extensions to ignore when expanding
    # version and branding strings.
    'xpi_no_expand_extensions': 'png',
  },
  'xpi_files': [
    '<(xpi_file_root)chrome.manifest',
    '<(xpi_file_root)content/about.xul',
    '<(xpi_file_root)content/bookmarks_api.js',
    '<(xpi_file_root)content/cf.js',
    '<(xpi_file_root)content/cookie_api.js',
    '<(xpi_file_root)content/infobars_api.js',
    '<(xpi_file_root)content/ceee.png',
    '<(xpi_file_root)content/options.xul',
    '<(xpi_file_root)content/overlay.js',
    '<(xpi_file_root)content/overlay.xul',
    '<(xpi_file_root)content/sidebar_api.js',
    '<(xpi_file_root)content/tab_api.js',
    '<(xpi_file_root)content/us/base.js',
    '<(xpi_file_root)content/us/json.js',
    '<(xpi_file_root)content/us/ceee_bootstrap.js',
    '<(xpi_file_root)content/userscript_api.js',
    '<(xpi_file_root)content/window_api.js',
    '<(xpi_file_root)defaults/preferences/ceee.js',
    '<(xpi_file_root)install.rdf',
    '<(xpi_file_root)locale/en-US/about.dtd',
    '<(xpi_file_root)locale/en-US/ceee.dtd',
    '<(xpi_file_root)locale/en-US/ceee.properties',
    '<(xpi_file_root)locale/en-US/options.dtd',
    '<(xpi_file_root)modules/global.js',
    '<(xpi_file_root)skin/overlay.css',
  ],
  'xpi_test_files': [
    '<(xpi_file_root)content/js-coverage.js',
  ],
}
