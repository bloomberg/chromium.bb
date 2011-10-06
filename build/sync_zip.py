# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import zipfile

import download_utils
import http_download

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


def SyncZip(url, target, username=None, password=None, verbose=True,
            remove_prefix=None):
  zip_filename = os.path.join(target, os.path.basename(url))
  if verbose:
    print 'Downloading %s to %s...' % (url, zip_filename)
  http_download.HttpDownload(url, zip_filename,
                             username=username, password=password)
  Unzip(zip_filename, target, verbose=verbose, remove_prefix=remove_prefix)
