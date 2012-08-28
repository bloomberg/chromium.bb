#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Download all Native Client toolchains for this platform.

This module downloads multiple tgz's and expands them.
"""

import chromebinaries
import download_utils
import optparse
import os
import sys
import tempfile
import zipfile


def MakeCommandLineParser():
  parser = optparse.OptionParser()
  parser.add_option('-b', '--base-url', dest='base_url',
                    default=chromebinaries.BASE_URL,
                    help='base url to download from')
  parser.add_option('-a', '--arch', dest='arch', default='x86-32',
                    help='Machine architecture (x86-32, x86-64)')
  parser.add_option('-o', '--os', dest='os', default=None,
                    help='Operating system (windows, mac, linux)')
  parser.add_option('-d', '--dst', dest='dst', default='.',
                    help='Directory to download into.')
  parser.add_option('-r', '--revision', dest='revision', default=None,
                    help='revision to download')
  parser.add_option('-f', '--force', dest='force', default=False,
                    action='store_true', help='Force the update')
  return parser


def Unzip(zip_filename, target, verbose=True, remove_prefix=None):
  if verbose:
    print 'Extracting from %s...' % zip_filename
  zf = zipfile.ZipFile(zip_filename, 'r')
  for info in zf.infolist():
    path = info.filename
    if remove_prefix is not None:
      if path.startswith(remove_prefix):
        path = path[len(remove_prefix):]
    if not path or path.endswith('/'):
      # It's a directory
      continue
    fullpath = os.path.join(target, os.path.normpath(path))
    bits = (info.external_attr >> 16) & 0777

    if verbose:
      print path, '%o' % bits
    data = zf.read(info.filename)
    download_utils.EnsureFileCanBeWritten(fullpath)
    f = open(fullpath, 'wb')
    f.write(data)
    f.close()
    os.chmod(fullpath, bits)


def Main():
 # Generate the time for the most recently modified script used by the download
  script_dir = os.path.dirname(__file__)
  src_list = ['download_chrome.py', 'download_utils.py', 'http_download.py']
  srcs = [os.path.join(script_dir, src) for src in src_list]
  src_times = []
  for src in srcs:
    src_times.append( os.stat(src).st_mtime )
  script_time = sorted(src_times)[-1]

  parser = MakeCommandLineParser()
  options, args = parser.parse_args()
  if args:
    parser.error('ERROR: invalid argument')
  try:
    options.os = download_utils.PlatformName(options.os)
  except Exception:
    parser.error('Invalid OS \'%s\'' % options.os)
  if options.revision is None:
    options.revision = chromebinaries.GetChromeRevision()

  # Slightly magical, but much safer that using options.dst directly because
  # dst will get blown away.
  # The downside is that SCons will need to understand how --dst gets modified.
  dst = os.path.join(options.dst, '%s_%s' % (options.os, options.arch))
  chrome_url = chromebinaries.GetChromeURL(options.base_url, options.os,
                                           options.arch, options.revision)

  tempdir = tempfile.mkdtemp(prefix='nacl_chrome_download_')
  filepath = os.path.join(tempdir, chrome_url.split('/')[-1])
  try:
    if options.force or download_utils.SyncURL(chrome_url, filepath,
                                               stamp_dir=dst,
                                               min_time=script_time):
      try:
        prefix = os.path.splitext(os.path.split(chrome_url)[1])[0] + '/'
        Unzip(filepath, tempdir, verbose=True, remove_prefix=prefix)
      except Exception:
        print
        print '*'*78
        print ('A Chromium binary cannot be found for revision %s - run '
               'build/find_chrome_revisions.py to find a better revision'
               % str(options.revision))
        print '*'*78
        print
        raise

      # Move binaries from temp directory to destination.
      download_utils.WriteSourceStamp(tempdir, chrome_url)
      sys.stdout.write('Moving %s to %s\n' % (tempdir, dst))
      download_utils.MoveDirCleanly(tempdir, dst)
    else:
      sys.stdout.write('No need to download Chrome.\n')
  except Exception:
    download_utils.RemoveDir(tempdir)
    print 'Failed to download.'
    raise


if __name__ == '__main__':
  Main()

