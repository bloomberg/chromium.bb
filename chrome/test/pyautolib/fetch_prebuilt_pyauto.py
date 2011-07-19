#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fetch prebuilt binaries to run PyAuto.

Sets up Chrome and PyAuto binaries using prebuilt binaries from the
continuous build archives.  Works on mac, win, linux (32 & 64 bit).

Examples:
  On Mac:
  $ python fetch_prebuilt_pyauto.py -d xcodebuild/Release
      http://build.chromium.org/f/chromium/continuous/mac/LATEST

  On Win:
  $ python fetch_prebuilt_pyauto.py -d chrome\Release
      http://build.chromium.org/f/chromium/continuous/win/LATEST
"""

import glob
import optparse
import os
import shutil
import sys
import urllib

import pyauto_utils


class FetchPrebuilt(object):
  """Util class to fetch prebuilt binaries to run PyAuto."""

  def _ParseArgs(self):
    parser = optparse.OptionParser()
    parser.add_option(
        '-d', '--outdir', type='string', default=None,
        help='Directory in which to setup. This is typically the directory '
             'where the binaries would go when compiled from source.')
    parser.add_option(
        '-p', '--platform', type='string',
        default=pyauto_utils.GetCurrentPlatform(),
        help='Platform. Valid options: win, mac, linux32, linux64. '
             'Default: current platform (%s)' %
             pyauto_utils.GetCurrentPlatform())
    self._options, self._args = parser.parse_args()
    if not self._options.outdir:
      print >>sys.stderr, 'Need output directory: -d/--outdir'
      sys.exit(1)
    if not self._args:
      print >>sys.stderr, 'Need download url'
      sys.exit(2)
    self._outdir = self._options.outdir
    self._url = self._args[0]

    # Setup urls to download
    self._chrome_zip_name = 'chrome-%s' % { 'linux64': 'linux64bit',
                                            'linux32': 'linux32bit',
                                            'mac': 'mac',
                                            'win': 'win32'
                                          }[self._options.platform]
    self._chrome_zip_url = '%s/%s.zip' % (self._url, self._chrome_zip_name)
    chrome_test_url = '%s/%s.test' % (self._url, self._chrome_zip_name)
    self._pyautolib_py_url = '%s/pyautolib.py' % chrome_test_url
    self._pyautolib_so_url = '%s/%s' % (chrome_test_url,
                                        { 'linux64': '_pyautolib.so',
                                          'linux32': '_pyautolib.so',
                                          'mac': '_pyautolib.so',
                                          'win': '_pyautolib.pyd',
                                         }[self._options.platform])

  def Cleanup(self):
    """Remove old binaries, if any."""
    pass

  def Run(self):
    self._ParseArgs()
    if not os.path.isdir(self._outdir):
      os.makedirs(self._outdir)

    # Fetch chrome & pyauto binaries
    print 'Fetching'
    print self._chrome_zip_url
    print self._pyautolib_py_url
    print self._pyautolib_so_url
    chrome_zip = urllib.urlretrieve(self._chrome_zip_url)[0]
    pyautolib_py = urllib.urlretrieve(self._pyautolib_py_url)[0]
    pyautolib_so = urllib.urlretrieve(self._pyautolib_so_url)[0]
    chrome_unzip_dir = os.path.join(self._outdir, self._chrome_zip_name)
    if os.path.exists(chrome_unzip_dir):
      print 'Cleaning', chrome_unzip_dir
      pyauto_utils.RemovePath(chrome_unzip_dir)
    pyauto_utils.UnzipFilenameToDir(chrome_zip, self._outdir)

    # Copy over the binaries to outdir
    items_to_copy = {
      pyautolib_py: os.path.join(self._outdir, 'pyautolib.py'),
      pyautolib_so: os.path.join(self._outdir,
                                 { 'linux64': '_pyautolib.so',
                                   'linux32': '_pyautolib.so',
                                   'mac': '_pyautolib.so',
                                   'win': '_pyautolib.pyd'
                                  }[self._options.platform])
    }
    unzip_dir_contents = glob.glob(os.path.join(chrome_unzip_dir, '*'))
    for item in unzip_dir_contents:
      name = os.path.basename(item)
      items_to_copy[item] = os.path.join(self._outdir, name)

    for src, dest in items_to_copy.iteritems():
      pyauto_utils.RemovePath(dest)
      print '%s ==> %s' % (os.path.basename(src), dest)
      shutil.move(src, dest)
    pyauto_utils.RemovePath(chrome_unzip_dir)

    # Final setup (if any)
    # Create symlink to .framework on Mac
    if self._options.platform == 'mac':
      mac_app_name = os.path.basename([x for x in unzip_dir_contents
                                       if x.endswith('.app')][0])
      os.chdir(self._outdir)
      framework = glob.glob(os.path.join(
          mac_app_name, 'Contents', 'Versions', '*', '*.framework'))[0]
      print framework
      dest = os.path.basename(framework)
      os.path.lexists(dest) and os.remove(dest)
      print 'Creating symlink "%s"' % dest
      os.symlink(framework, dest)

    print 'Prepared binaries in "%s"' % self._outdir


if __name__ == '__main__':
  FetchPrebuilt().Run()

