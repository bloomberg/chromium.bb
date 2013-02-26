#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script finds Native Client Toolchain revisions for every flavor that the
download_toolchains.py needs.

This information is helpful when changing arm_toolchain_version and
x86_toolchain_version in DEPS.
"""

import optparse
import sys
import urllib2

import toolchainbinaries


TARBALL_VERSION_CHECK_MAX = 30


def UrlAvailable(url):
  try:
    request = urllib2.Request(url)
    request.get_method = lambda : 'HEAD'
    response = urllib2.urlopen(request)
    response.info()
  except urllib2.URLError:
    return False
  return True


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

      {'x86': 'OK', 'pnacl': 'missing linux_arm-trusted'}
  """
  all_flavors = []
  for archs in toolchainbinaries.PLATFORM_MAPPING.itervalues():
    for flavors in archs.itervalues():
      all_flavors.extend(flavors)
  res = {'x86': 'OK', 'pnacl': 'OK'}
  for flavor in set(all_flavors):
    if type(flavor) is tuple:
      # This script doesn't handle multi-package toolchains.
      continue
    # The linux_arm-trusted set is going away and new ones don't matter now.
    if toolchainbinaries.IsArmTrustedFlavor(flavor):
      continue
    elif toolchainbinaries.IsPnaclFlavor(flavor):
      toolchain_type = 'pnacl'
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
    print ', '.join(key + ': ' + value for key, value in avail.iteritems())


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
