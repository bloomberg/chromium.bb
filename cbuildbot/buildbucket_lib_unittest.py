# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for buildbucket_lib"""

from __future__ import print_function

import json
import mock

from chromite.cbuildbot import buildbucket_lib
from chromite.cbuildbot import constants
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

  def testSearchBuilds(self):
    """Test SearchBuilds."""
    buildbucket_id_1 = 'test_buildbucket_id_1'
    buildbucket_id_2 = 'test_buildbucket_id_2'

    content = json.dumps({
        'kind': 'kind',
        'etag': 'etag',
        'builds': [{
            'bucket': 'master.chromiumios.tryserver',
            'status': 'COMPLETED',
            'id': buildbucket_id_1,
            'tags':[
                'bot_id:build265-m2',
                'build_type:tryjob',]
        }, {
            'bucket': 'master.chromiumios.tryserver',
            'status': 'COMPLETED',
            'id': buildbucket_id_2,
            'tags':[
                'bot_id:build265-m2',
                'build_type:tryjob']
        }],
    })

    self.http.request.return_value = (self.success_response, content)
    result_content = buildbucket_lib.SearchBuildBucket(
        self.http, False, False,
        buckets=[constants.TRYSERVER_BUILDBUCKET_BUCKET,],
        tags=['build_type:tryjob', 'bot_id:build265-m2'],
        status=buildbucket_lib.COMPLETED_STATUS)

    build_ids = buildbucket_lib.GetBuildIds(result_content)
    self.assertTrue(buildbucket_id_1 in build_ids and
                    buildbucket_id_2 in build_ids)

     # Test dryrun
    result_content_2 = buildbucket_lib.SearchBuildBucket(
        self.http, False, True,
        buckets=[constants.TRYSERVER_BUILDBUCKET_BUCKET,
                 constants.CHROMEOS_BUILDBUCKET_BUCKET,],
        tags=['build_type:tryjob'])
    self.assertEqual(result_content_2, None)

  def testSearchAllBuilds(self):
    """Test SearchAllBuilds."""
    buildbucket_id_1 = 'test_buildbucket_id_1'
    buildbucket_id_2 = 'test_buildbucket_id_2'

    # Test results with next_cursor.
    content = json.dumps({
        'next_cursor': 'next_cursor',
        'kind': 'kind',
        'etag': 'etag',
        'builds': [{
            'status': 'COMPLETED',
            'id': buildbucket_id_1,
        }, {
            'status': 'COMPLETED',
            'id': buildbucket_id_2,
        }],
    })
    self.http.request.return_value = (self.success_response, content)

    all_builds = buildbucket_lib.SearchAllBuilds(
        self.http, False, False,
        limit=1,
        buckets=[constants.TRYSERVER_BUILDBUCKET_BUCKET,],
        tags=['build_type:tryjob', 'bot_id:build265-m2'],
        status=buildbucket_lib.COMPLETED_STATUS)
    self.assertEqual(len(all_builds), 1)

    all_builds = buildbucket_lib.SearchAllBuilds(
        self.http, False, False,
        limit=3,
        buckets=[constants.TRYSERVER_BUILDBUCKET_BUCKET,],
        tags=['build_type:tryjob', 'bot_id:build265-m2'])
    self.assertEqual(len(all_builds), 3)

    all_builds = buildbucket_lib.SearchAllBuilds(
        self.http, False, False,
        limit=4,
        buckets=[constants.TRYSERVER_BUILDBUCKET_BUCKET,],
        tags=['build_type:tryjob',],
        status=buildbucket_lib.COMPLETED_STATUS)
    self.assertEqual(len(all_builds), 4)

    # Test results without next_cursor.
    content = json.dumps({
        'kind': 'kind',
        'etag': 'etag',
        'builds': [{
            'bucket': 'master.chromiumios.tryserver',
            'status': 'COMPLETED',
            'id': buildbucket_id_1,
        }, {
            'bucket': 'master.chromiumios.tryserver',
            'status': 'COMPLETED',
            'id': buildbucket_id_2,
        }],
    })
    self.http.request.return_value = (self.success_response, content)

    all_builds = buildbucket_lib.SearchAllBuilds(
        self.http, False, False)
    self.assertEqual(len(all_builds), 2)

    content = json.dumps({
        'kind': 'kind',
        'etag': 'etag',
    })
    self.http.request.return_value = (self.success_response, content)

    all_builds = buildbucket_lib.SearchAllBuilds(
        self.http, False, False)
    self.assertEqual(len(all_builds), 0)
