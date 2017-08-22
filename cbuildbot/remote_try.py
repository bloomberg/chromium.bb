# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code related to Remote tryjobs."""

from __future__ import print_function

import getpass
import json
import os
import time

from chromite.cbuildbot import repository
from chromite.cbuildbot import manifest_version
from chromite.lib import buildbucket_lib
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cache
from chromite.lib import git

site_config = config_lib.GetConfig()

class ChromiteUpgradeNeeded(Exception):
  """Exception thrown when it's detected that we need to upgrade chromite."""

  def __init__(self, version=None):
    Exception.__init__(self)
    self.version = version
    self.args = (version,)

  def __str__(self):
    version_str = ''
    if self.version:
      version_str = "  Need format version %r support." % (self.version,)
    return (
        "Your version of cbuildbot is too old; please resync it, "
        "and then retry your submission.%s" % (version_str,))


class ValidationError(Exception):
  """Thrown when tryjob validation fails."""


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
  EXTERNAL_URL = os.path.join(site_config.params.EXTERNAL_GOB_URL,
                              'chromiumos/tryjobs')
  INTERNAL_URL = os.path.join(site_config.params.INTERNAL_GOB_URL,
                              'chromeos/tryjobs')

  # In version 3, remote patches have an extra field.
  # In version 4, cherry-picking is the norm, thus multiple patches are
  # generated.
  TRYJOB_FORMAT_VERSION = 4
  TRYJOB_FORMAT_FILE = '.tryjob_minimal_format_version'

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

  def __init__(self, bots, local_patches,
               pass_through_args,
               cache_dir,
               remote_description,
               committer_email=None,
               use_buildbucket=True,
               slaves=None):
    """Construct the object.

    Args:
      bots: A list of configs to run tryjobs for.
      local_patches: A list of LocalPatch objects.
      pass_through_args: Command line arguments to pass to cbuildbot in job.
      cache_dir: Cache directory used to hold manifest_versions checkout.
      remote_description: Requested tryjob description.
      committer_email: Email address of person requesting job, or None.
      use_buildbucket: use buildbucket for scheduling?
      slaves: Specific requested slaves for the job or []
    """
    self.use_buildbucket = use_buildbucket
    self.user = getpass.getuser()
    self.repo_cache = cache.DiskCache(cache_dir)
    cwd = os.path.dirname(os.path.realpath(__file__))
    if committer_email is not None:
      self.user_email = committer_email
    else:
      self.user_email = git.GetProjectUserEmail(cwd)
    logging.info('Using email:%s', self.user_email)
    # Name of the job that appears on the waterfall.
    self.name = remote_description
    self.bots = bots[:]
    self.slaves_request = slaves or []
    self.description = ('name: %s' % self.name)
    self.extra_args = pass_through_args
    if '--buildbot' not in self.extra_args:
      self.extra_args.append('--remote-trybot')

    self.extra_args.append('--remote-version=%s'
                           % (self.TRYJOB_FORMAT_VERSION,))
    self.local_patches = local_patches
    self.repo_url = self.EXTERNAL_URL
    self.cache_key = ('trybot',)
    self.manifest = git.ManifestCheckout.Cached(constants.SOURCE_ROOT)
    if repository.IsInternalRepoCheckout(constants.SOURCE_ROOT):
      self.repo_url = self.INTERNAL_URL
      self.cache_key = ('trybot-internal',)

  @property
  def values(self):
    return {
        'bot' : self.bots,
        'email' : [self.user_email],
        'extra_args' : self.extra_args,
        'name' : self.name,
        'slaves_request' : self.slaves_request,
        'user' : self.user,
        'version' : self.TRYJOB_FORMAT_VERSION,
        }

  def _VerifyForBuildbot(self):
    """Early validation, to ensure the job can be processed by buildbot."""

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

  def _Submit(self, workdir, testjob, dryrun):
    """Internal submission function.  See Submit() for arg description."""
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
    repository.UpdateGitRepo(workdir, self.repo_url)
    version_path = os.path.join(workdir,
                                self.TRYJOB_FORMAT_FILE)
    with open(version_path, 'r') as f:
      try:
        val = int(f.read().strip())
      except ValueError:
        raise ChromiteUpgradeNeeded()
      if val > self.TRYJOB_FORMAT_VERSION:
        raise ChromiteUpgradeNeeded(val)

    if self.use_buildbucket:
      self._PostConfigsToBuildBucket(testjob, dryrun)
    else:
      self._PushConfig(workdir, testjob, dryrun, current_time)

  def _GetBuilder(self, bot):
    """Find and return the builder for bot."""
    # Default to etc builder.
    builder = 'etc'
    if bot in site_config and '_template' in site_config[bot]:
      builder = site_config[bot]['_template']

    return builder

  def _GetProperties(self, bot):
    """Construct and return properties for buildbucket request."""
    properties = dict(self.values)
    properties.update({
        'cbb_config': bot,
        'cbb_extra_args': self.extra_args,
        'owners': [self.user_email]
    })
    return properties

  def _PutConfigToBuildBucket(self, buildbucket_client, bot, dryrun):
    """Put the tryjob request to buildbucket.

    Args:
      buildbucket_client: The buildbucket client instance.
      bot: The bot config to put.
      dryrun: Whether a dryrun.
    """

    body = json.dumps({
        'bucket': constants.TRYSERVER_BUILDBUCKET_BUCKET,
        'parameters_json': json.dumps({
            'builder_name': self._GetBuilder(bot),
            'properties': self._GetProperties(bot),
        }),
        'tags':['build_type:%s' % constants.TRYJOB_TYPE]
    })
    content = buildbucket_client.PutBuildRequest(body, dryrun)
    buildbucket_id = buildbucket_lib.GetBuildId(content)

    if buildbucket_id is not None:
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
        service_account=buildbucket_lib.GetServiceAccount(
            constants.CHROMEOS_SERVICE_ACCOUNT),
        host=host)

    for bot in self.bots:
      self._PutConfigToBuildBucket(buildbucket_client, bot, dryrun)

  def _PushConfig(self, workdir, testjob, dryrun, current_time):
    """Pushes the tryjob config to Git as a file.

    Args:
      workdir: see Submit()
      testjob: see Submit()
      dryrun: see Submit()
      current_time: the current time as a string represention of the time since
        unix epoch.
    """
    push_branch = manifest_version.PUSH_BRANCH

    remote_branch = None
    if testjob:
      remote_branch = git.RemoteRef('origin', 'refs/remotes/origin/test')
    git.CreatePushBranch(push_branch, workdir, sync=False,
                         remote_push_branch=remote_branch)
    file_name = '%s.%s' % (self.user, current_time)
    user_dir = os.path.join(workdir, self.user)
    if not os.path.isdir(user_dir):
      os.mkdir(user_dir)

    fullpath = os.path.join(user_dir, file_name)
    with open(fullpath, 'w+') as job_desc_file:
      json.dump(self.values, job_desc_file)

    git.RunGit(workdir, ['add', fullpath])
    extra_env = {
        # The committer field makes sure the creds match what the remote
        # gerrit instance expects while the author field allows lookup
        # on the console to work.  http://crosbug.com/27939
        'GIT_COMMITTER_EMAIL' : self.user_email,
        'GIT_AUTHOR_EMAIL'    : self.user_email,
    }
    git.RunGit(workdir, ['commit', '-m', self.description],
               extra_env=extra_env)

    try:
      git.PushWithRetry(push_branch, workdir, retries=3, dryrun=dryrun)
    except cros_build_lib.RunCommandError:
      logging.error(
          'Failed to submit tryjob.  This could be due to too many '
          'submission requests by users.  Please try again.')
      raise

  def Submit(self, workdir=None, testjob=False, dryrun=False):
    """Submit the tryjob through Git.

    Args:
      workdir: The directory to clone tryjob repo into.  If you pass this
               in, you are responsible for deleting the directory.  Used for
               testing.
      testjob: Submit job to the test branch of the tryjob repo.  The tryjob
               will be ignored by production master.
      dryrun: Setting to true will run everything except the final submit step.
    """
    if workdir is None:
      with self.repo_cache.Lookup(self.cache_key) as ref:
        self._Submit(ref.path, testjob, dryrun)
    else:
      self._Submit(workdir, testjob, dryrun)

  def GetTrybotWaterfallLink(self):
    """Get link to the waterfall for the user."""
    # The builders on the trybot waterfall are named after the templates.
    builders = set(self._GetBuilder(bot) for bot in self.bots)

    # Note that this will only show the jobs submitted by the user in the last
    # 24 hours.
    return '%s/waterfall?committer=%s&%s' % (
        constants.TRYBOT_DASHBOARD, self.user_email,
        '&'.join('builder=%s' % b for b in sorted(builders)))
