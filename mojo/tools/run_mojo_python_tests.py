#!/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys
import unittest


# TODO(dpranke): crbug.com/364709 . We should add the ability to return
# JSONified results data for the bots.


def main():
  parser = optparse.OptionParser()
  parser.add_option('-v', '--verbose', action='count', default=0)
  options, args = parser.parse_args()
  if args:
    parser.usage()
    return 1

  chromium_src_dir = os.path.join(os.path.dirname(__file__),
                                  os.pardir,
                                  os.pardir)

  loader = unittest.loader.TestLoader()
  print "Running Python unit tests under mojo/public/tools/bindings/pylib ..."
  suite = loader.discover(os.path.join(chromium_src_dir, 'mojo', 'public',
                                      'tools', 'bindings', 'pylib'),
                          pattern='*_unittest.py')

  runner = unittest.runner.TextTestRunner(verbosity=(options.verbose+1))
  result = runner.run(suite)
  return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
  sys.exit(main())
