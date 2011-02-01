#!/usr/bin/python

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Stop gap sync function until cbuildbot is integrated into all builders"""

import cbuildbot_comm
import cbuildbot
import optparse
import sys

"""Number of retries to retry repo sync before giving up"""
_NUMBER_OF_RETRIES = 3

def main():
  parser = optparse.OptionParser()
  parser.add_option('-r', '--buildroot',
                    help='root directory where sync occurs')
  parser.add_option('-c', '--clobber', action='store_true', default=False,
                    help='clobber build directory and do a full checkout')
  parser.add_option('-t', '--tracking_branch', default='cros/master',
                    help='Branch to sync against for full checkouts.')
  (options, args) = parser.parse_args()
  if options.buildroot:
    if options.clobber:
      cbuildbot._FullCheckout(options.buildroot, options.tracking_branch,
                              retries=_NUMBER_OF_RETRIES)
    else:
      cbuildbot._IncrementalCheckout(options.buildroot,
                                     retries=_NUMBER_OF_RETRIES)
  else:
    print >> sys.stderr, 'ERROR:  Must set buildroot'
    sys.exit(1)

if __name__ == '__main__':
    main()
