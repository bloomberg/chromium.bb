#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Applies an issue from Rietveld.
"""

import logging
import optparse
import os
import sys
import urllib2

import breakpad  # pylint: disable=W0611

import checkout
import fix_encoding
import rietveld
import scm


def main():
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-v', '--verbose', action='count', default=0,
      help='Prints debugging infos')
  parser.add_option(
      '-e',
      '--email',
      help='IGNORED: Kept for compatibility.')
  parser.add_option(
      '-i', '--issue', type='int', help='Rietveld issue number')
  parser.add_option(
      '-p', '--patchset', type='int', help='Rietveld issue\'s patchset number')
  parser.add_option(
      '-r',
      '--root_dir',
      default=os.getcwd(),
      help='Root directory to apply the patch')
  parser.add_option(
      '-s',
      '--server',
      default='http://codereview.chromium.org',
      help='Rietveld server')
  options, args = parser.parse_args()
  logging.basicConfig(
      format='%(levelname)5s %(module)11s(%(lineno)4d): %(message)s',
      level=[logging.WARNING, logging.INFO, logging.DEBUG][
          min(2, options.verbose)])
  if args:
    parser.error('Extra argument(s) "%s" not understood' % ' '.join(args))
  if not options.issue:
    parser.error('Require --issue')
  options.server = options.server.rstrip('/')
  if not options.server:
    parser.error('Require a valid server')

  obj = rietveld.Rietveld(options.server, '', None)
  try:
    properties = obj.get_issue_properties(options.issue, False)
  except rietveld.upload.ClientLoginError:
    # Requires login.
    obj = rietveld.Rietveld(options.server, None, None)
    properties = obj.get_issue_properties(options.issue, False)

  if not options.patchset:
    options.patchset = properties['patchsets'][-1]
    print('No patchset specified. Using patchset %d' % options.patchset)

  print('Downloading the patch.')
  try:
    patchset = obj.get_patch(options.issue, options.patchset)
  except urllib2.HTTPError, e:
    print >> sys.stderr, (
        'Failed to fetch the patch for issue %d, patchset %d.\n'
        'Try visiting %s/%d') % (
            options.issue, options.patchset,
            options.server, options.issue)
    return 1
  for patch in patchset.patches:
    print(patch)
  scm_type = scm.determine_scm(options.root_dir)
  if scm_type == 'svn':
    scm_obj = checkout.SvnCheckout(options.root_dir, None, None, None, None)
  elif scm_type == 'git':
    scm_obj = checkout.GitCheckoutBase(options.root_dir, None, None)
  elif scm_type == None:
    scm_obj = checkout.RawCheckout(options.root_dir, None)
  else:
    parser.error('Couldn\'t determine the scm')

  # Apply the patch.
  try:
    scm_obj.apply_patch(patchset)
  except checkout.PatchApplicationFailed, e:
    print >> sys.stderr, str(e)
    return 1
  return 0


if __name__ == "__main__":
  fix_encoding.fix_encoding()
  sys.exit(main())
