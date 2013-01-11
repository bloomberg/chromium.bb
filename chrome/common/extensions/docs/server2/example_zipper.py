# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from io import BytesIO
import re
from zipfile import ZipFile

import compiled_file_system as compiled_fs

class ExampleZipper(object):
  """This class creates a zip file given a samples directory.
  """
  def __init__(self, file_system, cache_factory, base_path):
    self._base_path = base_path
    self._zip_cache = cache_factory.Create(self._MakeZipFile,
                                           compiled_fs.ZIP)
    self._file_system = file_system

  def _MakeZipFile(self, base_dir, files):
    zip_path = os.path.commonprefix(files).rsplit('/', 1)[-2]
    prefix = zip_path.rsplit('/', 1)[-2]
    if zip_path + '/manifest.json' not in files:
      return None
    zip_bytes = BytesIO()
    zip_file = ZipFile(zip_bytes, mode='w')
    try:
      for name, file_contents in (
          self._file_system.Read(files, binary=True).Get().iteritems()):
        zip_file.writestr(name[len(prefix):].strip('/'), file_contents)
    finally:
      zip_file.close()
    return zip_bytes.getvalue()

  def Create(self, path):
    """ Creates a new zip file from the recursive contents of |path|
    as returned by |_zip_cache|.
    Paths within the zip file are given relative to and including |path|.
    """
    return self._zip_cache.GetFromFileListing(
        self._base_path + '/' + path)
