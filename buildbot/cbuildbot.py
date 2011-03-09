#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main builder code for Chromium OS.

Used by Chromium OS buildbot configuration for all Chromium OS builds including
full and pre-flight-queue builds.
"""

import errno
import optparse
import os
import sys

if __name__ == '__main__':
  import constants
  sys.path.append(constants.SOURCE_ROOT)

import chromite.buildbot.cbuildbot_comm as cbuildbot_comm
import chromite.buildbot.cbuildbot_config as cbuildbot_config
import chromite.buildbot.cbuildbot_stages as stages
import chromite.lib.cros_build_lib as cros_lib


def _GetConfig(config_name):
  """Gets the configuration for the build"""
  build_config = {}
  if not cbuildbot_config.config.has_key(config_name):
    Warning('Non-existent configuration specified.')
    Warning('Please specify one of:')
    config_names = config.keys()
    config_names.sort()
    for name in config_names:
      Warning('  %s' % name)
    sys.exit(1)

  return cbuildbot_config.config[config_name]


def main():
  # Parse options
  usage = "usage: %prog [options] cbuildbot_config"
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-a', '--acl', default='private',
                    help='ACL to set on GSD archives')
  parser.add_option('-r', '--buildroot',
                    help='root directory where build occurs', default=".")
  parser.add_option('-n', '--buildnumber',
                    help='build number', type='int', default=0)
  parser.add_option('--chrome_rev', default=None, type='string',
                    dest='chrome_rev',
                    help=('Chrome_rev of type [tot|latest_release|'
                          'sticky_release]'))
  parser.add_option('-g', '--gsutil', default='', help='Location of gsutil')
  parser.add_option('-c', '--gsutil_archive', default='',
                    help='Datastore archive location')
  parser.add_option('--clobber', action='store_true', dest='clobber',
                    default=False,
                    help='Clobbers an old checkout before syncing')
  parser.add_option('--debug', action='store_true', dest='debug',
                    default=False,
                    help='Override some options to run as a developer.')
  parser.add_option('--noarchive', action='store_false', dest='archive',
                    default=True,
                    help="Don't run archive stage.")
  parser.add_option('--nobuild', action='store_false', dest='build',
                    default=True,
                    help="Don't actually build (for cbuildbot dev")
  parser.add_option('--noprebuilts', action='store_false', dest='prebuilts',
                    default=True,
                    help="Don't upload prebuilts.")
  parser.add_option('--nosync', action='store_false', dest='sync',
                    default=True,
                    help="Don't sync before building.")
  parser.add_option('--notests', action='store_false', dest='tests',
                    default=True,
                    help='Override values from buildconfig and run no tests.')
  parser.add_option('-f', '--revisionfile',
                    help='file where new revisions are stored')
  parser.add_option('-t', '--tracking-branch', dest='tracking_branch',
                    default='cros/master', help='Run the buildbot on a branch')
  parser.add_option('--nouprev', action='store_false', dest='uprev',
                    default=True,
                    help='Override values from buildconfig and never uprev.')
  parser.add_option('-u', '--url', dest='url',
                    default='http://git.chromium.org/git/manifest',
                    help='Run the buildbot on internal manifest')

  (options, args) = parser.parse_args()

  if len(args) >= 1:
    bot_id = args[-1]
    build_config = _GetConfig(bot_id)
  else:
    parser.error('Invalid usage.  Use -h to see usage.')

  try:
    if options.sync:
      stages.SyncStage(bot_id, options, build_config).Run()

    if options.build:
      stages.BuildBoardStage(bot_id, options, build_config).Run()

    if options.uprev:
      stages.UprevStage(bot_id, options, build_config).Run()

    if options.build:
      stages.BuildTargetStage(bot_id, options, build_config).Run()

    # TODO(sosa): We only want to archive artifacts if we have artifacts to
    # archive.  Today this means we have at least built an image.  We should
    # make this more general and have dependencies within stages themselves ...
    # i.e. archive -> build_target ... however someday it might be
    # archive->sync.
    try:
      if options.tests:
        stages.TestStage(bot_id, options, build_config).Run()

      # Control master / slave logic here.
      if build_config['master']:
        if cbuildbot_comm.HaveSlavesCompleted(cbuildbot_config.config):
          stages.PushChangesStage(bot_id, options, build_config).Run()
        else:
          cros_lib.Die('One of the other slaves failed.')

      elif build_config['important']:
        cbuildbot_comm.PublishStatus(cbuildbot_comm.STATUS_BUILD_COMPLETE)

    finally:
      if options.archive:
        stages.ArchiveStage(bot_id, options, build_config).Run()
        cros_lib.Info('BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:')
        cros_lib.Info(stages.BuilderStage.archive_url)

  except:
    # Send failure to master.
    if not build_config['master'] and build_config['important']:
      cbuildbot_comm.PublishStatus(cbuildbot_comm.STATUS_BUILD_FAILED)

    raise


if __name__ == '__main__':
    main()
