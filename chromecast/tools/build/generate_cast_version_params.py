#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import re
import sys
import time

from subprocess import call

BUILD_NUM_KEY = 'CAST_BUILD_INCREMENTAL'
CAST_PRODUCT_KEY = 'CAST_PRODUCT_TYPE'
IS_DEBUG_KEY = 'CAST_IS_DEBUG_BUILD'
RELEASE_NUM_KEY = 'CAST_BUILD_RELEASE'


def main(argv):
  parser = optparse.OptionParser('usage: %prog [options]')
  parser.add_option('-o', '--output-file', action='store', dest='output_file',
                    help='Output path for the key-value file.')
  parser.add_option('-d', '--debug', action='store_true', dest='is_debug',
                    default=False, help='Build is Cast debug mode.')
  parser.add_option('-p', '--product-type', action='store', dest='product_type',
                    help='The Cast product type.')
  parser.add_option('-r', '--release-path', action='store', dest='release_path',
                     default=None,
                     help='The path to a file with the Cast Release version.')

  (options, _) = parser.parse_args(argv)
  if not options.output_file:
    parser.error('Output file not provided.')
  if not options.product_type:
    parser.error('Product type not provided')

  params = {}

  # The Cast automated build system will set this parameter in the build
  # environment. If it has not been set, this build is likely being done
  # on a developer's machine. If so, set a dummy string based on the date.
  dummy_incremental = time.strftime('%Y%m%d.%H%M%S')
  params[BUILD_NUM_KEY] = os.environ.get(BUILD_NUM_KEY, dummy_incremental)

  # If this is an internal build, the Cast Release version will be stored in a
  # file. Read and validate the value in this file. If the file is not present,
  # this is likely a public build. If so, create a dummy release version.
  version = 'eng.' + os.environ.get('USER', '')
  if options.release_path:
    with open(options.release_path, 'r') as f:
      version = f.read().strip()
      if not re.compile('^[0-9]*\.[0-9]*$').match(version):
        sys.exit(
            'Cast version file is corrupt: {}'.format(options.release_path))

  params[RELEASE_NUM_KEY] = version

  # If -d has been passed, this is a Cast debug build.
  params[IS_DEBUG_KEY] = '1' if options.is_debug else '0'

  # Store the Cast Product Type.
  params[CAST_PRODUCT_KEY] = str(options.product_type)

  # Write the key-value pairs to file.
  with open(options.output_file, 'w') as f:
    for key, val in params.items():
      f.write('{}={}\n'.format(key,val))
  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv))
