# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Request Handler to allow test mask updates."""

import webapp2
import re
import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
import auth_constants
from tools import rendering_test_manager
from tools import image_tools
from tests.rendering_test_manager import cloud_bucket_impl


class UpdateMaskHandler(webapp2.RequestHandler):
  """Request handler to allow test mask updates."""

  def post(self):
    """Accepts post requests.

    This method will accept a post request containing device, site and
      device_id parameters. This method takes the diff of the run
      indicated by it's parameters and adds it to the mask of the run's
      test. It will then delete the run it is applied to and redirect
      to the device list view.
    """
    batch_name = self.request.get('batch_name')
    test_name = self.request.get('test_name')
    run_name = self.request.get('run_name')

    # Short-circuit if a parameter is missing.
    if not (batch_name and test_name and run_name):
      self.response.header['Content-Type'] = 'json/application'
      self.response.write(json.dumps(
          {'error': 'batch_name, test_name, and run_name must be '
                    'supplied to update a mask.'}))
      return
    # Otherwise, set up the rendering_test_manager.
    self.bucket = cloud_bucket_impl.CloudBucketImpl(
        auth_constants.KEY, auth_constants.SECRET, auth_constants.BUCKET)
    self.manager = rendering_test_manager.RenderingTestManager(self.bucket)
    # Short-circuit if the failure does not exist.
    if not self.manager.FailureExists(batch_name, test_name, run_name):
      self.response.header['Content-Type'] = 'json/application'
      self.response.write(json.dumps(
        {'error': 'Could not update mask because failure does not exist.'}))
      return
    # Get the failure namedtuple (which also computes the diff).
    failure = self.manager.GetFailure(batch_name, test_name, run_name)
    # Get the mask of the image.
    path = 'tests/%s/%s/mask.png' % (batch_name, test_name)
    mask = self.manager.DownloadImage(path)
    # Merge the masks.
    combined_mask = image_tools.AddMasks([mask, failure.diff])
    # Upload the combined mask in place of the original.
    self.manager.UploadImage(path, combined_mask)
    # Now that there is no div for the two images, remove the failure.
    self.manager.RemoveFailure(batch_name, test_name, run_name)
    # Redirect back to the sites list for the device.
    self.redirect('/?batch_name=%s' % batch_name)
