#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Copy xml resource files and add -v17 to the sub directory names.

This is coupled with generate_v14_resources.py. Please refer to
generate_v14_resources.py's comment for why we are doing this.
Or http://crbug.com/235118 .
"""

import optparse
import os
import shutil
import sys
import xml.dom.minidom as minidom

from util import build_utils


def CopyXmlResourcesInDir(input_dir, output_dir, only_styles=False):
  """Copy all XML resources from input_dir to output_dir."""
  for input_file in build_utils.FindInDirectory(input_dir, '*.xml'):
    if only_styles:
      # If the xml file does not have a style element,
      # it's not style resource, so skip.
      dom = minidom.parse(input_file)
      if not dom.getElementsByTagName('style'):
        continue

    output_path = os.path.join(output_dir,
                               os.path.relpath(input_file, input_dir))
    build_utils.MakeDirectory(os.path.dirname(output_path))
    shutil.copy2(input_file, output_path)


def ParseArgs():
  """Parses command line options.

  Returns:
    An options object as from optparse.OptionsParser.parse_args()
  """
  parser = optparse.OptionParser()
  parser.add_option('--res-dir',
                    help='directory containing resources to be copied')
  parser.add_option('--res-v17-dir',
                    help='output directory to which resources will be copied.')
  parser.add_option('--stamp', help='File to touch on success')

  options, args = parser.parse_args()

  if args:
    parser.error('No positional arguments should be given.')

  # Check that required options have been provided.
  required_options = ('res_dir', 'res_v17_dir')
  build_utils.CheckOptions(options, parser, required=required_options)
  return options


def main(argv):
  options = ParseArgs()

  build_utils.DeleteDirectory(options.res_v17_dir)
  build_utils.MakeDirectory(options.res_v17_dir)

  for name in os.listdir(options.res_dir):
    if not os.path.isdir(os.path.join(options.res_dir, name)):
      continue

    dir_pieces = name.split('-')
    resource_type = dir_pieces[0]
    qualifiers = dir_pieces[1:]

    # Skip RTL resources because they are not supported by API 14.
    if 'ldrtl' in qualifiers:
      continue

    input_dir = os.path.join(options.res_dir, name)
    output_dir = os.path.join(options.res_v17_dir, name + '-v17')

    # We only copy resources under layout*/, xml*/,
    # and style resources under values*/.
    # TODO(kkimlabs): don't process xml directly once all layouts have
    # been moved out of XML directory. see http://crbug.com/238458
    if resource_type in ('layout', 'xml'):
      CopyXmlResourcesInDir(input_dir, output_dir)
    elif resource_type in ('values'):
      CopyXmlResourcesInDir(input_dir, output_dir, only_styles=True)

  if options.stamp:
    build_utils.Touch(options.stamp)

if __name__ == '__main__':
  sys.exit(main(sys.argv))

