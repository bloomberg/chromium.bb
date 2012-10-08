#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
import httplib
import optparse
import os
import platform
import shutil
import sys
import urllib
import urllib2
import urlparse

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
        'Default: current platform (%s)' % pyauto_utils.GetCurrentPlatform())
    parser.add_option(
        '-l', '--latest', action='store_true', default=False,
        help='Download the latest chromium build from commondatastorage. '
        '[default=False]')
    self._options, self._args = parser.parse_args()
    if self._options.latest:
      self._url = self._GetLastestDownloadURL(self._options.platform)
    elif not self._args:
      print >>sys.stderr, 'Need download url'
      sys.exit(2)
    else:
      self._url = self._args[0]
    if not self._options.outdir:
      print >>sys.stderr, 'Need output directory: -d/--outdir'
      sys.exit(1)
    self._outdir = self._options.outdir
    # Chromium continuous build archive has a non-standard format.
    if 'index.html?path=' in self._url:
      self._url = self._url.replace('index.html?path=', '')
    self._url = self._url.rstrip('/')
    # Determine name of zip.
    if not self._options.platform.startswith('linux'):
      self._chrome_zip_name = 'chrome-%s' % {'mac': 'mac',
                                             'win': 'win32'
                                            }[self._options.platform]
    else:
      linux_32_names = ['linux', 'lucid32bit']
      linux_64_names = ['linux64', 'lucid64bit']
      linux_names = {'linux': linux_32_names + linux_64_names,
                     'linux32': linux_32_names,
                     'linux64': linux_64_names
                    }[self._options.platform]
      for name in linux_names:
        zip_name = 'chrome-' + name
        if pyauto_utils.DoesUrlExist('%s/%s.zip' % (self._url, zip_name)):
          self._chrome_zip_name = zip_name
          break
      else:
        raise RuntimeError('Could not find chrome zip at ' + self._url)

    # Setup urls to download.
    self._chrome_zip_url = '%s/%s.zip' % (self._url, self._chrome_zip_name)
    self._remoting_zip_url = self._url + '/' + 'remoting-webapp.zip'
    chrome_test_url = '%s/%s.test' % (self._url, self._chrome_zip_name)
    self._pyautolib_py_url = '%s/pyautolib.py' % chrome_test_url
    if self._options.platform == 'win':
      self._pyautolib_so_name = '_pyautolib.pyd'
      self._chromedriver_name = 'chromedriver.exe'
    else:
      self._pyautolib_so_name = '_pyautolib.so'
      self._chromedriver_name = 'chromedriver'
    if self._options.platform == 'mac':
      self._ffmpegsumo_so_name = 'ffmpegsumo.so'
      self._ffmpegsumo_so_url = chrome_test_url + '/' + self._ffmpegsumo_so_name
    self._pyautolib_so_url = chrome_test_url + '/' + self._pyautolib_so_name
    self._chromedriver_url = chrome_test_url + '/' + self._chromedriver_name

  def _GetLastestDownloadURL(self, os_platform):
    os_type = {'win': 'Win',
               'mac': 'Mac',
               'linux': 'Linux',
               'linux32': 'Linux',
               'linux64': 'Linux_x64'}[os_platform]
    if os_type == 'Linux' and platform.architecture()[0] == '64bit':
      os_type = 'Linux_x64'
    last_change_url = ('http://commondatastorage.googleapis.com/'
                       'chromium-browser-continuous/%s/LAST_CHANGE' % os_type)
    response = urllib2.urlopen(last_change_url)
    last_change = response.read()
    if not last_change:
      print >>sys.stderr, ('Unable to get latest from %s' % last_change_url)
      sys.exit(2)
    last_change_url = ('http://commondatastorage.googleapis.com/'
                       'chromium-browser-continuous/%s/%s' % (os_type,
                                                              last_change))
    return last_change_url

  def Cleanup(self):
    """Remove old binaries, if any."""
    pass

  def Run(self):
    self._ParseArgs()
    if not os.path.isdir(self._outdir):
      os.makedirs(self._outdir)
    get_remoting = pyauto_utils.DoesUrlExist(self._remoting_zip_url)

    # Fetch chrome & pyauto binaries
    print 'Fetching', self._chrome_zip_url
    chrome_zip = urllib.urlretrieve(self._chrome_zip_url)[0]

    if get_remoting:
      print 'Fetching', self._remoting_zip_url
      remoting_zip = urllib.urlretrieve(self._remoting_zip_url)[0]
    else:
      print 'Warning: %s does not exist.' % self._remoting_zip_url

    print 'Fetching', self._pyautolib_py_url
    pyautolib_py = urllib.urlretrieve(self._pyautolib_py_url)[0]

    print 'Fetching', self._pyautolib_so_url
    pyautolib_so = urllib.urlretrieve(self._pyautolib_so_url)[0]

    if self._options.platform == 'mac':
      print 'Fetching', self._ffmpegsumo_so_url
      ffmpegsumo_so = urllib.urlretrieve(self._ffmpegsumo_so_url)[0]

    print 'Fetching', self._chromedriver_url
    chromedriver = urllib.urlretrieve(self._chromedriver_url)[0]

    chrome_unzip_dir = os.path.join(self._outdir, self._chrome_zip_name)
    if os.path.exists(chrome_unzip_dir):
      print 'Cleaning', chrome_unzip_dir
      pyauto_utils.RemovePath(chrome_unzip_dir)
    print 'Unzipping'
    pyauto_utils.UnzipFilenameToDir(chrome_zip, self._outdir)
    if get_remoting:
      pyauto_utils.UnzipFilenameToDir(remoting_zip, self._outdir)
      shutil.move(self._outdir + '/remoting-webapp',
                  self._outdir + '/remoting/remoting.webapp')

    # Copy over the binaries to outdir
    items_to_copy = {
      pyautolib_py: os.path.join(self._outdir, 'pyautolib.py'),
      pyautolib_so: os.path.join(self._outdir, self._pyautolib_so_name),
      chromedriver: os.path.join(self._outdir, self._chromedriver_name)
    }
    if self._options.platform == 'mac':
      items_to_copy[ffmpegsumo_so] = \
          os.path.join(self._outdir, self._ffmpegsumo_so_name)

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
    # Set executable bit on chromedriver binary.
    if not self._options.platform == 'win':
      os.chmod(items_to_copy[chromedriver], 0700)

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
    return 0


if __name__ == '__main__':
  sys.exit(FetchPrebuilt().Run())
