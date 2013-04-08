# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from io import BytesIO
import re
from zipfile import ZipFile

import compiled_file_system as compiled_fs

# Increment this if the data model changes for ExampleZipper.
_VERSION = 1

class ExampleZipper(object):
  """This class creates a zip file given a samples directory.
  """
  def __init__(self, file_system, compiled_fs_factory, base_path):
    self._base_path = base_path.rstrip('/')
    self._zip_cache = compiled_fs_factory.Create(self._MakeZipFile,
                                                 ExampleZipper,
                                                 version=_VERSION)
    self._file_system = file_system

  def _MakeZipFile(self, base_dir, files):
    if 'manifest.json' not in files:
      return None
    zip_bytes = BytesIO()
    zip_file = ZipFile(zip_bytes, mode='w')
    try:
      for name, file_contents in (
          self._file_system.Read(['%s%s' % (base_dir, f) for f in files],
                                 binary=True).Get().iteritems()):
        # We want e.g. basic.zip to expand to basic/manifest.json etc, not
        # chrome/common/extensions/.../basic/manifest.json, so only use the
        # end of the path component when writing into the zip file.
        redundant_prefix = '%s/' % base_dir.rstrip('/').rsplit('/', 1)[0]
        zip_file.writestr(name[len(redundant_prefix):], file_contents)
    finally:
      zip_file.close()
    return zip_bytes.getvalue()

  def Create(self, path):
    """ Creates a new zip file from the recursive contents of |path|
    as returned by |_zip_cache|.
    Paths within the zip file are given relative to and including |path|.
    """
    return self._zip_cache.GetFromFileListing(
        '%s/%s' % (self._base_path, path.strip('/')))
