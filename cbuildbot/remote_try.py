# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code related to Remote tryjobs."""

from __future__ import print_function

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


class ValidationError(Exception):
  """Thrown when tryjob validation fails."""


class RemoteRequestFailure(Exception):
  """Thrown when requesting a tryjob fails."""


def DefaultDescription(description_branch='master', patches=None):
  """Calculate the default description for a tryjob.

  Args:
    description_branch: String name of branch to build.
    patches: List of strings describing all patches includes.
             Usually based on raw command line values.

  Returns:
    str
  """
  result = ''
  if description_branch != 'master':
    result += '[%s] ' % description_branch

  if patches:
    result += ','.join(patches[:RemoteTryJob.MAX_PATCHES_IN_DESCRIPTION])
    if len(patches) > RemoteTryJob.MAX_PATCHES_IN_DESCRIPTION:
      remaining_patches = len(patches) - RemoteTryJob.MAX_PATCHES_IN_DESCRIPTION
      result += '... (%d more CLs)' % (remaining_patches,)

  return result


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

  def __init__(self, build_configs, local_patches,
               pass_through_args,
               remote_description,
               committer_email=None,
               swarming=False):
    """Construct the object.

    Args:
      build_configs: A list of configs to run tryjobs for.
      local_patches: A list of LocalPatch objects.
      pass_through_args: Command line arguments to pass to cbuildbot in job.
      remote_description: Requested tryjob description.
      committer_email: Email address of person requesting job, or None.
      swarming: Boolean, do we use a swarming build?
    """
    self.user = getpass.getuser()
    cwd = os.path.dirname(os.path.realpath(__file__))
    if committer_email is not None:
      self.user_email = committer_email
    else:
      self.user_email = git.GetProjectUserEmail(cwd)
    logging.info('Using email:%s', self.user_email)
    # Name of the job that appears on the waterfall.
    self.name = remote_description
    self.build_configs = build_configs[:]
    self.description = ('name: %s' % self.name)
    self.extra_args = pass_through_args
    self.local_patches = local_patches
    self.manifest = git.ManifestCheckout.Cached(constants.SOURCE_ROOT)
    self.swarming = swarming

  def _VerifyForBuildbot(self):
    """Early validation, to ensure the job can be processed by buildbot."""

    # TODO: Delete this after all tryjobs are on swarming. This restriction
    # will have been lifted.

    # Buildbot stores the trybot description in a property with a 256
    # character limit. Validate that our description is well under the limit.
    if (len(self.user) + len(self.name) + self.PADDING >
        self.MAX_DESCRIPTION_LENGTH):
      logging.warning('remote tryjob description is too long, truncating it')
      self.name = self.name[:self.MAX_DESCRIPTION_LENGTH - self.PADDING] + '...'

    # Buildbot will set extra_args as a buildset 'property'.  It will store
    # the property in its database in JSON form.  The limit of the database
    # field is 1023 characters.
    if len(json.dumps(self.extra_args)) > self.MAX_PROPERTY_LENGTH:
      raise ValidationError(
          'The number of extra arguments passed to cbuildbot has exceeded the '
          'limit.  If you have a lot of local patches, upload them and use the '
          '-g flag instead.')

  def Submit(self, testjob=False, dryrun=False):
    """Submit the tryjob through Git.

    Args:
      testjob: Submit job to the test branch of the tryjob repo.  The tryjob
               will be ignored by production master.
      dryrun: Setting to true will run everything except the final submit step.
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

    self._VerifyForBuildbot()
    self._PostConfigsToBuildBucket(testjob, dryrun)

  def _GetBuilder(self, bot):
    """Find and return the builder for bot."""
    if bot in site_config and site_config[bot]['_template']:
      return site_config[bot]['_template']

    # Default to etc builder.
    return 'etc'

  def _GetRequestProperties(self, bot):
    """Construct and return properties for buildbucket request.

    Args:
      bot: The bot config to put.
    """
    return {
        'bot' : self.build_configs,
        'email' : [self.user_email],
        'extra_args' : self.extra_args,
        'name' : self.name,
        'user' : self.user,
        'cbb_config': bot,
        'cbb_extra_args': self.extra_args,
        'owners': [self.user_email]
    }

  def _GetBuildbotRequestBody(self, bot):
    """Generate the request body for a buildbot buildbucket request.

    Args:
      bot: The bot config to put.

    Returns:
      buildbucket request properties as a python dict.
    """
    properties = self._GetRequestProperties(bot)

    return {
        'bucket': constants.TRYSERVER_BUILDBUCKET_BUCKET,
        'parameters_json': json.dumps({
            'builder_name': self._GetBuilder(bot),
            'properties': properties,
        }),
        'tags':[
            'build_type:%s' % constants.TRYJOB_TYPE,
        ]
    }

  def _GetSwarmingRequestBody(self, bot):
    """Generate the request body for a swarming buildbucket request.

    Args:
      bot: The bot config to put.

    Returns:
      buildbucket request properties as a python dict.
    """
    properties = self._GetRequestProperties(bot)
    properties['mastername'] = 'chromiumos.chromium'

    return {
        'bucket': constants.INTERNAL_SWARMING_BUILDBUCKET_BUCKET,
        'parameters_json': json.dumps({
            'builder_name': 'Generic',
            'properties': properties,
        }),
        'tags':[
            'build_type:%s' % constants.TRYJOB_TYPE,
        ]
    }

  def _PutConfigToBuildBucket(self, buildbucket_client, bot, dryrun):
    """Put the tryjob request to buildbucket.

    Args:
      buildbucket_client: The buildbucket client instance.
      bot: The bot config to put.
      dryrun: Whether a dryrun.
    """
    if self.swarming:
      request_body = self._GetSwarmingRequestBody(bot)
    else:
      request_body = self._GetBuildbotRequestBody(bot)

    content = buildbucket_client.PutBuildRequest(
        json.dumps(request_body), dryrun)

    if buildbucket_lib.GetNestedAttr(content, ['error']):
      raise RemoteRequestFailure(
          'buildbucket error.\nReason: %s\n Message: %s' %
          (buildbucket_lib.GetErrorReason(content),
           buildbucket_lib.GetErrorMessage(content)))

    buildbucket_id = buildbucket_lib.GetBuildId(content)
    print(self.BUILDBUCKET_PUT_RESP_FORMAT %
          (constants.TRYSERVER_BUILDBUCKET_BUCKET, bot, buildbucket_id))

  def _PostConfigsToBuildBucket(self, testjob=False, dryrun=False):
    """Posts the tryjob configs to buildbucket.

    Args:
      dryrun: Whether to skip the request to buildbucket.
      testjob: Whether to use the test instance of the buildbucket server.
    """
    host = (buildbucket_lib.BUILDBUCKET_TEST_HOST if testjob
            else buildbucket_lib.BUILDBUCKET_HOST)
    buildbucket_client = buildbucket_lib.BuildbucketClient(
        auth.GetAccessToken, host,
        service_account_json=buildbucket_lib.GetServiceAccount(
            constants.CHROMEOS_SERVICE_ACCOUNT))

    for bot in self.build_configs:
      self._PutConfigToBuildBucket(buildbucket_client, bot, dryrun)

  def GetTrybotWaterfallLink(self):
    """Get link to the waterfall for the user."""
    # The builders on the trybot waterfall are named after the templates.
    builders = set(self._GetBuilder(bot) for bot in self.build_configs)

    # Note that this will only show the jobs submitted by the user in the last
    # 24 hours.
    return '%s/waterfall?committer=%s&%s' % (
        constants.TRYBOT_DASHBOARD, self.user_email,
        '&'.join('builder=%s' % b for b in sorted(builders)))
