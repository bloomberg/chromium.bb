#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all the buildbot steps for ChromeDriver except for update/compile."""

import optparse
import os
import sys
import urllib2
import zipfile

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(_THIS_DIR, os.pardir, 'pylib'))

from common import chrome_paths
from common import util


def MaybeRelease(revision):
  # Version is embedded as: const char kChromeDriverVersion[] = "0.1";
  with open(os.path.join(_THIS_DIR, 'version.cc'), 'r') as f:
    version_line = filter(lambda x: 'kChromeDriverVersion' in x, f.readlines())
  version = version_line[0].split('"')[1]

  # This assumes the bitness of python is the same as the built ChromeDriver.
  bitness = '32'
  if sys.maxint > 2**32:
    bitness = '64'
  zip_name = 'experimental_chromedriver2_%s%s_%s.zip' % (
      util.GetPlatformName(), bitness, version)

  site = 'https://code.google.com/p/chromedriver/downloads/list'
  s = urllib2.urlopen(site)
  downloads = s.read()
  s.close()

  if zip_name in downloads:
    return 0

  print '@@@BUILD_STEP releasing %s@@@' % zip_name
  if util.IsWindows():
    server_orig_name = 'chromedriver2_server.exe'
    server_name = 'chromedriver.exe'
  else:
    server_orig_name = 'chromedriver2_server'
    server_name = 'chromedriver'
  server = os.path.join(chrome_paths.GetBuildDir([server_orig_name]),
                        server_orig_name)

  print 'Zipping ChromeDriver server', server
  temp_dir = util.MakeTempDir()
  zip_path = os.path.join(temp_dir, zip_name)
  f = zipfile.ZipFile(zip_path, 'w')
  f.write(server, server_name)
  f.close()

  cmd = [
    sys.executable,
    os.path.join(_THIS_DIR, 'third_party', 'googlecode',
                 'googlecode_upload.py'),
    '--summary', 'alpha version of ChromeDriver2 r%s' % revision,
    '--project', 'chromedriver',
    '--user', 'chromedriver.bot@gmail.com',
    '--label', 'Release-Alpha',
    zip_path
  ]
  code = util.RunCommand(cmd)
  if code != 0:
    print '@@@STEP_FAILURE@@@'
  return code


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '-r', '--revision', type='string', default=None,
      help='Chromium revision')
  options, _ = parser.parse_args()
  if options.revision is None:
    parser.error('Must supply a --revision')

  cmd = [
    sys.executable,
    os.path.join(_THIS_DIR, 'run_all_tests.py'),
  ]
  code = 0 #util.RunCommand(cmd)
  if code != 0:
    return code

  return MaybeRelease(options.revision)


if __name__ == '__main__':
  sys.exit(main())
