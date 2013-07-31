# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from io import BytesIO
from zipfile import ZipFile

class ExampleZipper(object):
  '''This class creates a zip file given a samples directory.
  '''
  def __init__(self, compiled_fs_factory, file_system, base_path):
    self._base_path = base_path.rstrip('/')
    self._file_system = file_system
    self._zip_cache = compiled_fs_factory.Create(self._MakeZipFile,
                                                 ExampleZipper)

  def _MakeZipFile(self, base_dir, files):
    if 'manifest.json' not in files:
      return None
    zip_bytes = BytesIO()
    zip_file = ZipFile(zip_bytes, mode='w')
    try:
      for file_name in files:
        file_path = '%s%s' % (base_dir, file_name)
        file_contents = self._file_system.ReadSingle(file_path, binary=True)
        if isinstance(file_contents, unicode):
          # Data is sometimes already cached as unicode.
          file_contents = file_contents.encode('utf8')
        # We want e.g. basic.zip to expand to basic/manifest.json etc, not
        # chrome/common/extensions/.../basic/manifest.json, so only use the
        # end of the path component when writing into the zip file.
        redundant_prefix = '%s/' % base_dir.rstrip('/').rsplit('/', 1)[0]
        zip_file.writestr(file_path[len(redundant_prefix):], file_contents)
    finally:
      zip_file.close()
    return zip_bytes.getvalue()

  def Create(self, path):
    ''' Creates a new zip file from the recursive contents of |path|
    as returned by |_zip_cache|.
    Paths within the zip file are given relative to and including |path|.
    '''
    return self._zip_cache.GetFromFileListing(
        '%s/%s' % (self._base_path, path.strip('/')))
