# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for buildbucket_lib"""

from __future__ import print_function

import json
import mock

from chromite.cbuildbot import buildbucket_lib
from chromite.lib import cros_test_lib

class BuildbucketLibTest(cros_test_lib.TestCase):
  """Tests for buildbucket_lib."""

  def setUp(self):
    self.http = mock.MagicMock()
    self.success_response = {'status': 200}

  def testPutBuild(self):
    """Test PutBuild."""
    buildbucket_id = 'test_buildbucket_id'
    content = json.dumps({
        'build': {
            'id': buildbucket_id
        }
    })
    self.http.request.return_value = (self.success_response, content)
    body = json.dumps({
        'bucket': 'test-bucket',
    })

    result_content = buildbucket_lib.PutBuildBucket(
        body, self.http, False, False)
    self.assertEqual(buildbucket_lib.GetBuildId(result_content), buildbucket_id)

    # Test dryrun
    result_content_2 = buildbucket_lib.PutBuildBucket(
        body, self.http, False, True)
    self.assertIsNone(result_content_2)

  def testGetBuild(self):
    """Test GetBuild."""
    buildbucket_id = 'test_buildbucket_id'
    complete_status = 'COMPLETED'
    content = json.dumps({
        'build': {
            'id': buildbucket_id,
            'status': complete_status
        }
    })
    self.http.request.return_value = (self.success_response, content)
    result_content = buildbucket_lib.GetBuildBucket(
        buildbucket_id, self.http, False, False)
    self.assertEqual(buildbucket_lib.GetBuildStatus(result_content),
                     complete_status)

    # Test dryrun
    result_content_2 = buildbucket_lib.GetBuildBucket(
        buildbucket_id, self.http, False, True)
    self.assertIsNone(result_content_2)

  def testCancelBuild(self):
    """Test CancelBuild."""
    buildbucket_id = 'test_buildbucket_id'
    result = 'CANCELED'
    content = json.dumps({
        'build':{
            'id': buildbucket_id,
            'result': result
        }
    })
    self.http.request.return_value = (self.success_response, content)
    result_content = buildbucket_lib.CancelBuildBucket(
        buildbucket_id, self.http, False, False)
    self.assertEqual(buildbucket_lib.GetBuildResult(result_content), result)

    # Test dryrun
    result_content_2 = buildbucket_lib.CancelBuildBucket(
        buildbucket_id, self.http, False, True)
    self.assertIsNone(result_content_2)

  def testCancelBatchBuilds(self):
    """Test CancelBatchBuilds."""
    buildbucket_id_1 = 'test_buildbucket_id_1'
    buildbucket_id_2 = 'test_buildbucket_id_2'
    status = 'COMPLETED'
    result = 'CANCELED'
    error_reason = 'BUILD_IS_COMPLETED'
    content = json.dumps({
        'results':[{
            'build_id': buildbucket_id_1,
            'build': {
                'status': status,
                'result': result,
            }
        }, {
            'build_id': buildbucket_id_2,
            'error': {
                'message': "Cannot cancel a completed build",
                'reason': error_reason,
            }
        }]
    })
    self.http.request.return_value = (self.success_response, content)
    result_content = buildbucket_lib.CancelBatchBuildBucket(
        [buildbucket_id_1, buildbucket_id_2], self.http, False, False)

    result_map = buildbucket_lib.GetResultMap(result_content)
    self.assertTrue(buildbucket_lib.HasBuild(result_map[buildbucket_id_1]))
    self.assertTrue(buildbucket_lib.HasError(result_map[buildbucket_id_2]))
    self.assertEqual(buildbucket_lib.GetBuildStatus(
        result_map[buildbucket_id_1]), status)
    self.assertEqual(buildbucket_lib.GetErrorReason(
        result_map[buildbucket_id_2]), error_reason)

    # Test dryrun
    result_content_2 = buildbucket_lib.CancelBatchBuildBucket(
        [buildbucket_id_1, buildbucket_id_2], self.http, False, True)
    self.assertIsNone(result_content_2)
