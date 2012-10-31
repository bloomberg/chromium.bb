#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility script to wait for PDB writes to complete."""

import optparse
import os
import sys
import time


def _ParseOptions(args):
  print 'Args: ' + ' '.join(args)
  option_parser = optparse.OptionParser()
  option_parser.add_option('--input', action='append', default=[],
      dest='inputs', help='Add the file to the list of input dependencies.')
  option_parser.add_option('--pdb', action='store', default=None,
      help='The PDB file to lock and compare time against.')
  option_parser.add_option('--stamp',
      help='The stamp file to generate when the PDB is ready.')
  option_parser.add_option('--verbose', action='store_true', default=True,
      help='Provide verbose output.')
  option_parser.add_option('--epsilon', default=0.05,
      help='Seconds (0.05) to wait.')
  option_parser.add_option('--timeout', default=10.0,
      help='Seconds (10.0) to wait before timing out with a failure.')
  options, args = option_parser.parse_args(args)

  if not len(options.inputs):
    option_parser.error('You must provide at least one input binary.')
  if not options.pdb:
    option_parser.error('You must provide the PDB to verify.')
  if not options.stamp:
    option_parser.error('You must provide the stamp file to create.')

  if len(args):
    option_parser.error('Did not expect other inputs.')

  return options


def _TestPDBAccess(pdb):
  """Attemps to open provided PDB filepath for writing

  This function will attempt to find and write to the provided PDB filepath
  as a way to determine if the build system is no longer holding the file.
  A succesful open of a zero size file is considered an accidental
  create, so it also fails."""

  try:
    fd = os.open(pdb, os.O_APPEND + os.O_RDWR)
  except OSError:
    return -1

  # Return the size since 0 implies something is wrong.
  passed = os.fstat(fd).st_size
  os.close(fd)
  return passed


def main(args):
  options = _ParseOptions(args[1:])

  # While the PDB is expected to have been opened by mspdbsrv prior to the build
  # process for inputs successfully exiting, we wait for epsilon since have
  # no documentaion, and no wait to verify this.
  time.sleep(options.epsilon)

  start = time.clock()
  current = start

  # Verify each input exists, which should always be True
  for input in options.inputs:
    if not os.path.isfile(input):
      print 'WaitForPDB: Failed to find file %s.' % input
      return 1

  # Continue to attempt open the PDB for writing until we succeed or timeout.
  while (current - options.timeout) <= start:
    time.sleep(options.epsilon)
    current = time.clock()

    pdb_size = _TestPDBAccess(options.pdb)
    if pdb_size == 0:
      'WaitForPDB: Opened %s size zero.' % options.pdb
      return 1

    # If we were able to open for writing, and the size is non-zero, assume
    # mspdbsrv is done with the file.
    if pdb_size > 0:
      try:
        open(options.stamp, 'w').write('Stamp for: %s\n' % options.pdb)
        return 0
      except:
        print 'Failed to write stamp file %s.' % options.stamp
        return 1

    if options.verbose:
      print 'Waiting for %s.' % options.pdb

  print 'WaitForPDB: Time expired waiting for %s.' % options.pdb
  return 1

if '__main__' == __name__:
  sys.exit(main(sys.argv))
