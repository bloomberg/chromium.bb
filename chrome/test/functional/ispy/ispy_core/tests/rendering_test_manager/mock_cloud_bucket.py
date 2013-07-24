# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Subclass of CloudBucket used for testing."""

from tests.rendering_test_manager import cloud_bucket


class MockCloudBucket(cloud_bucket.CloudBucket):
  """Subclass of CloudBucket used for testing."""

  def __init__(self):
    """Initializes the MockCloudBucket with its datastore.

    Returns:
      An instance of MockCloudBucket.
    """
    self.datastore = {}

  def Reset(self):
    """Clears the MockCloudBucket's datastore."""
    self.datastore = {}

  # override
  def UploadFile(self, path, contents, content_type):
    self.datastore[path] = contents

  # override
  def DownloadFile(self, path):
    if self.datastore.has_key(path):
      return self.datastore[path]
    else:
      raise cloud_bucket.FileNotFoundError

  # override
  def RemoveFile(self, path):
    if self.datastore.has_key(path):
      self.datastore.pop(path)

  # override
  def FileExists(self, path):
    return self.datastore.has_key(path)

  # override
  def GetURL(self, path):
    if self.datastore.has_key(path):
      return path
    else:
      raise cloud_bucket.FileNotFoundError

  # override
  def GetAllPaths(self, prefix):
    return (item[0] for item in self.datastore.items()
            if item[0].startswith(prefix))
