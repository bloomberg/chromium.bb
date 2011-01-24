#!/usr/bin/python
# Copyright 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

"""This script finds Chrome revisions with prebuilt binaries for every platform.

This information is helpful when changing chrome_rev in DEPS.
"""

import optparse

import chromebinaries


def MakeCommandLineParser():
  parser = optparse.OptionParser()
  parser.add_option('--min', dest='min', default=None, type='int',
                    help='First revision to consider.')
  parser.add_option('--max', dest='max', default=None, type='int',
                    help='Last revision to consider.')
  return parser


def Main():
  parser = MakeCommandLineParser()
  options, args = parser.parse_args()

  if args:
    parser.error('ERROR: invalid argument')
  if options.min is not None and options.max is not None:
    if options.min > options.max:
      parser.error('Min is greater than max.')

  print "Getting snapshot listings"
  print
  revs = chromebinaries.GetCommonRevisions(options.min, options.max,
                                           verbose=True)

  print
  print "Revisions"
  print
  for rev in sorted(revs):
    print rev
  print
  print "Count", len(revs)


if __name__ == '__main__':
  Main()
