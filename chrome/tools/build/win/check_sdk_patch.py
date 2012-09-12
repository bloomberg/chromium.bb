#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to check that the Windows 8 SDK has been appropriately patched so that
   it can be used with VS 2010.

   In practice, this checks for the presence of 'enum class' in asyncinfo.h.
   Changing that to 'enum' is the only thing needed to build with the WinRT
   headers in VS 2010.
"""

import os
import sys


def main(argv):
  if len(argv) < 2:
    print "Usage: check_sdk_patch.py path_to_windows_8_sdk [dummy_output_file]"
    return 1

  # Look for asyncinfo.h
  async_info_path = os.path.join(argv[1], 'Include/winrt/asyncinfo.h')
  if not os.path.exists(async_info_path):
    print ("Could not find %s in provided SDK path. Please check input." %
           async_info_path)
    print "CWD: %s" % os.getcwd()
    return 2
  else:
    if 'enum class' in open(async_info_path).read():
      print ("\nERROR: You are using an unpatched Windows 8 SDK located at %s."
             "\nPlease see instructions at"
             "\nhttp://www.chromium.org/developers/how-tos/"
             "build-instructions-windows\nfor how to apply the patch to build "
             "with VS2010.\n" % argv[1])
      return 3
    else:
      if len(argv) > 2:
        with open(argv[2], 'w') as dummy_file:
          dummy_file.write('Windows 8 SDK has been patched!')

      # Patched Windows 8 SDK found.
      return 0


if '__main__' == __name__:
  sys.exit(main(sys.argv))
