# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Request handler to allow restful interaction with tests and runs."""

import json
import os
import sys
import webapp2

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))

from tests.rendering_test_manager import cloud_bucket
from tests.rendering_test_manager import cloud_bucket_impl
from tools import image_tools
from tools import rendering_test_manager

import auth_constants

class MissingArgError(Exception):
  pass


def Command(*expected_parameters):
  """Convenient decorator for Command functions the CommandHandler.

  Args:
    *expected_parameters: the expected parameters for the command.

  Returns:
    A decorated method that accepts a request object and returns nothing.
  """
  def _Decorate(method):
    def _Wrapper(self, request):
      # Try accessing all expected_parameters, if any are absent from the
      #  request raise MissingArgError.
      try:
        for param in expected_parameters:
          if not request.get(param):
            raise MissingArgError('Argument: missing %s' % param)
      # If a MissingArgError was raised, write an error response.
      except MissingArgError, e:
        self.response.headers['Content-Type'] = 'application/json'
        self.response.out.write(json.dumps({'success': False, 'error': str(e)}))
      else:
        # If all parameters are present run the decorated method.
        results = method(self, request)
        self.response.headers['Content-Type'] = 'application/json'
        self.response.out.write(json.dumps(results))
    return _Wrapper
  return _Decorate


class CommandHandler(webapp2.RequestHandler):
  """Command handler that allows restful interaction with tests and runs."""

  def __init__(self, *args, **kwargs):
    super(CommandHandler, self).__init__(*args, **kwargs)
    self.command_functions = {
        'upload_test': self._UploadTest,
        'test_exists': self._TestExists,
        'failure_exists': self._FailureExists,
        'run_test': self._RunTest,
        'upload_test_pink_out': self._UploadTestPinkOut,
        'remove_test': self._RemoveTest,
        'remove_failure': self._RemoveFailure,
        'add_to_test_mask': self._AddToTestMask}

  def post(self):
    """Handles post requests.

    This method accepts a post request that minimally has a 'command' parameter
      which indicates which of the handler's commands to run. If the
      command is not in the valid_commands dictionary, then an error
      will be returned. All responses are json-encoded objects that
      have either a Succeeded or Failed parameter indicating success or
      failure in addition to other returned parameters specific to the command.
    """
    cmd = self.request.get('command')
    if not cmd:
      self._Error(self.request)
      return
    if cmd not in self.command_functions:
      self._InvalidCommand(self.request)
      return
    self.bucket = cloud_bucket_impl.CloudBucketImpl(
        auth_constants.KEY, auth_constants.SECRET, auth_constants.BUCKET)
    self.manager = rendering_test_manager.RenderingTestManager(self.bucket)
    self.command_functions.get(cmd)(self.request)

  @Command()
  def _Error(self, request):
    """This command returns an error when no command is given.

    This command has no expected parameters.

    Args:
      request: A request object.

    Returns:
      A dictionary indicating a failure, and an error message indicating
        that no command was specified.
    """
    return {'success': False, 'error': 'No command was specified.'}

  @Command()
  def _InvalidCommand(self, request):
    """This command returns an error when an invalid command is given.

    This command has no expected parameters.

    Args:
      request: a request object.

    Returns:
      A dictionary indicating a failure, and an error message
        containing all possible valid commands.
    """
    return {'success': False,
            'error': 'Invalid command. Valid commands are: %s.' %
            ' '.join(self.command_functions.keys())}

  @Command('batch_name', 'test_name', 'images')
  def _UploadTest(self, request):
    """Uploads an ispy-test to GCS.

    This function uploads a collection of images as a 'test' to the
      ispy server. A mask is then computed from these images, and the
      first image in the images list and the mask are stored in the
      GCS as an ispy-test.

    This function is called for the command 'upload_test'.
      Request Parameters:
        test_name: The name of the test to be uploaded.
        images: a json encoded list of base64 encoded png images.
      Response JSON:
        succeeded: True.

    Args:
      request: a request object.

    Returns:
      A dictionary indicating success or failure.
    """
    batch_name = request.get('batch_name')
    test_name = request.get('test_name')
    raw_images = request.get('images')
    images = json.loads(raw_images)
    self.manager.UploadTest(
        batch_name, test_name,
        [image_tools.DeserializeImage(image) for image in images])
    return {'success': True}

  @Command('batch_name', 'test_name', 'images', 'pink_out')
  def _UploadTestPinkOut(self, request):
    """Uploads an ispy-test to GCS with the pink_out workaround.

    This function is called for the command 'upload_test_pink_out'.
      Request Parameters:
        test_name: The name of the test to be uploaded.
        images: a json encoded list of base64 encoded png images.
        pink_out: a base64 encoded png image.
      Response JSON:
        succeeded: True.

    Args:
      request: a request object.

    Returns:
      A dictionary indicating success or failure.
    """
    batch_name = request.get('batch_name')
    test_name = request.get('test_name')
    images = [image_tools.DeserializeImage(i)
              for i in json.loads(request.get('images'))]
    pink_out = image_tools.DeserializeImage(request.get('pink_out'))
    # convert the pink_out into a mask
    pink_out.putdata(
        [(0, 0, 0, 255) if px == (255, 71, 191, 255) else (255, 255, 255, 255)
         for px in pink_out.getdata()])
    mask = image_tools.CreateMask(images)
    mask = image_tools.InflateMask(image_tools.CreateMask(images), 7)
    combined_mask = image_tools.AddMasks([mask, pink_out])
    path = 'tests/%s/%s/' % (batch_name, test_name)
    self.manager.UploadImage(path + 'expected.png',
                             images[0])
    self.manager.UploadImage(path + 'mask.png',
                             combined_mask)
    return {'success': True}

  @Command('mask', 'batch_name', 'test_name')
  def _AddToTestMask(self, request):
    """Adds a another mask image to the existing mask for a given test.

      Request Parameters:
        mask: A RGBA png white/black mask image.
        test_name: The name of a test in i-spy to add to.
      Response JSON:
        succeeded: True
      Response Error:
        error: if the test was not found in GCS.

    Args:
      request: A request object.

    Returns:
      A dictionary indicating success or failure.
    """
    batch_name = request.get('batch_name')
    test_name = request.get('test_name')
    mask_to_be_added = image_tools.InflateMask(
        image_tools.DeserializeImage(request.get('mask')), 7)
    if not self.manager.TestExists(batch_name, test_name):
      return {'success': False, 'error': 'Test does not exist.'}
    path = 'tests/%s/%s/mask.png' % (batch_name, test_name)
    test_mask = self.manager.DownloadImage(path)
    combined_mask = image_tools.AddMasks([test_mask, mask_to_be_added])
    self.manager.UploadImage(path, combined_mask)
    return {'success': True}

  @Command('image', 'batch_name', 'test_name', 'run_name')
  def _RunTest(self, request):
    """Runs a test on GCS and stores a failure if it doesn't pass.

    This method compares the submitted image with the expected and mask
      images stored in GCS under the submitted test name. If there are
      any differences between the submitted image and the expected test
      image (with respect to the mask), the submitted image is stored
      as a failure in GCS.

    This method is run if the command parameter is set to 'run_test'.
      Request Parameters:
        'image': A base64 encoded screenshot that corresponds to a test in GCS.
        'test_name': The name of the test that 'image' corresponds to.
        'run_name': The title of this particular test run.
      Response JSON:
        'succeeded': True

    Args:
      request: a request object

    Returns:
      A dictionary indicating success or failure.
    """
    raw_image = request.get('image')
    batch_name = request.get('batch_name')
    test_name = request.get('test_name')
    run_name = request.get('run_name')
    image = image_tools.DeserializeImage(raw_image)
    try:
      self.manager.RunTest(batch_name, test_name, run_name, image)
    except cloud_bucket.FileNotFoundError, e:
      return {'success': False, 'error': str(e)}
    else:
      return {'success': True}

  @Command('batch_name', 'test_name')
  def _TestExists(self, request):
    """Checks to see if a test exists in GCS.

      This method confirms whether or not an expected image, and
        mask exist under the given test_name in GCS. Returning true
        only if all components of a test exist.

      This method is run if the command parameter is 'test_exists'.
        Request Parameters:
          'test_name': The name of the test to look for.
        Response JSON:
          'exists': boolean indicating whether the test exists.
          'succeeded': True.

    Args:
      request: a request object.

    Returns:
      A dictionary indicating success or failure, and if the command was
        successful, whether the test exists.
    """
    batch_name = request.get('batch_name')
    test_name = request.get('test_name')
    return {'success': True, 'exists': self.manager.TestExists(batch_name,
                                                                 test_name)}

  @Command('batch_name', 'test_name', 'run_name')
  def _FailureExists(self, request):
    """Checks to see if a particular failed run exists in GCS.

    This method is run if the command parameter is 'failure_exists'.
      Request Parameters:
        'test_name': the name of the test that failure occurred on.
        'run_name': the name of the particular run that failed.
      Response JSON:
        'exists': boolean indicating whether the failure exists.
        'succeeded': True.

    Args:
      request: a request object.

    Returns:
      A dictionary indicating success or failure, and if the command was
        successful, whether the failed run exists.
    """
    batch_name = request.get('batch_name')
    test_name = request.get('test_name')
    run_name = request.get('run_name')
    return {'success': True,
            'exists': self.manager.FailureExists(
                batch_name, test_name, run_name)}

  @Command('batch_name', 'test_name')
  def _RemoveTest(self, request):
    """Removes a test and associated runs from GCS.

    This method will locate all files in ispy's GCS datastore
      associated with the given test_name, and remove them from GCS.
      This includes the test's mask, expected image, and all failures
      on the given test.

    This method is run if the command parameter is 'remove_test'.
      Request Parameters:
        'test_name': the name of the test to remove.
      Response JSON:
        succeeded: True.

    Args:
      request: a request object.

    Returns:
      A dictionary indicating success or failure.
    """
    batch_name = request.get('batch_name')
    test_name = request.get('test_name')
    self.manager.RemoveTest(batch_name, test_name)
    return {'success': True}

  @Command('batch_name', 'test_name', 'run_name')
  def _RemoveFailure(self, request):
    """Removes a failure from GCS.

    This method is run if the command parameter is 'remove_failure'.
      Request Parameters:
        'test_name': the name of the test in which the failure occurred.
      Response JSON:
        succeeded: True.

    Args:
      request: a request object.

    Returns:
      A dictionary indicating success or failure.
    """
    batch_name = request.get('batch_name')
    test_name = request.get('test_name')
    run_name = request.get('run_name')
    self.manager.RemoveFailure(batch_name, test_name, run_name)
    return {'success': True}
