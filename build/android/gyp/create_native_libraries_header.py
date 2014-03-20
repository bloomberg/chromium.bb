#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes .h files for NativeLibraries.template

The native library list header should contain the list of native libraries to
load in the form:
  = { "lib1", "lib2" }
The version header should contain a version name string of the form
  = "version_name"
"""

import json
import optparse
import os
import sys

from util import build_utils


def main():
  parser = optparse.OptionParser()

  parser.add_option('--native-library-list',
                    help='Path to generated .java file containing library list')
  parser.add_option('--version-output',
                    help='Path to generated .java file containing version name')
  parser.add_option('--ordered-libraries',
      help='Path to json file containing list of ordered libraries')
  parser.add_option('--version-name',
                    help='expected version name of native library')

  # args should be the list of libraries in dependency order.
  options, _ = parser.parse_args()

  build_utils.MakeDirectory(os.path.dirname(options.native_library_list))

  with open(options.ordered_libraries, 'r') as libfile:
    libraries = json.load(libfile)
  # Generates string of the form '= { "base", "net",
  # "content_shell_content_view" }' from a list of the form ["libbase.so",
  # libnet.so", "libcontent_shell_content_view.so"]
  libraries = ['"' + lib[3:-3] + '"' for lib in libraries]
  array = '= { ' + ', '.join(libraries) + '}'

  with open(options.native_library_list, 'w') as header:
    header.write(array)

  with open(options.version_output, 'w') as header:
    header.write('= "%s"' % options.version_name)

if __name__ == '__main__':
  sys.exit(main())
