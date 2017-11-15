# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for remote_try.py."""

from __future__ import print_function

import json
import mock

from chromite.lib import auth
from chromite.lib import buildbucket_lib
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import remote_try

# Tests need internal access.
# pylint: disable=protected-access

class RemoteTryHelperTestsBase(cros_test_lib.MockTestCase):
  """Tests for RemoteTryJob."""
  BRANCH = 'test-branch'
  PATCHES = ('5555', '6666')
  BUILD_CONFIGS = ('amd64-generic-paladin', 'arm-generic-paladin')
  UNKNOWN_CONFIGS = ('unknown-config')
  BUILD_GROUP = 'display'
  PASS_THROUGH_ARGS = ['funky', 'cold', 'medina']
  TEST_EMAIL = 'explicit_email'
  MASTER_BUILDBUCKET_ID = 'master_bb_id'

  def setUp(self):
    self.maxDiff = None
    self.PatchObject(git, 'GetProjectUserEmail', return_value='default_email')

  def _CreateJobMin(self):
    return remote_try.RemoteTryJob(
        self.BUILD_CONFIGS,
        self.BUILD_GROUP,
        'description')

  def _CreateJobMax(self):
    return remote_try.RemoteTryJob(
        self.BUILD_CONFIGS,
        self.BUILD_GROUP,
        'description',
        branch=self.BRANCH,
        pass_through_args=self.PASS_THROUGH_ARGS,
        local_patches=(),  # TODO: Populate/test these, somehow.
        committer_email=self.TEST_EMAIL,
        swarming=True,
        master_buildbucket_id=self.MASTER_BUILDBUCKET_ID)

  def _CreateJobUnknown(self):
    return remote_try.RemoteTryJob(
        self.UNKNOWN_CONFIGS,
        self.BUILD_GROUP,
        'description')


class RemoteTryHelperTestsMock(RemoteTryHelperTestsBase):
  """Perform real buildbucket requests against a fake instance."""

  def setUp(self):
    # This mocks out the class, then creates a return_value for a function on
    # instances of it. We do this instead of just mocking out the function to
    # ensure not real network requests are made in other parts of the class.
    client_mock = self.PatchObject(buildbucket_lib, 'BuildbucketClient')
    client_mock().PutBuildRequest.return_value = {
        'build': {'id': 'fake_buildbucket_id'}
    }

  def testDefaultDescription(self):
    """Verify job description formatting."""
    description = remote_try.DefaultDescription(self.BRANCH, self.PATCHES)
    self.assertEqual(description, '[test-branch] 5555,6666')

  def testDefaultEmail(self):
    """Verify a default discovered email address."""
    job = self._CreateJobMin()

    # Verify default email detection.
    self.assertEqual(job.user_email, 'default_email')

  def testExplicitEmail(self):
    """Verify an explicitly set email address."""
    job = self._CreateJobMax()

    # Verify explicit email detection.
    self.assertEqual(job.user_email, self.TEST_EMAIL)

  def testMinRequestBody(self):
    """Verify our request body with min options."""
    body = self._CreateJobMin()._GetRequestBody(self.BUILD_CONFIGS[0])

    self.assertEqual(body, {
        'parameters_json': mock.ANY,
        'bucket': 'master.chromiumos.tryserver',
        'tags': [
            'cbb_display_label:display',
            'cbb_branch:master',
            'cbb_config:amd64-generic-paladin',
            'cbb_master_build_id:',
            'cbb_email:default_email',
        ]
    })

    parameters_parsed = json.loads(body['parameters_json'])

    self.assertEqual(parameters_parsed, {
        'builder_name': 'Generic',
        'properties': {
            'extra_args': [],
            'cbb_extra_args': [],
            'name': 'description',
            'owners': ['default_email'],
            'bot': ['amd64-generic-paladin', 'arm-generic-paladin'],
            'email': ['default_email'],
            'cbb_config': 'amd64-generic-paladin',
            'user': mock.ANY,
        }
    })

  def testMaxRequestBody(self):
    """Verify our request body with max options."""
    self.maxDiff = None
    body = self._CreateJobMax()._GetRequestBody(self.BUILD_CONFIGS[0])

    self.assertEqual(body, {
        'parameters_json': mock.ANY,
        'bucket': 'luci.chromeos.general',
        'tags': [
            'cbb_display_label:display',
            'cbb_branch:test-branch',
            'cbb_config:amd64-generic-paladin',
            'cbb_master_build_id:master_bb_id',
            'cbb_email:explicit_email',
        ]
    })

    parameters_parsed = json.loads(body['parameters_json'])

    self.assertEqual(parameters_parsed, {
        'builder_name': 'Generic',
        'properties': {
            'extra_args': ['funky', 'cold', 'medina'],
            'cbb_extra_args': ['funky', 'cold', 'medina'],
            'name': 'description',
            'owners': ['explicit_email'],
            'bot': ['amd64-generic-paladin', 'arm-generic-paladin'],
            'email': ['explicit_email'],
            'cbb_config': 'amd64-generic-paladin',
            'user': mock.ANY,
        }
    })

  def testUnknownRequestBody(self):
    """Verify our request body with max options."""
    self.maxDiff = None
    body = self._CreateJobUnknown()._GetRequestBody('unknown-config')

    self.assertEqual(body, {
        'parameters_json': mock.ANY,
        'bucket': 'master.chromiumos.tryserver',
        'tags': [
            'cbb_display_label:display',
            'cbb_branch:master',
            'cbb_config:unknown-config',
            'cbb_master_build_id:',
            'cbb_email:default_email',
        ]
    })

    parameters_parsed = json.loads(body['parameters_json'])

    self.assertEqual(parameters_parsed, {
        'builder_name': 'Generic',
        'properties': {
            'extra_args': [],
            'cbb_extra_args': [],
            'name': 'description',
            'owners': ['default_email'],
            'bot': 'unknown-config',
            'email': ['default_email'],
            'cbb_config': 'unknown-config',
            'user': mock.ANY,
        }
    })

  def testMinDryRun(self):
    """Do a dryrun of posting the request, min options."""
    job = self._CreateJobMin()
    job._PostConfigsToBuildBucket(testjob=True, dryrun=True)
    # TODO: Improve coverage to all of Submit. Verify behavior.

  def testMaxDryRun(self):
    """Do a dryrun of posting the request, max options."""
    job = self._CreateJobMax()
    job._PostConfigsToBuildBucket(testjob=True, dryrun=True)
    # TODO: Improve coverage to all of Submit. Verify behavior.


class RemoteTryHelperTestsNetork(RemoteTryHelperTestsBase):
  """Perform real buildbucket requests against a test instance."""

  def verifyBuildbucketRequest(self,
                               buildbucket_id,
                               expected_bucket,
                               expected_tags,
                               expected_parameters):
    """Verify the contents of a push to the TEST buildbucket instance.

    Args:
      buildbucket_id: Id to verify.
      expected_bucket: Bucket the push was supposed to go to as a string.
      expected_tags: List of buildbucket tags as strings.
      expected_parameters: Python dict equivalent to json string in
                           parameters_json.
    """
    buildbucket_client = buildbucket_lib.BuildbucketClient(
        auth.GetAccessToken, buildbucket_lib.BUILDBUCKET_TEST_HOST,
        service_account_json=buildbucket_lib.GetServiceAccount(
            constants.CHROMEOS_SERVICE_ACCOUNT))

    request = buildbucket_client.GetBuildRequest(buildbucket_id, False)

    self.assertEqual(request['build']['id'], buildbucket_id)
    self.assertEqual(request['build']['bucket'], expected_bucket)
    self.assertItemsEqual(request['build']['tags'], expected_tags)

    request_parameters = json.loads(request['build']['parameters_json'])
    self.assertEqual(request_parameters, expected_parameters)

  @cros_test_lib.NetworkTest()
  def testMinTestBucket(self):
    """Talk to a test buildbucket instance with min job settings."""
    # Submit jobs
    job = self._CreateJobMin()
    job.Submit(testjob=True)
    buildbucket_ids = job.buildbucket_ids

    self.verifyBuildbucketRequest(
        buildbucket_ids[0],
        'master.chromiumos.tryserver',
        [
            'builder:Generic',
            'cbb_branch:master',
            'cbb_display_label:display',
            'cbb_config:amd64-generic-paladin',
            'cbb_email:default_email',
            'cbb_master_build_id:',
        ],
        {
            'builder_name': 'Generic',
            'properties': {
                'bot': ['amd64-generic-paladin', 'arm-generic-paladin'],
                'cbb_config': 'amd64-generic-paladin',
                'cbb_extra_args': [],
                'email': ['default_email'],
                'extra_args': [],
                'name': 'description',
                'owners': ['default_email'],
                'user': mock.ANY,
            },
        })

    self.verifyBuildbucketRequest(
        buildbucket_ids[1],
        'master.chromiumos.tryserver',
        [
            'builder:Generic',
            'cbb_branch:master',
            'cbb_display_label:display',
            'cbb_config:arm-generic-paladin',
            'cbb_email:default_email',
            'cbb_master_build_id:',
        ],
        {
            'builder_name': 'Generic',
            'properties': {
                'bot': ['amd64-generic-paladin', 'arm-generic-paladin'],
                'cbb_config': 'arm-generic-paladin',
                'cbb_extra_args': [],
                'email': ['default_email'],
                'extra_args': [],
                'name': 'description',
                'owners': ['default_email'],
                'user': mock.ANY,
            },
        })

    # Verify live URLs.
    job_links = job.GetTrybotWaterfallLinks()
    self.assertEqual(job_links, [
        ('https://uberchromegw.corp.google.com/i/chromiumos.tryserver/'
         'waterfall?committer=default_email&builder=paladin'),
    ])

  @cros_test_lib.NetworkTest()
  def testMaxTestBucket(self):
    """Talk to a test buildbucket instance with max job settings."""
    # Submit jobs
    job = self._CreateJobMax()
    job.Submit(testjob=True)
    buildbucket_ids = job.buildbucket_ids

    # Verify buildbucket contents.
    self.verifyBuildbucketRequest(
        buildbucket_ids[0],
        'luci.chromeos.general',
        [
            'builder:Generic',
            'cbb_branch:test-branch',
            'cbb_display_label:display',
            'cbb_config:amd64-generic-paladin',
            'cbb_email:explicit_email',
            'cbb_master_build_id:master_bb_id',
        ],
        {
            'builder_name': 'Generic',
            'properties': {
                'bot': ['amd64-generic-paladin', 'arm-generic-paladin'],
                'cbb_config': 'amd64-generic-paladin',
                'cbb_extra_args': ['funky', 'cold', 'medina'],
                'email': ['explicit_email'],
                'extra_args': ['funky', 'cold', 'medina'],
                'name': 'description',
                'owners': ['explicit_email'],
                'user': mock.ANY,
            },
        })

    self.verifyBuildbucketRequest(
        buildbucket_ids[1],
        'luci.chromeos.general',
        [
            'builder:Generic',
            'cbb_branch:test-branch',
            'cbb_display_label:display',
            'cbb_config:arm-generic-paladin',
            'cbb_email:explicit_email',
            'cbb_master_build_id:master_bb_id',
        ],
        {
            'builder_name': 'Generic',
            'properties': {
                'bot': ['amd64-generic-paladin', 'arm-generic-paladin'],
                'cbb_config': 'arm-generic-paladin',
                'cbb_extra_args': ['funky', 'cold', 'medina'],
                'email': ['explicit_email'],
                'extra_args': ['funky', 'cold', 'medina'],
                'name': 'description',
                'owners': ['explicit_email'],
                'user': mock.ANY,
            },
        })

    # Verify live URLs.
    job_links = job.GetTrybotWaterfallLinks()

    self.assertEqual(len(buildbucket_ids), len(job_links))
    for buildbucket_id, link in zip(buildbucket_ids, job_links):
      self.assertEqual(
          link,
          ('http://cros-goldeneye/chromeos/healthmonitoring/buildDetails?'
           'buildbucketId=%s' %
           buildbucket_id)
      )

  # pylint: disable=protected-access
  def testPostConfigsToBuildBucket(self):
    """Check syntax for PostConfigsToBuildBucket."""
    self.PatchObject(auth, 'Login')
    self.PatchObject(auth, 'Token')
    self.PatchObject(remote_try.RemoteTryJob, '_PutConfigToBuildBucket')
    remote_try_job = remote_try.RemoteTryJob(
        ['build_config'],
        'display_label',
        remote_description='description')
    remote_try_job._PostConfigsToBuildBucket(testjob=True, dryrun=True)

  def testGetRequestBody(self):
    """Test GetRequestBody."""
    remote_try_job = remote_try.RemoteTryJob(
        ['lumpy-pre-cq', 'lumpy-paladin'],
        'display_label',
        remote_description='description')
    body = remote_try_job._GetRequestBody('lumpy-pre-cq')
    self.assertTrue('build_type:pre_cq' in body['tags'])

    body = remote_try_job._GetRequestBody('lumpy-paladin')
    self.assertFalse('build_type:pre_cq' in body['tags'])
