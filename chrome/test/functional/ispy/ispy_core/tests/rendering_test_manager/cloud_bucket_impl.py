# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of CloudBucket using Google Cloud Storage as the backend."""
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                             'gsutil', 'third_party', 'boto'))
import boto


sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
from tests.rendering_test_manager import cloud_bucket


class CloudBucketImpl(cloud_bucket.CloudBucket):
  """Subclass of cloud_bucket.CloudBucket with actual GCS commands."""

  def __init__(self, key, secret, bucket_name):
    """Initializes the bucket with a key, secret, and bucket_name.

    Args:
      key: the API key to access GCS.
      secret: the API secret to access GCS.
      bucket_name: the name of the bucket to connect to.

    Returns:
      Instance of CloudBucketImpl.
    """
    uri = boto.storage_uri('', 'gs')
    conn = uri.connect(key, secret)
    self.bucket = conn.get_bucket(bucket_name)

  # override
  def UploadFile(self, path, contents, content_type):
    key = boto.gs.key.Key(self.bucket)
    key.set_metadata('Content-Type', content_type)
    key.key = path
    key.set_contents_from_string(contents)

  # override
  def DownloadFile(self, path):
    key = boto.gs.key.Key(self.bucket)
    key.key = path
    if key.exists():
      return key.get_contents_as_string()
    else:
      raise cloud_bucket.FileNotFoundError

  # override
  def RemoveFile(self, path):
    key = boto.gs.key.Key(self.bucket)
    key.key = path
    key.delete()

  # override
  def FileExists(self, path):
    key = boto.gs.key.Key(self.bucket)
    key.key = path
    return key.exists()

  # override
  def GetURL(self, path):
    key = boto.gs.key.Key(self.bucket)
    key.key = path
    if key.exists():
      # Corrects a bug in boto that incorrectly generates a url
      #  to a resource in Google Cloud Storage.
      return key.generate_url(3600).replace('AWSAccessKeyId', 'GoogleAccessId')
    else:
      raise cloud_bucket.FileNotFoundError(path)

  # override
  def GetAllPaths(self, prefix):
    return (key.key for key in self.bucket.get_all_keys(prefix=prefix))
