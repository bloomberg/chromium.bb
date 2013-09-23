# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Abstract injector class for GCS requests."""


class FileNotFoundError(Exception):
  """Thrown by a subclass of CloudBucket when a file is not found."""
  pass


class CloudBucket(object):
  """An abstract injector that wraps GCS requests."""

  def UploadFile(self, path, contents, content_type):
    """Uploads a file to GCS.

    Args:
      path: where in GCS to upload the file.
      contents: the contents of the file to be uploaded.
      content_type: the MIME Content-Type of the file.
    """
    raise NotImplementedError

  def DownloadFile(self, path):
    """Downsloads a file from GCS.

    Args:
      path: the location in GCS to download the file from.

    Returns:
      String contents of the file downloaded.

    Raises:
      bucket_injector.NotFoundException: if the file is not found.
    """
    raise NotImplementedError

  def RemoveFile(self, path):
    """Removes a file from GCS.

    Args:
      path: the location in GCS to download the file from.
    """
    raise NotImplementedError

  def FileExists(self, path):
    """Checks if a file exists in GCS.

    Args:
      path: the location in GCS of the file.

    Returns:
      boolean representing whether the file exists in GCS.
    """
    raise NotImplementedError

  def GetURL(self, path):
    """Gets a URL to an item in GCS from its path.

    Args:
      path: the location in GCS of a file.

    Returns:
      an url to a file in GCS.

    Raises:
      bucket_injector.NotFoundException: if the file is not found.
    """
    raise NotImplementedError

  def GetAllPaths(self, prefix):
    """Gets paths to files in GCS that start with a prefix.

    Args:
      prefix: the prefix to filter files in GCS.

    Returns:
      a generator of paths to files in GCS.
    """
    raise NotImplementedError
