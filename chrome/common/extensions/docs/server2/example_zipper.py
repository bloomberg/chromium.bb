# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from io import BytesIO
import re
from zipfile import ZipFile

class ExampleZipper(object):
  """This class creates a zip file given a samples directory.
  """
  def __init__(self, fetcher, cache_builder, base_path, match_path):
    self._base_path = base_path
    self._zip_cache = cache_builder.build(self._MakeZipFile)
    self._fetcher = fetcher
    self._match_path = match_path

  def _MakeZipFile(self, files):
    zip_path = os.path.commonprefix(files).rsplit('/', 1)[-2]
    prefix = zip_path.rsplit('/', 1)[-2]
    if zip_path + '/manifest.json' not in files:
      return None
    zip_bytes = BytesIO()
    zip_file = ZipFile(zip_bytes, mode='w')
    try:
      for filename in files:
          zip_file.writestr(
              filename[len(prefix):].strip('/'),
              self._fetcher.FetchResource(filename).content)
    finally:
      zip_file.close()
    return zip_bytes.getvalue()

  def create(self, path):
    """ Creates a new zip file from the recursive contents of |path|
    as returned by |_zip_cache|.
    Paths within the zip file are given relative to and including |path|.
    """
    return self._zip_cache.getFromFileListing(self._base_path + '/' + path,
                                              recursive=True)
