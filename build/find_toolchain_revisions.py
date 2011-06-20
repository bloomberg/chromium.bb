#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script finds Native Client Toolchain revisions for every flavor that the
download_toolchains.py needs.

This information is helpful when changing arm_toolchain_version and
x86_toolchain_version in DEPS.
"""

import httplib2
import optparse
import sys

import toolchainbinaries


TARBALL_VERSION_CHECK_MAX = 30


def UrlAvailable(url):
  response, data = httplib2.Http().request(url, 'HEAD')
  return response.status == 200


def TarballsAvailableForVersion(base, version):
  """Checks if toolchains are available for a certain version.

  Iterates over all known toolchain flavors and for each of them checks if it is
  available for download.  The results are grouped by type: 'x86' and 'arm'.

  Args:
      base: URL base to test for toolchain presence.
      version: SVN revision that the toolchains need to have been built from.

  Returns:
      A dict mapping toolchain type to the status string 'OK' for success and
      the availability details if an error occurred.  For example:

      {'x86': 'OK', 'arm': 'missing linux_arm-trusted'}
  """
  all_flavors = []
  for archs in toolchainbinaries.PLATFORM_MAPPING.itervalues():
    for flavors in archs.itervalues():
      all_flavors.extend(flavors)
  res = {'x86': 'OK', 'arm': 'OK'}
  for flavor in set(all_flavors):
    if toolchainbinaries.IsArmFlavor(flavor):
      toolchain_type = 'arm'
    else:
      toolchain_type = 'x86'
    if res[toolchain_type] != 'OK':
      continue
    if not UrlAvailable(
        toolchainbinaries.EncodeToolchainUrl(base, version, flavor)):
      res[toolchain_type] = 'missing %s' % flavor
  return res


def SearchAvailableToolchains(base_url, rev):
  print 'Searching for available toolchains...'
  for i in xrange(rev, rev - TARBALL_VERSION_CHECK_MAX, -1):
    sys.stdout.write('checking r%d: ' % i)
    sys.stdout.flush()
    avail = TarballsAvailableForVersion(base_url, i)
    sys.stdout.write('x86: %s, arm: %s\n' % (avail['x86'], avail['arm']))


def Main():
  parser = optparse.OptionParser()
  parser.add_option(
      '-s', '--search-toolchain-rev-start', dest='search_rev_start',
      default=None,
      help='start revision number for searching toolchains '
           'suitable for DEPS.  The revisions are searched backwards.')
  options, args = parser.parse_args()
  if args or options.search_rev_start is None:
    parser.error('ERROR: invalid argument')
  try:
    SearchAvailableToolchains(toolchainbinaries.BASE_DOWNLOAD_URL,
                              int(options.search_rev_start))
  except KeyboardInterrupt:
    print '\nSearch interrupted.'


if __name__ == '__main__':
  Main()
