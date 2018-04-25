# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code related to Remote tryjobs."""

from __future__ import print_function

import collections
import getpass
import json
import os
import time

from chromite.lib import auth
from chromite.lib import buildbucket_lib
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import git

site_config = config_lib.GetConfig()


# URL to open a build details page.
BUILD_DETAILS_PATTERN = (
    'http://cros-goldeneye/chromeos/healthmonitoring/buildDetails?'
    'buildbucketId=%(buildbucket_id)s'
)


class ValidationError(Exception):
  """Thrown when tryjob validation fails."""


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
  # Constants for controlling the length of JSON fields sent to buildbot.
  # - The trybot description is shown when the run starts, and helps users
  #   distinguish between their various runs. If no trybot description is
  #   specified, the list of patches is used as the description. The buildbot
  #   database limits this field to MAX_DESCRIPTION_LENGTH characters.
  # - When checking the trybot description length, we also add some PADDING
  #   to give buildbot room to add extra formatting around the fields used in
  #   the description.
  # - We limit the number of patches listed in the description to
  #   MAX_PATCHES_IN_DESCRIPTION. This is for readability only.
  # - Every individual field that is stored in a buildset is limited to
  #   MAX_PROPERTY_LENGTH. We use this to ensure that our serialized list of
  #   arguments fits within that limit.
  MAX_DESCRIPTION_LENGTH = 256
  MAX_PATCHES_IN_DESCRIPTION = 10
  MAX_PROPERTY_LENGTH = 1023
  PADDING = 50

  # Buildbucket_put response must contain 'buildbucket_bucket:bucket]',
  # '[config:config_name] and '[buildbucket_id:id]'.
  BUILDBUCKET_PUT_RESP_FORMAT = ('Successfully sent PUT request to '
                                 '[buildbucket_bucket:%s] '
                                 'with [config:%s] [buildbucket_id:%s].')

  def __init__(self,
               build_config,
               display_label,
               branch='master',
               pass_through_args=(),
               local_patches=(),
               committer_email=None,
               master_buildbucket_id=''):
    """Construct the object.

    Args:
      build_config: A list of configs to run tryjobs for.
      display_label: String describing how build group on waterfall.
      branch: Name of branch to build for.
      pass_through_args: Command line arguments to pass to cbuildbot in job.
      local_patches: A list of LocalPatch objects.
      committer_email: Email address of person requesting job, or None.
      master_buildbucket_id: String with buildbucket id of scheduling builder.
    """
    self.user = getpass.getuser()
    if committer_email is not None:
      self.user_email = committer_email
    else:
      cwd = os.path.dirname(os.path.realpath(__file__))
      self.user_email = git.GetProjectUserEmail(cwd)
    logging.info('Using email:%s', self.user_email)

    # Name of the job that appears on the waterfall.
    self.build_config = build_config
    self.display_label = display_label
    self.extra_args = pass_through_args
    self.branch = branch
    self.local_patches = local_patches
    self.master_buildbucket_id = master_buildbucket_id

    # Needed for handling local patches.
    self.manifest = git.ManifestCheckout.Cached(constants.SOURCE_ROOT)

  def Submit(self, testjob=False, dryrun=False):
    """Submit the tryjob through Git.

    Args:
      testjob: Submit job to the test branch of the tryjob repo.  The tryjob
               will be ignored by production master.
      dryrun: Setting to true will run everything except the final submit step.

    Returns:
      A ScheduledBuild instance.
    """
    # TODO(rcui): convert to shallow clone when that's available.
    current_time = str(int(time.time()))

    ref_base = os.path.join('refs/tryjobs', self.user_email, current_time)
    for patch in self.local_patches:
      # Isolate the name; if it's a tag or a remote, let through.
      # Else if it's a branch, get the full branch name minus refs/heads.
      local_branch = git.StripRefsHeads(patch.ref, False)
      ref_final = os.path.join(ref_base, local_branch, patch.sha1)

      checkout = patch.GetCheckout(self.manifest)
      checkout.AssertPushable()
      print('Uploading patch %s' % patch)
      patch.Upload(checkout['push_url'], ref_final, dryrun=dryrun)

      # TODO(rcui): Pass in the remote instead of tag. http://crosbug.com/33937.
      tag = constants.EXTERNAL_PATCH_TAG
      if checkout['remote'] == site_config.params.INTERNAL_REMOTE:
        tag = constants.INTERNAL_PATCH_TAG

      self.extra_args.append('--remote-patches=%s:%s:%s:%s:%s'
                             % (patch.project, local_branch, ref_final,
                                patch.tracking_branch, tag))

    return self._PostConfigToBuildBucket(testjob, dryrun)

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

  def _PostConfigToBuildBucket(self, testjob=False, dryrun=False):
    """Posts the tryjob configs to buildbucket.

    Args:
      dryrun: Whether to skip the request to buildbucket.
      testjob: Whether to use the test instance of the buildbucket server.

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
