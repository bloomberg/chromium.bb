#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to repack paks for a list of locales for iOS.

Gyp doesn't have any built-in looping capability, so this just provides a way to
loop over a list of locales when repacking pak files, thus avoiding a
proliferation of mostly duplicate, cut-n-paste gyp actions.
"""

import optparse
import os
import sys

script_dir = os.path.dirname(__file__)
src_dir = os.path.join(script_dir, os.pardir, os.pardir, os.pardir, os.pardir)
sys.path.append(os.path.join(src_dir, 'tools', 'grit'))

from grit.format import data_pack


def calc_output(options, locale):
  """Determine the file that will be generated for the given locale."""
  #e.g. '<(INTERMEDIATE_DIR)/repack/da.pak',
  # For Fake Bidi, generate it at a fixed path so that tests can safely
  # reference it.
  if locale == 'fake-bidi':
    return os.path.join(options.share_int_dir, locale + '.pak')
  # For Cocoa to find the locale at runtime, it needs to use '_' instead
  # of '-' (http://crbug.com/20441).  Also, 'en-US' should be represented
  # simply as 'en' (http://crbug.com/19165, http://crbug.com/25578).
  if locale == 'en-US':
    locale = 'en'
  else:
    locale = locale.replace('-', '_')
  return os.path.join(options.out_dir, locale + '.lproj', 'locale.pak')


def calc_inputs(options, locale):
  """Determine the files that need processing for the given locale."""
  inputs = []

  #e.g. 'out/Debug/gen/components/strings/components_strings_da.pak'
  inputs.append(os.path.join(options.share_int_dir, 'components', 'strings',
                'components_strings_%s.pak' % locale))

  #e.g. 'out/Debug/gen/components/strings/components_chromium_strings_da.pak'
  # or  'out/Debug/gen/components/strings/
  # components_google_chrome_strings_da.pak'
  inputs.append(os.path.join(options.share_int_dir, 'components', 'strings',
                'components_%s_strings_%s.pak' % (options.branding, locale)))

  #e.g. 'out/Debug/gen/ui/strings/ui_strings_da.pak'
  inputs.append(os.path.join(options.share_int_dir, 'ui', 'strings',
                'ui_strings_%s.pak' % locale))

  #e.g. 'out/Debug/gen/ui/strings/app_locale_settings_da.pak',
  inputs.append(os.path.join(options.share_int_dir, 'ui', 'strings',
                'app_locale_settings_%s.pak' % locale))

  #e.g. 'out/Debug/gen/ios/chrome/ios_locale_settings_da.pak'
  inputs.append(os.path.join(options.share_int_dir, 'ios', 'chrome',
                'ios_locale_settings_%s.pak' % locale))

  #e.g. 'out/Debug/gen/ios/chrome/ios_strings_da.pak'
  inputs.append(os.path.join(options.share_int_dir, 'ios', 'chrome',
                'ios_strings_%s.pak' % locale))

  #e.g. 'out/Debug/gen/ios/chrome/ios_chromium_strings_da.pak'
  # or  'out/Debug/gen/ios/chrome/ios_google_chrome_strings_da.pak'
  inputs.append(os.path.join(options.share_int_dir, 'ios', 'chrome',
                'ios_%s_strings_%s.pak' % (options.branding, locale)))

  # Add any extra input files.
  for extra_file in options.extra_input:
    inputs.append('%s_%s.pak' % (extra_file, locale))

  return inputs


def list_outputs(options, locales):
  """Returns the names of files that will be generated for the given locales.

  This is to provide gyp the list of output files, so build targets can
  properly track what needs to be built.
  """
  outputs = []
  for locale in locales:
    outputs.append(calc_output(options, locale))
  return outputs


def list_inputs(options, locales):
  """Returns the names of files that will be processed for the given locales.

  This is to provide gyp the list of input files, so build targets can properly
  track their prerequisites.
  """
  inputs = []
  for locale in locales:
    inputs.extend(calc_inputs(options, locale))
  return inputs


def quote_filenames(filenames):
  """Quote each elements so filename spaces don't mess up gyp's attempt to parse
  it into a list."""
  return " ".join(['"%s"' % x for x in filenames])


def repack_locales(options, locales):
  """ Loop over and repack the given locales."""
  for locale in locales:
    inputs = calc_inputs(options, locale)
    output = calc_output(options, locale)
    data_pack.DataPack.RePack(output, inputs, whitelist_file=options.whitelist)


def DoMain(argv):
  parser = optparse.OptionParser("usage: %prog [options] locales")
  parser.add_option(
      "-i", action="store_true", dest="print_inputs", default=False,
      help="Print the expected input file list, then exit.")
  parser.add_option(
      "-o", action="store_true", dest="print_outputs", default=False,
      help="Print the expected output file list, then exit.")
  parser.add_option(
      "-x", action="store", dest="out_dir",
      help="Intermediate build files output directory.")
  parser.add_option(
      "-s", action="store", dest="share_int_dir",
      help="Shared intermediate build files output directory.")
  parser.add_option(
      "-b", action="store", dest="branding",
      help="Branding type of this build.")
  parser.add_option(
      "-e", action="append", dest="extra_input", default=[],
      help="Full path to an extra input pak file without the "
           "locale suffix and \".pak\" extension.")
  parser.add_option(
      "--whitelist", action="store", help="Full path to the "
      "whitelist used to filter output pak file resource IDs")
  options, locales = parser.parse_args(argv)

  if not locales:
    parser.error('Please specificy at least one locale to process.\n')

  if not (options.out_dir and options.share_int_dir):
    parser.error('Please specify all of "-x" and "-s".\n')
  if options.print_inputs and options.print_outputs:
    parser.error('Please specify only one of "-i" or "-o".\n')
  # Need to know the branding, unless we're just listing the outputs.
  if not options.print_outputs and not options.branding:
    parser.error('Please specify "-b" to determine the input files.\n')

  if options.print_inputs:
    return quote_filenames(list_inputs(options, locales))

  if options.print_outputs:
    return quote_filenames(list_outputs(options, locales))

  return repack_locales(options, locales)

if __name__ == '__main__':
  results = DoMain(sys.argv[1:])
  if results:
    print results
