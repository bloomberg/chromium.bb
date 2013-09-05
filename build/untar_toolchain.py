#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cygtar
import optparse
import os
import oshelpers
import subprocess
import sys

def Main(args):
  parser = optparse.OptionParser()
  # Modes
  parser.add_option('-s', '--sdk', help='SDK directory.',
      dest='sdk')
  parser.add_option('-t', '--tool', help='Which toolchain.',
      dest='tool')
  parser.add_option('-o', '--os', help='Untar for which OS.',
      dest='os')
  parser.add_option('-T', '--tmp', help='Temp directory.',
      dest='tmp')
  parser.add_option('-v', '--verbose', dest='verbose', default=False,
      help='Enable verbosity', action='store_true')

  options, args = parser.parse_args(args[1:])
  if not options.sdk:
    parser.error('Expecting SDK directory.')
  if not options.os:
    parser.error('Expecting OS to be specified.')
  if not options.tool:
    parser.error('Expecting which tool to untar.')
  if len(args) < 1:
    parser.error('Expecting path(s) to tarball(s).')

  untar_path = os.path.join(options.tmp, options.tool)
  if options.tool == 'x86_newlib':
    tool_path = os.path.join(untar_path, 'sdk', 'nacl-sdk')
  elif options.tool == 'x86_glibc':
    tool_path = os.path.join(untar_path, 'toolchain', options.os + '_x86')
  elif options.tool in ('pnacl', 'arm_newlib'):
    tool_path = untar_path
  else:
    parser.error('Unknown tool type: ' + options.tool)

  tool_name = '%s_%s' % (options.os, options.tool)
  final_path = os.path.join(options.sdk, 'toolchain', tool_name)
  stamp_path = os.path.join(final_path, 'stamp.untar')

  final_path = os.path.abspath(final_path)
  untar_path = os.path.abspath(untar_path)
  stamp_path = os.path.abspath(stamp_path)
  tool_path = os.path.abspath(tool_path)

  if options.verbose:
   print 'Delete: ' + untar_path
  oshelpers.Remove(['-fr', untar_path])

  if options.verbose:
    print 'Mkdir: ' + untar_path
  oshelpers.Mkdir(['-p', untar_path])

  if options.verbose:
    print 'Delete: ' + final_path
  oshelpers.Remove(['-fr', final_path])

  if options.verbose:
    print 'Mkdir: ' + os.path.join(options.sdk, 'toolchain')
  oshelpers.Mkdir(['-p', os.path.join(options.sdk, 'toolchain')])

  args = [os.path.abspath(a) for a in args]

  old_path = os.getcwd()
  os.chdir(untar_path)

  for arg in args:
    if options.verbose:
      print 'Open: ' + arg
    tar = cygtar.CygTar(arg, 'r', verbose=options.verbose)

    if options.verbose:
      print 'Extract'
    tar.Extract()

  os.chdir(old_path)
  if options.verbose:
    print 'Move: %s to %s' % (tool_path, final_path)
  oshelpers.Move([tool_path, final_path])

  if options.verbose:
    print 'Stamp: ' + stamp_path
  fh = open(stamp_path, 'w')
  fh.write(args[0] + '\n')
  fh.close()
  if options.verbose:
    print 'Done.'
  return 0


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
