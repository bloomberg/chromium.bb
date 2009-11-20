#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to repack paks for a list of locales.

Gyp doesn't have any built-in looping capability, so this just provides a way to
loop over a list of locales when repacking pak files, thus avoiding a
proliferation of mostly duplicate, cut-n-paste gyp actions.
"""

import getopt
import os
import sys

sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..', '..', '..',
                             'tools', 'data_pack'))
import repack

# The gyp "branding" variable.
BRANDING = None

# Some build paths defined by gyp.
GRIT_DIR = None
SHARE_INT_DIR = None
INT_DIR = None


class Usage(Exception):
  def __init__(self, msg):
    self.msg = msg


def calc_output(locale):
  """Determine the file that will be generated for the given locale."""
  #e.g. '<(INTERMEDIATE_DIR)/repack/da.pak',
  if sys.platform in ('darwin',):
    # For Cocoa to find the locale at runtime, it needs to use '_' instead
    # of '-' (http://crbug.com/20441).  Also, 'en-US' should be represented
    # simply as 'en' (http://crbug.com/19165, http://crbug.com/25578).
    if locale == 'en-US':
      locale = 'en'
    return '%s/repack/%s.lproj/locale.pak' % (INT_DIR, locale.replace('-', '_'))
  else:
    return '%s/repack/%s.pak' % (INT_DIR, locale)


def calc_inputs(locale):
  """Determine the files that need processing for the given locale."""
  inputs = []

  #e.g. '<(grit_out_dir)/generated_resources_da.pak'
  inputs.append('%s/generated_resources_%s.pak' % (GRIT_DIR, locale))

  #e.g. '<(grit_out_dir)/locale_settings_da.pak'
  inputs.append('%s/locale_settings_%s.pak' % (GRIT_DIR, locale))

  #e.g. '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_da.pak'
  inputs.append('%s/webkit/webkit_strings_%s.pak' % (SHARE_INT_DIR, locale))

  #e.g. '<(SHARED_INTERMEDIATE_DIR)/app/app_strings_da.pak',
  inputs.append('%s/app/app_strings/app_strings_%s.pak' % (
      SHARE_INT_DIR, locale))

  #e.g. '<(grit_out_dir)/google_chrome_strings_da.pak'
  #     or
  #     '<(grit_out_dir)/chromium_strings_da.pak'
  inputs.append('%s/%s_strings_%s.pak' % (GRIT_DIR, BRANDING, locale))

  return inputs


def list_outputs(locales):
  """Print the names of files that will be generated for the given locales.

  This is to provide gyp the list of output files, so build targets can
  properly track what needs to be built.
  """
  outputs = []
  for locale in locales:
    outputs.append(calc_output(locale))
  # Quote each element so filename spaces don't mess up gyp's attempt to parse
  # it into a list.
  print " ".join(['"%s"' % x for x in outputs])


def list_inputs(locales):
  """Print the names of files that will be processed for the given locales.

  This is to provide gyp the list of input files, so build targets can properly
  track their prerequisites.
  """
  inputs = []
  for locale in locales:
    inputs += calc_inputs(locale)
  # Quote each element so filename spaces don't mess up gyp's attempt to parse
  # it into a list.
  print " ".join(['"%s"' % x for x in inputs])


def repack_locales(locales):
  """ Loop over and repack the given locales."""
  for locale in locales:
    inputs = []
    inputs += calc_inputs(locale)
    output = calc_output(locale)
    print 'Repacking %s -> %s' % (inputs, output)
    repack.RePack(output, inputs)


def main(argv=None):
  global BRANDING
  global GRIT_DIR
  global SHARE_INT_DIR
  global INT_DIR

  if argv is None:
    argv = sys.argv

  short_options = 'iog:s:x:b:h'
  long_options = 'help'

  print_inputs = False
  print_outputs = False
  usage_msg = ''

  helpstr = """\
Usage:  %s [-h] [-i | -o] -g <DIR> -x <DIR> -s <DIR> -b <branding> <locale> [...]
  -h, --help     Print this help, then exit.
  -i             Print the expected input file list, then exit.
  -o             Print the expected output file list, then exit.
  -g DIR         GRIT build files output directory.
  -x DIR         Intermediate build files output directory.
  -s DIR         Shared intermediate build files output directory.
  -b branding    Branding type of this build.
  locale [...]   One or more locales to repack.""" % (argv[0])

  try:
    try:
      opts, locales = getopt.getopt(argv[1:], short_options, long_options)
    except getopt.GetoptError, msg:
      raise Usage(str(msg))

    if not locales:
      usage_msg = 'Please specificy at least one locale to process.\n'

    for o, a in opts:
      if o in ('-i'):
        print_inputs = True
      elif o in ('-o'):
        print_outputs = True
      elif o in ('-g'):
        GRIT_DIR = a
      elif o in ('-s'):
        SHARE_INT_DIR = a
      elif o in ('-x'):
        INT_DIR = a
      elif o in ('-b'):
        BRANDING = a
      elif o in ('-h', '--help'):
        print helpstr
        return 0

    if not (GRIT_DIR and INT_DIR and SHARE_INT_DIR):
      usage_msg += 'Please specify all of "-g" and "-x" and "-s".\n'
    if print_inputs and print_outputs:
      usage_msg += 'Please specify only one of "-i" or "-o".\n'
    # Need to know the branding, unless we're just listing the outputs.
    if not print_outputs and not BRANDING:
      usage_msg += 'Please specify "-b" to determine the input files.\n'

    if usage_msg:
      raise Usage(usage_msg)
  except Usage, err:
    sys.stderr.write(err.msg + '\n')
    sys.stderr.write(helpstr + '\n')
    return 2

  if print_inputs:
    list_inputs(locales)
    return 0

  if print_outputs:
    list_outputs(locales)
    return 0

  repack_locales(locales)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
