#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs 'ld -shared' and generates a .TOC file that's untouched when unchanged.

This script exists to avoid using complex shell commands in
gcc_toolchain.gni's tool("solink"), in case the host running the compiler
does not have a POSIX-like shell (e.g. Windows).
"""

import argparse
import os
import subprocess
import sys

import wrapper_utils


def CollectSONAME(args):
  """Replaces: readelf -d $sofile | grep SONAME"""
  toc = ''
  readelf = subprocess.Popen(wrapper_utils.CommandToRun(
      [args.readelf, '-d', args.sofile]), stdout=subprocess.PIPE, bufsize=-1)
  for line in readelf.stdout:
    if 'SONAME' in line:
      toc += line
  return readelf.wait(), toc


def CollectDynSym(args):
  """Replaces: nm --format=posix -g -D $sofile | cut -f1-2 -d' '"""
  toc = ''
  nm = subprocess.Popen(wrapper_utils.CommandToRun([
      args.nm, '--format=posix', '-g', '-D', args.sofile]),
                        stdout=subprocess.PIPE, bufsize=-1)
  for line in nm.stdout:
    toc += ' '.join(line.split(' ', 2)[:2]) + '\n'
  return nm.wait(), toc


def CollectTOC(args):
  result, toc = CollectSONAME(args)
  if result == 0:
    result, dynsym = CollectDynSym(args)
    toc += dynsym
  return result, toc


def UpdateTOC(tocfile, toc):
  if os.path.exists(tocfile):
    old_toc = open(tocfile, 'r').read()
  else:
    old_toc = None
  if toc != old_toc:
    open(tocfile, 'w').write(toc)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--readelf',
                      required=True,
                      help='The readelf binary to run',
                      metavar='PATH')
  parser.add_argument('--nm',
                      required=True,
                      help='The nm binary to run',
                      metavar='PATH')
  parser.add_argument('--strip',
                      help='The strip binary to run',
                      metavar='PATH')
  parser.add_argument('--sofile',
                      required=True,
                      help='Shared object file produced by linking command',
                      metavar='FILE')
  parser.add_argument('--tocfile',
                      required=True,
                      help='Output table-of-contents file',
                      metavar='FILE')
  parser.add_argument('--map-file',
                      help=('Use --Wl,-Map to generate a map file. Will be '
                            'gzipped if extension ends with .gz'),
                      metavar='FILE')
  parser.add_argument('--output',
                      required=True,
                      help='Final output shared object file',
                      metavar='FILE')
  parser.add_argument('command', nargs='+',
                      help='Linking command')
  args = parser.parse_args()

  # Work-around for gold being slow-by-default. http://crbug.com/632230
  fast_env = dict(os.environ)
  fast_env['LC_ALL'] = 'C'

  # Extract the --link-only argument, which goes for a ride through ldflags into
  # the command, but is meant to be intercepted by this wrapper script (not
  # passed to the linker). https://crbug.com/954311 tracks finding a better way
  # to plumb this argument.
  link_only = '--link-only' in args.command
  if link_only:
    args.command.remove('--link-only')

  # First, run the actual link.
  command = wrapper_utils.CommandToRun(args.command)
  result = wrapper_utils.RunLinkWithOptionalMapFile(command, env=fast_env,
                                                    map_file=args.map_file)

  if result != 0:
    return result

  # If only linking, we are likely generating a partitioned .so that will be
  # split apart later. In that case:
  #
  # - The TOC file optimization isn't useful, because the partition libraries
  #   must always be re-extracted if the combined library changes (and nothing
  #   should be depending on the combined library's dynamic symbol table).
  # - Stripping isn't necessary, because the combined library is not used in
  #   production or published.
  #
  # Both of these operations could still be done, they're needless work, and
  # tools would need to be updated to handle and/or not complain about
  # partitioned libraries. Instead, to keep Ninja happy, simply create dummy
  # files for the TOC and stripped lib.
  if link_only:
    with open(args.output, 'w'):
      pass
    with open(args.tocfile, 'w'):
      pass
    return 0

  # Next, generate the contents of the TOC file.
  result, toc = CollectTOC(args)
  if result != 0:
    return result

  # If there is an existing TOC file with identical contents, leave it alone.
  # Otherwise, write out the TOC file.
  UpdateTOC(args.tocfile, toc)

  # Finally, strip the linked shared object file (if desired).
  if args.strip:
    result = subprocess.call(wrapper_utils.CommandToRun(
        [args.strip, '-o', args.output, args.sofile]))

  return result


if __name__ == "__main__":
  sys.exit(main())
