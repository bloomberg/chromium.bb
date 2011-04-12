#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Applies an issue from Rietveld.
"""

import logging
import optparse
import sys

import breakpad  # pylint: disable=W0611
import fix_encoding
import rietveld


def main():
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-v', '--verbose', action='count', help='Prints debugging infos')
  parser.add_option(
      '-i', '--issue', type='int', help='Rietveld issue number')
  parser.add_option(
      '-p', '--patchset', type='int', help='Rietveld issue\'s patchset number')
  parser.add_option(
      '-r',
      '--root_dir',
      action='store',
      help='Root directory to apply the patch')
  parser.add_option(
      '-s',
      '--server',
      action='store',
      default='http://codereview.chromium.org',
      help='Rietveld server')
  options, args = parser.parse_args()
  LOG_FORMAT = '%(levelname)s %(filename)s(%(lineno)d): %(message)s'
  if not options.verbose:
    logging.basicConfig(level=logging.WARNING, format=LOG_FORMAT)
  elif options.verbose == 1:
    logging.basicConfig(level=logging.INFO, format=LOG_FORMAT)
  elif options.verbose > 1:
    logging.basicConfig(level=logging.DEBUG, format=LOG_FORMAT)
  if args:
    parser.error('Extra argument(s) "%s" not understood' % ' '.join(args))
  if not options.issue:
    parser.error('Require --issue')

  obj = rietveld.Rietveld(options.server, None, None)

  if not options.patchset:
    options.patchset = obj.get_issue_properties(
        options.issue, False)['patchsets'][-1]
    logging.info('Using patchset %d' % options.patchset)
  obj.get_patch(options.issue, options.patchset)
  return 0


if __name__ == "__main__":
  fix_encoding.fix_encoding()
  sys.exit(main())
