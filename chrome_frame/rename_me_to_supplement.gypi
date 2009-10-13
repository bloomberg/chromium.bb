# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# It is used to override the Chrome Frame appid, as well as
# add extra required source files and defines. The file is checked in as
# rename_me_to_supplement.gypi so as to not be active on the official builder.
# This is required because with these fields present, Chrome will build in
# Chrome Frame mode, which isn't what the Chrome official builder wants yet.
#
# Renaming this file to supplement.gypi will cause gyp to pick it up. 
# supplement.gypi is a magic gyp include file that gets pulled in before any
# other includes. 
#
# The official builder will define these extra vars and whatnot in the build
# scripts themselves.
{
  'variables': {
    'google_update_appid': '{8BA986DA-5100-405E-AA35-86F34A02ACBF}',
    'extra_installer_util_sources': 0,
    'chrome_frame_define': 1,
  },
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
