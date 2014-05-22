#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates the contents of an Cronet LICENSE file for the third-party code.

It makes use of src/tools/licenses.py and the README.chromium files on which
it depends. Based on android_webview/tools/webview_licenses.py.
"""

import optparse
import os
import sys
import textwrap

REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools'))
import licenses

def _ReadFile(path):
  """Reads a file from disk.
  Args:
    path: The path of the file to read, relative to the root of the repository.
  Returns:
    The contents of the file as a string.
  """
  return open(os.path.join(REPOSITORY_ROOT, path), 'rb').read()


def GenerateLicense():
  """Generates the contents of an Cronet LICENSE file for the third-party code.
  Returns:
    The contents of the LICENSE file.
  """
  # TODO(mef): Generate list of third_party libraries using checkdeps.
  third_party_dirs = [
    "libevent",
    "ashmem",
    "zlib",
    "modp_b64",
    "openssl"
  ]

  # Start with Chromium's LICENSE file
  content = [_ReadFile('LICENSE')]

  # Add necessary third_party.
  for directory in sorted(third_party_dirs):
    metadata = licenses.ParseDir("third_party/" + directory, REPOSITORY_ROOT,
                                 require_license_file=True)
    content.append("-" * 20)
    content.append(directory)
    content.append("-" * 20)
    license_file = metadata['License File']
    if license_file and license_file != licenses.NOT_SHIPPED:
      content.append(_ReadFile(license_file))

  return '\n'.join(content)


def main():
  class FormatterWithNewLines(optparse.IndentedHelpFormatter):
    def format_description(self, description):
      paras = description.split('\n')
      formatted_paras = [textwrap.fill(para, self.width) for para in paras]
      return '\n'.join(formatted_paras) + '\n'

  parser = optparse.OptionParser(formatter=FormatterWithNewLines(),
                                 usage='%prog command [options]')
  parser.description = (__doc__ +
                       '\nCommands:\n' \
                       '  license [filename]\n' \
                       '    Generate Cronet LICENSE to filename or stdout.\n')
  (_, args) = parser.parse_args()
  if not args:
    parser.print_help()
    return 1

  if args[0] == 'license':
    if len(args) > 1:
      print 'Saving license to %s' % args[1]
      f = open(args[1], "w")
      try:
        f.write(GenerateLicense())
      finally:
        f.close()
    else:
      print GenerateLicense()
    return 0

  parser.print_help()
  return 1


if __name__ == '__main__':
  sys.exit(main())
