# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for buildbucket_lib"""

from __future__ import print_function

import json
import mock

from chromite.cbuildbot import buildbucket_lib
from chromite.lib import auth
from chromite.lib import constants
from chromite.lib import cros_test_lib

class BuildbucketClientTest(cros_test_lib.MockTestCase):
  """Tests for BuildbucketClient."""

  def setUp(self):
    self.mock_http = mock.MagicMock()
    self.PatchObject(auth, 'AuthorizedHttp', return_value=self.mock_http)
    self.success_response = {'status': 200}
    self.client = buildbucket_lib.BuildbucketClient()

  def testPutBuildRequest(self):
    """Test PutBuildRequest."""
    buildbucket_id = 'test_buildbucket_id'
    content = json.dumps({
        'build': {
            'id': buildbucket_id
        }
    })
    self.mock_http.request.return_value = (self.success_response, content)

    body = json.dumps({
        'bucket': 'test-bucket',
    })

    result_content = self.client.PutBuildRequest(
        body, False, False)
    self.assertEqual(buildbucket_lib.GetBuildId(result_content), buildbucket_id)

    # Test dryrun
    result_content_2 = self.client.PutBuildRequest(
        body, False, True)
    self.assertIsNone(result_content_2)

  def testGetBuildRequest(self):
    """Test GetBuildRequest."""
    buildbucket_id = 'test_buildbucket_id'
    complete_status = 'COMPLETED'
    content = json.dumps({
        'build': {
            'id': buildbucket_id,
            'status': complete_status
        }
    })
    self.mock_http.request.return_value = (self.success_response, content)
    result_content = self.client.GetBuildRequest(
        buildbucket_id, False, False)
    self.assertEqual(buildbucket_lib.GetBuildStatus(result_content),
                     complete_status)

    # Test dryrun
    result_content_2 = self.client.GetBuildRequest(
        buildbucket_id, False, True)
    self.assertIsNone(result_content_2)

  def testCancelBuildRequest(self):
    """Test CancelBuild."""
    buildbucket_id = 'test_buildbucket_id'
    result = 'CANCELED'
    content = json.dumps({
        'build':{
            'id': buildbucket_id,
            'result': result
        }
    })
    self.mock_http.request.return_value = (self.success_response, content)
    result_content = self.client.CancelBuildRequest(
        buildbucket_id, False, False)
    self.assertEqual(buildbucket_lib.GetBuildResult(result_content), result)

    # Test dryrun
    result_content_2 = self.client.CancelBuildRequest(
        buildbucket_id, False, True)
    self.assertIsNone(result_content_2)

  def testCancelBatchBuildsRequest(self):
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
    self.mock_http.request.return_value = (self.success_response, content)
    result_content = self.client.CancelBatchBuildsRequest(
        [buildbucket_id_1, buildbucket_id_2], False, False)

    result_map = buildbucket_lib.GetResultMap(result_content)
    self.assertIsNotNone(buildbucket_lib.GetNestedAttr(
        result_map[buildbucket_id_1], ['build']))
    self.assertIsNotNone(buildbucket_lib.GetNestedAttr(
        result_map[buildbucket_id_2], ['error']))
    self.assertEqual(buildbucket_lib.GetBuildStatus(
        result_map[buildbucket_id_1]), status)
    self.assertEqual(buildbucket_lib.GetErrorReason(
        result_map[buildbucket_id_2]), error_reason)

    # Test dryrun
    result_content_2 = self.client.CancelBatchBuildsRequest(
        [buildbucket_id_1, buildbucket_id_2], False, True)
    self.assertIsNone(result_content_2)

  def testRetryBuildRequest(self):
    """Test RetryBuild."""
    buildbucket_id = '001'
    buildbucket_id_2 = '002'

    content = json.dumps({
        'build':{
            'status': 'SCHEDULED',
            'id': buildbucket_id_2,
            'retry_of': buildbucket_id
        }
    })

    self.mock_http.request.return_value = (self.success_response, content)
    result_content = self.client.RetryBuildRequest(
        buildbucket_id, False, False)
    self.assertEqual(buildbucket_lib.GetBuildId(result_content),
                     buildbucket_id_2)

    reason = 'BUILD_NOT_FOUND'
    message = 'Build 001 not found'
    content = json.dumps({
        'error': {
            'message': message,
            'reason': reason
        }
    })

    self.mock_http.request.return_value = (self.success_response, content)
    result_content = self.client.RetryBuildRequest(
        buildbucket_id, False, False)
    self.assertEqual(buildbucket_lib.GetErrorReason(result_content),
                     reason)
    self.assertEqual(buildbucket_lib.GetErrorMessage(result_content),
                     message)

    # Test dryrun
    result_content = self.client.RetryBuildRequest(
        buildbucket_id, False, True)
    self.assertIsNone(result_content)

  def testSearchBuildsRequest(self):
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

    self.mock_http.request.return_value = (self.success_response, content)
    result_content = self.client.SearchBuildsRequest(
        False,
        False,
        buckets=[constants.TRYSERVER_BUILDBUCKET_BUCKET,],
        tags=['build_type:tryjob', 'bot_id:build265-m2'],
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED,
        start_cursor='start_cursor',
        max_builds=10)

    build_ids = buildbucket_lib.GetBuildIds(result_content)
    self.assertTrue(buildbucket_id_1 in build_ids and
                    buildbucket_id_2 in build_ids)

     # Test dryrun
    result_content_2 = self.client.SearchBuildsRequest(
        False,
        True,
        buckets=[constants.TRYSERVER_BUILDBUCKET_BUCKET,
                 constants.CHROMEOS_BUILDBUCKET_BUCKET,],
        tags=['build_type:tryjob'],
        max_builds=100)
    self.assertEqual(result_content_2, None)

  def _MockSearchContent(self, *args, **kwargs):
    """Mock searched content."""
    dryrun = args[1]
    if dryrun:
      return

    next_cursor = kwargs.get('next_cursor', 'next_cursor')
    max_builds = kwargs.get('max_builds', 0)
    builds = [{'status': 'COMPLETE', 'id': str(i)}
              for i in range(max_builds)]
    content_dict = {
        'kind': 'kind',
        'etag': 'etag',
        'builds': builds
    }

    if next_cursor:
      content_dict.update({'next_cursor': next_cursor})

    return content_dict

  def _MockSearchContentWithCursor(self, *args, **kwargs):
    """Mock searched content with next_cursor."""
    kwargs.setdefault('next_cursor', 'next_cursor')
    return self._MockSearchContent(*args, **kwargs)

  def _SearchAllBuildsWithCursor(self, limit):
    """Test for SearchAllBuildsWithCursor."""
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'SearchBuildsRequest',
                     side_effect=self._MockSearchContentWithCursor)

    return self.client.SearchAllBuilds(
        False,
        False,
        limit=limit,
        buckets=[constants.TRYSERVER_BUILDBUCKET_BUCKET,],
        tags=['build_type:tryjob', 'bot_id:build265-m2'],
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED)

  def testSearchAllBuildsWithCursor(self):
    """Test SearchAllBuilds with next_cursor."""

    for limit in (1, 2, 5, 100, 101, 150, 200):
      all_builds = self._SearchAllBuildsWithCursor(limit)
      self.assertEqual(len(all_builds), limit)

  def testSearchAllBuildsWithoutCursor(self):
    """Test SearchAllBuilds without next_cursor."""
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'SearchBuildsRequest',
                     side_effect=self._MockSearchContent)

    all_builds = self.client.SearchAllBuilds(False, False, limit=2)
    self.assertEqual(len(all_builds), 2)

    max_builds = 5
    builds = [{'status': 'COMPLETE', 'id': str(i)}
              for i in range(max_builds)]
    content_dict = {
        'kind': 'kind',
        'etag': 'etag',
        'builds': builds
    }
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'SearchBuildsRequest', return_value=content_dict)

    all_builds = self.client.SearchAllBuilds(False, False)
    self.assertEqual(len(all_builds), max_builds)

    all_builds = self.client.SearchAllBuilds(False, False, limit=50)
    self.assertEqual(len(all_builds), max_builds)


class GetAttributeTest(cros_test_lib.MockTestCase):
  """Test cases for getting attributes."""

  def testGetNestedAttr(self):
    """Test GetNestedAttr."""
    buildbucket_id_1 = 'test_buildbucket_id_1'
    buildbucket_id_2 = 'test_buildbucket_id_2'
    content = {
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
        'result': {
            'status': 'COMPLETED'
        }
    }

    etag = buildbucket_lib.GetNestedAttr(
        content, ['etag'], default='default_etag')
    self.assertEqual(etag, 'etag')

    status = buildbucket_lib.GetNestedAttr(
        content, ['result', 'status'])
    self.assertEqual('COMPLETED', status)

    test_attr = buildbucket_lib.GetNestedAttr(
        content, ['test_attr'], default='default_test_attr')
    self.assertEqual(test_attr, 'default_test_attr')

    test_etag_attr = buildbucket_lib.GetNestedAttr(
        content, ['etag', 'test_attr'], default='default_etag_test_attr')
    self.assertEqual(test_etag_attr, 'default_etag_test_attr')

    build_ids = buildbucket_lib.ExtractBuildIds(
        buildbucket_lib.GetNestedAttr(content, ['builds'], default=[]))
    self.assertTrue(buildbucket_id_1 in build_ids and
                    buildbucket_id_2 in build_ids)

    build_ids = buildbucket_lib.GetBuildIds(content)
    self.assertTrue(buildbucket_id_1 in build_ids and
                    buildbucket_id_2 in build_ids)

    content = {
        'results':[{
            'build_id': buildbucket_id_1,
            'error': {
                'message': "Cannot cancel a completed build",
                'reason': 'error_reason',
            }
        }]
    }
    results = buildbucket_lib.GetNestedAttr(
        content, ['results'], default=[])

    for r in results:
      reason = buildbucket_lib.GetErrorReason(r)
      self.assertEqual(reason, 'error_reason')
