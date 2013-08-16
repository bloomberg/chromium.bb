# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Request handler to display an image from ispy.googleplex.com."""

import json
import os
import sys
import webapp2

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))

from tests.rendering_test_manager import cloud_bucket
from tests.rendering_test_manager import cloud_bucket_impl

import ispy_auth_constants


class ImageHandler(webapp2.RequestHandler):
  """A request handler to avoid the Same-Origin problem in the debug view."""

  def get(self):
    """Handles get requests to the ImageHandler.

    GET Parameters:
      file_path: A path to an image resource in Google Cloud Storage.
    """
    file_path = self.request.get('file_path')
    if not file_path:
      self.error(404)
      return
    bucket = cloud_bucket_impl.CloudBucketImpl(
        ispy_auth_constants.KEY, ispy_auth_constants.SECRET,
        ispy_auth_constants.BUCKET)
    try:
      image = bucket.DownloadFile(file_path)
    except cloud_bucket.FileNotFoundError:
      self.error(404)
    else:
      self.response.headers['Content-Type'] = 'image/png'
      self.response.out.write(image)
