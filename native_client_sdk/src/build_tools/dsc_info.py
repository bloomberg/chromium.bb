#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extracts information from a library.dsc file."""

import optparse
import os
import sys

import parse_dsc

def Error(msg):
  print >> sys.stderr, 'dsc_info: %s' % msg
  sys.exit(1)


def FindTarget(tree, target_name):
  targets = tree['TARGETS']
  for target in targets:
    if target['NAME'] == target_name:
      return target
  Error('Target %s not found' % target_name)


def GetSources(lib_dir, tree, target_name):
  result = []
  target = FindTarget(tree, target_name)
  for filename in target['SOURCES']:
    result.append('/'.join([lib_dir, filename]))
  return result


def DoMain(argv):
  "Entry point for gyp's pymod_do_main command."
  parser = optparse.OptionParser(usage='%prog: [OPTIONS] TARGET')
  # Give a clearer error message when this is used as a module.
  parser.prog = 'dsc_info'
  parser.add_option('-s', '--sources',
                    help='Print a list of source files for the target',
                    action='store_true', default=False)
  parser.add_option('-l', '--libdir',
                    help='Directory where the library.dsc file is located',
                    metavar='DIR')
  options, args = parser.parse_args(argv)
  if len(args) != 1:
    parser.error('Expecting exactly one argument.')
  target = args[0]
  libdir = options.libdir or ''
  tree = parse_dsc.LoadProject(os.path.join(libdir, 'library.dsc'))
  if options.sources:
    return '\n'.join(GetSources(libdir, tree, target))
  parser.error('No action specified')


def main(argv):
  print DoMain(argv[1:])


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv))
  except KeyboardInterrupt:
    Error('interrupted')
