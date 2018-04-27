# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code related to Remote tryjobs."""

from __future__ import print_function

import collections
import json

from chromite.lib import auth
from chromite.lib import buildbucket_lib
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging

site_config = config_lib.GetConfig()


# URL to open a build details page.
BUILD_DETAILS_PATTERN = (
    'http://cros-goldeneye/chromeos/healthmonitoring/buildDetails?'
    'buildbucketId=%(buildbucket_id)s'
)


class RemoteRequestFailure(Exception):
  """Thrown when requesting a tryjob fails."""


# Contains the results of a single scheduled build.
ScheduledBuild = collections.namedtuple(
    'ScheduledBuild', ('buildbucket_id', 'build_config', 'url'))


def TryJobUrl(buildbucket_id):
  """Get link to the build UI for a given build.

  Returns:
    The URL as a string to view the given build.
  """
  return BUILD_DETAILS_PATTERN % {'buildbucket_id': buildbucket_id}


class RemoteTryJob(object):
  """Remote Tryjob that is submitted through a Git repo."""
  # Buildbucket_put response must contain 'buildbucket_bucket:bucket]',
  # '[config:config_name] and '[buildbucket_id:id]'.
  BUILDBUCKET_PUT_RESP_FORMAT = ('Successfully sent PUT request to '
                                 '[buildbucket_bucket:%s] '
                                 'with [config:%s] [buildbucket_id:%s].')

  def __init__(self,
               build_config,
               display_label,
               branch,
               extra_args,
               user_email,
               master_buildbucket_id):
    """Construct the object.

    Args:
      build_config: A list of configs to run tryjobs for.
      display_label: String describing how build group on waterfall.
      branch: Name of branch to build for.
      extra_args: Command line arguments to pass to cbuildbot in job.
      user_email: Email address of person requesting job, or None.
      master_buildbucket_id: String with buildbucket id of scheduling builder.
    """
    self.build_config = build_config
    self.display_label = display_label
    self.branch = branch
    self.extra_args = extra_args
    self.user_email = user_email
    self.master_buildbucket_id = master_buildbucket_id

    logging.info('Using email:%s', self.user_email)

  def Submit(self, testjob=False, dryrun=False):
    """Submit the tryjob through Git.

    Args:
      testjob: Submit job to the test branch of the tryjob repo.  The tryjob
               will be ignored by production master.
      dryrun: Setting to true will run everything except the final submit step.

    Returns:
      A ScheduledBuild instance.
    """
    host = (buildbucket_lib.BUILDBUCKET_TEST_HOST if testjob
            else buildbucket_lib.BUILDBUCKET_HOST)
    buildbucket_client = buildbucket_lib.BuildbucketClient(
        auth.GetAccessToken, host,
        service_account_json=buildbucket_lib.GetServiceAccount(
            constants.CHROMEOS_SERVICE_ACCOUNT))

    return self._PutConfigToBuildBucket(
        buildbucket_client, self.build_config, dryrun)

  def _GetRequestBody(self, bot):
    """Generate the request body for a swarming buildbucket request.

    Args:
      bot: The bot config to put.

    Returns:
      buildbucket request properties as a python dict.
    """
    bucket = constants.INTERNAL_SWARMING_BUILDBUCKET_BUCKET

    tags = {
        'cbb_display_label': self.display_label,
        'cbb_branch': self.branch,
        'cbb_config': bot,
        'cbb_email': self.user_email,
        'cbb_master_build_id': self.master_buildbucket_id,
    }

    # Don't include tags with no value, there is no point.
    tags = {k: v for k, v in tags.iteritems() if v}

    # All tags should also be listed as properties.
    properties = tags.copy()
    properties['cbb_extra_args'] = self.extra_args

    luci_builder = config_lib.LUCI_BUILDER_TRY
    if bot in site_config:
      luci_builder = site_config[bot].luci_builder

    parameters = {
        'builder_name': luci_builder,
        'properties': properties,
    }

    if self.user_email:
      parameters['email_notify'] = [{'email': self.user_email}]

    return {
        'bucket': bucket,
        'parameters_json': json.dumps(parameters, sort_keys=True),
        # These tags are indexed and searchable in buildbucket.
        'tags': ['%s:%s' % (k, tags[k]) for k in sorted(tags.keys())],
    }

  def _PutConfigToBuildBucket(self, buildbucket_client, bot, dryrun):
    """Put the tryjob request to buildbucket.

    Args:
      buildbucket_client: The buildbucket client instance.
      bot: The bot config to put.
      dryrun: bool controlling dryrun behavior.

    Returns:
      ScheduledBuild describing the scheduled build.

    Raises:
      RemoteRequestFailure.
    """
    request_body = self._GetRequestBody(bot)
    content = buildbucket_client.PutBuildRequest(
        json.dumps(request_body), dryrun)

    if buildbucket_lib.GetNestedAttr(content, ['error']):
      raise RemoteRequestFailure(
          'buildbucket error.\nReason: %s\n Message: %s' %
          (buildbucket_lib.GetErrorReason(content),
           buildbucket_lib.GetErrorMessage(content)))

    buildbucket_id = buildbucket_lib.GetBuildId(content)
    logging.info(self.BUILDBUCKET_PUT_RESP_FORMAT,
                 (constants.TRYSERVER_BUILDBUCKET_BUCKET, bot, buildbucket_id))

    return ScheduledBuild(buildbucket_id, bot, TryJobUrl(buildbucket_id))
