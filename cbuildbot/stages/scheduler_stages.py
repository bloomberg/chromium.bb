# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the scheduler stages."""

from __future__ import print_function

import json
import time

from chromite.cbuildbot.stages import generic_stages
from chromite.lib import buildbucket_lib
from chromite.lib import constants
from chromite.lib import config_lib
from chromite.lib import cros_logging as logging
from chromite.lib import failures_lib

class ScheduleSlavesStage(generic_stages.BuilderStage):
  """Stage that schedules slaves for the master build."""

  def __init__(self, builder_run, sync_stage, **kwargs):
    super(ScheduleSlavesStage, self).__init__(builder_run, **kwargs)
    self.sync_stage = sync_stage
    self.buildbucket_client = self.GetBuildbucketClient()

  def _GetBuildbucketBucket(self, build_name, build_config):
    """Get the corresponding Buildbucket bucket.

    Args:
      build_name: name of the build to put to Buildbucket.
      build_config: config of the build to put to Buildbucket.

    Raises:
      NoBuildbucketBucketFoundException when no Buildbucket bucket found.
    """
    bucket = buildbucket_lib.WATERFALL_BUCKET_MAP.get(
        build_config.active_waterfall)

    if bucket is None:
      raise buildbucket_lib.NoBuildbucketBucketFoundException(
          'No Buildbucket bucket found for builder %s waterfall: %s' %
          (build_name, build_config.active_waterfall))

    return bucket

  def PostSlaveBuildToBuildbucket(self, build_name, build_config,
                                  master_build_id, buildset_tag, dryrun):
    """Send a Put slave build request to Buildbucket.

    Args:
      build_name: Salve build name to put to Buildbucket.
      build_config: Slave build config to put to Buildbucket.
      master_build_id: Master build id of the slave build.
      buildset_tag: The buildset tag for strong consistent tag queries.
                    More context: crbug.com/661689
      dryrun: Whether a dryrun.
    """
    tags = ['buildset:%s' % buildset_tag,
            'build_type:%s' % build_config.build_type,
            'master:False',
            'master_config:%s' % self._run.config.name,
            'cbb_config:%s' % build_name,
            'cbb_branch:%s' % self._run.manifest_branch,
            'cbb_master_build_id:%s' % master_build_id]

    if build_config.boards:
      for board in build_config.boards:
        tags.append('board:%s' % board)

    body = json.dumps({
        'bucket': self._GetBuildbucketBucket(build_name, build_config),
        'parameters_json': json.dumps({
            'builder_name': build_name,
            'properties': {
                'cbb_config': build_name,
                'cbb_branch': self._run.manifest_branch,
                'cbb_master_build_id': master_build_id,
            }
        }),
        'tags': tags
    })

    content = self.buildbucket_client.PutBuildRequest(body, dryrun)

    buildbucket_id = buildbucket_lib.GetBuildId(content)
    created_ts = buildbucket_lib.GetBuildCreated_ts(content)

    logging.info('Build_name %s buildbucket_id %s created_timestamp %s',
                 build_name, buildbucket_id, created_ts)

    return (buildbucket_id, created_ts)

  def ScheduleSlaveBuildsViaBuildbucket(self, important_only, dryrun):
    """Schedule slave builds by sending PUT requests to Buildbucket.

    Args:
      important_only: Whether only schedule important slave builds.
      dryrun: Whether a dryrun.
    """
    if self.buildbucket_client is None:
      logging.info('No buildbucket_client. Skip scheduling slaves.')
      return

    build_id, _ = self._run.GetCIDBHandle()
    if build_id is None:
      logging.info('No build id. Skip scheduling slaves.')
      return

    buildset_tag = 'cbuildbot/%s/%s/%s' % (
        self._run.manifest_branch, self._run.config.name, build_id)

    scheduled_slave_builds = []
    unscheduled_slave_builds = []

    # Get all active slave build configs.
    slave_config_map = self._GetSlaveConfigMap(important_only)
    for slave_name, slave_config in slave_config_map.iteritems():
      try:
        buildbucket_id, created_ts = self.PostSlaveBuildToBuildbucket(
            slave_name, slave_config, build_id, buildset_tag, dryrun)

        if slave_config.important:
          scheduled_slave_builds.append(
              (slave_name, buildbucket_id, created_ts))
      except buildbucket_lib.BuildbucketResponseException as e:
        # Use 16-digit ts to be consistent with the created_ts from Buildbucket
        current_ts = int(round(time.time() * 1000000))
        unscheduled_slave_builds.append((slave_name, None, current_ts))
        if important_only or slave_config.important:
          raise
        else:
          logging.warning('Failed to schedule %s current timestamp %s: %s'
                          % (slave_name, current_ts, e))

    self._run.attrs.metadata.ExtendKeyListWithList(
        constants.METADATA_SCHEDULED_SLAVES, scheduled_slave_builds)
    self._run.attrs.metadata.ExtendKeyListWithList(
        constants.METADATA_UNSCHEDULED_SLAVES, unscheduled_slave_builds)

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    if (config_lib.IsMasterCQ(self._run.config) and
        not self.sync_stage.pool.HasPickedUpCLs()):
      logging.info('No new CLs or chumpped CLs found to verify in this CQ run,'
                   'do not schedule CQ slaves.')
      return

    self.ScheduleSlaveBuildsViaBuildbucket(important_only=False,
                                           dryrun=self._run.options.debug)
