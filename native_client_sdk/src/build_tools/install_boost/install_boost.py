#! -*- python -*-
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install boost headers into a list of toolchains, in such a way that it gets
pulled into the SDK installer.  Note that this script only installs boost
headers, and does not build any of the boost libraries that require building.
"""

import build_utils
import os
import shutil
import sys
import tarfile
import tempfile
import urllib

from optparse import OptionParser

# The original boost distro can be found here:
# http://sourceforge.net/projects/boost/files/boost/1.47.0/\
#     boost_1_47_0.tar.gz/download
BOOST_URL = ('http://commondatastorage.googleapis.com/nativeclient-mirror'
             '/nacl/boost_1_47_0.tar.gz')
BOOST_PATH = 'boost_1_47_0'


def DownloadAndExtract(working_dir, url, path):
  boost_path = os.path.abspath(os.path.join(working_dir, path))
  print 'Download: %s' % url
  try:
    (tgz_file, headers) = urllib.urlretrieve(url, '%s.tgz' % boost_path)
    tar = None
    try:
      tar = tarfile.open(tgz_file)
      tar.extractall(working_dir)
    finally:
      if tar:
        tar.close()
  except (URLError, ContentTooShortError):
    print 'Error retrieving %s' % url
    raise


# Install the boost headers into the toolchains.
def InstallBoost(options):
  # Create a temporary working directory.  This is where all the tar files go
  # and where the packages get built prior to installation in the toolchain.
  working_dir = tempfile.mkdtemp(prefix='boost')
  try:
    DownloadAndExtract(working_dir, BOOST_URL, BOOST_PATH)
  except:
    print "Error in download"
    return 1
  boost_include = options.third_party_dir
  build_utils.ForceMakeDirs(boost_include)
  boost_path = os.path.abspath(os.path.join(working_dir, BOOST_PATH))
  # Copy the headers.
  print 'Installing boost headers into %s...' % boost_include
  dst_include_dir = os.path.join(boost_include, 'boost')
  shutil.rmtree(dst_include_dir, ignore_errors=True)
  shutil.copytree(os.path.join(boost_path, 'boost'),
                  dst_include_dir,
                  symlinks=True)
  # Copy the license file.
  print 'Installing boost license...'
  shutil.copy(os.path.join(boost_path, 'LICENSE_1_0.txt'), dst_include_dir)

  # Clean up.
  shutil.rmtree(working_dir, ignore_errors=True)
  return 0


# Parse the command-line args and set up the options object.  There is one
# command-line switch:
#   --toolchain=<path to the platform-specific toolchain>
#               e.g.: --toolchain=../toolchain/mac-x86
#               default is 'toolchain'.
#               --toolchain can appear more than once, the Boost library is
#               installed into each toolchain listed.
def main(argv):
  parser = OptionParser()
  parser.add_option(
      '-t', '--toolchain', dest='toolchains',
      action='append',
      type='string',
      help='NaCl toolchain directory')
  parser.add_option(
      '--third-party', dest='third_party_dir',
      type='string',
      default='third_party',
      help='location of third_party directory')
  (options, args) = parser.parse_args(argv)
  if args:
    print 'WARNING: unrecognized argument: %s' % str(args)
    parser.print_help()

  if not options.toolchains:
    options.toolchains = [build_utils.TOOLCHAIN_AUTODETECT]
  options.toolchains = [build_utils.NormalizeToolchain(tc)
                        for tc in options.toolchains]

  print "Installing boost into %s" % str(options.third_party_dir)
  return InstallBoost(options)


if __name__ == '__main__':
  main(sys.argv[1:])
