#!/usr/bin/python
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

"""Download all Native Client toolchains for this platform.

This module downloads multiple tgz's and expands them.
"""

import optparse
import os.path
import sys
import tempfile

import chromebinaries
import download_utils
import sync_file
import sync_zip


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


def CreateLinkToMacFrameworkDir(bin_root):
  # On Mac, a link to the 'Chromium Framework.framework' directory must be
  # available next to _pyautolib.so so that it can load the directory. Since
  # the framework directory path is dependent on the Chromium version, we list
  # the subdirectories of 'Versions' and pick the first subdirectory. This works
  # on the NaCl bots because only one Chromium version is downloaded at a time.
  dir_name = 'Chromium Framework.framework'
  rel_path = os.path.join('Chromium.app',
                          'Contents',
                          'Versions')
  rel_path = os.path.join(rel_path,
                          os.listdir(os.path.join(bin_root, rel_path))[0],
                          dir_name)
  sys.stdout.write('Creating symlink in %s: %s -> %s\n' % (bin_root,
                                                           dir_name,
                                                           rel_path))
  os.symlink(rel_path, os.path.join(bin_root, dir_name))


def Main():
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

  uid = " ".join([options.base_url, options.os, options.arch, options.revision])

  if options.force or not download_utils.SourceIsCurrent(dst, uid):

    # Create a temporary working directory.
    tempdir = tempfile.mkdtemp(prefix='nacl_chrome_download_')
    try:
      chrome_url = chromebinaries.GetChromeURL(options.base_url,
                                               options.os,
                                               options.arch,
                                               options.revision)
      # Everything inside the zip file will be inside this directory.
      prefix = os.path.splitext(os.path.split(chrome_url)[1])[0] + '/'

      try:
        sync_zip.SyncZip(chrome_url, tempdir, remove_prefix=prefix)
      except Exception:
        print
        print '*'*78
        print ('A Chromium binary cannot be found for revision %s - run '
               'build/find_chrome_revisions.py to find a better revision'
               % str(options.revision))
        print '*'*78
        print
        raise

      # Copy over files necessary to run pyauto.
      pyautopy_url, pyautolib_url = chromebinaries.GetPyAutoURLs(
          options.base_url,
          options.os,
          options.arch,
          options.revision)
      sync_file.SyncFile(pyautopy_url, tempdir)
      sync_file.SyncFile(pyautolib_url, tempdir)

      # On Mac, create a symlink to the framework directory so it can be loaded.
      if options.os == 'mac':
        CreateLinkToMacFrameworkDir(tempdir)

      # Move binaries from temp directory to destination.
      download_utils.WriteSourceStamp(tempdir, uid)
      sys.stdout.write('Moving %s to %s\n' % (tempdir, dst))
      download_utils.MoveDirCleanly(tempdir, dst)

    except Exception:
      download_utils.RemoveDir(tempdir)
      raise
  else:
    sys.stdout.write('No need to download Chrome.\n')


if __name__ == '__main__':
  Main()
