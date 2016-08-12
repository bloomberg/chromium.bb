#!/usr/bin/env python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to rename paks for a list of locales for the Android WebView.

Gyp doesn't have any built-in looping capability, so this just provides a way to
loop over a list of locales when renaming pak files. Based on
chrome/tools/build/repack_locales.py
"""

import optparse
import os
import shutil
import sys

# Prepend the grit module from the source tree so it takes precedence over other
# grit versions that might present in the search path.
sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..', '..',
                                'tools', 'grit'))
from grit.format import data_pack

def calc_output(locale):
  """Determine the file that will be generated for the given locale."""
  return os.path.join(PRODUCT_DIR, 'android_webview_assets',
                      'locales', locale + '.pak')

def calc_inputs(locale):
  """Determine the files that need processing for the given locale."""
  inputs = []

  inputs.append(os.path.join(SHARE_INT_DIR, 'content', 'app', 'strings',
                             'content_strings_%s.pak' % locale))
  inputs.append(os.path.join(SHARE_INT_DIR, 'android_webview',
                             'aw_strings_%s.pak' % locale))
  inputs.append(os.path.join(SHARE_INT_DIR, 'android_webview',
                             'components_strings_%s.pak' % locale))
  return inputs

def list_outputs(locales):
  """Returns the names of files that will be generated for the given locales.

  This is to provide gyp the list of output files, so build targets can
  properly track what needs to be built.
  """
  outputs = []
  for locale in locales:
    outputs.append(calc_output(locale))
  # Quote each element so filename spaces don't mess up gyp's attempt to parse
  # it into a list.
  return " ".join(['"%s"' % x for x in outputs])

def list_inputs(locales):
  """Returns the names of files that will be processed for the given locales.

  This is to provide gyp the list of input files, so build targets can properly
  track their prerequisites.
  """
  inputs = []
  for locale in locales:
    inputs += calc_inputs(locale)
  # Quote each element so filename spaces don't mess up gyp's attempt to parse
  # it into a list.
  return " ".join(['"%s"' % x for x in inputs])

def repack_locales(locales):
  """ Loop over and repack the given locales."""
  for locale in locales:
    inputs = []
    inputs += calc_inputs(locale)
    output = calc_output(locale)
    data_pack.DataPack.RePack(output, inputs)

def DoMain(argv):
  global SHARE_INT_DIR
  global PRODUCT_DIR

  parser = optparse.OptionParser("usage: %prog [options] locales")
  parser.add_option("-i", action="store_true", dest="inputs", default=False,
                    help="Print the expected input file list, then exit.")
  parser.add_option("-o", action="store_true", dest="outputs", default=False,
                    help="Print the expected output file list, then exit.")
  parser.add_option("-p", action="store", dest="product_dir",
                    help="Product build files output directory.")
  parser.add_option("-s", action="store", dest="share_int_dir",
                    help="Shared intermediate build files output directory.")
  options, locales = parser.parse_args(argv)
  if not locales:
    parser.error('Please specify at least one locale to process.\n')

  print_inputs = options.inputs
  print_outputs = options.outputs
  SHARE_INT_DIR = options.share_int_dir
  PRODUCT_DIR = options.product_dir

  if print_inputs and print_outputs:
    parser.error('Please specify only one of "-i" or "-o".\n')

  if print_inputs:
    return list_inputs(locales)

  if print_outputs:
    return list_outputs(locales)

  return repack_locales(locales)

if __name__ == '__main__':
  results = DoMain(sys.argv[1:])
  if results:
    print results
