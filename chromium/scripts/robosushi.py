#!/usr/bin/python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Set up everything for the roll.
#
# --setup: set up the host to do a roll.  Idempotent, but probably doesn't need
#          to be run more than once in a while.
# --test:  configure ffmpeg for the host machine, and try running media unit
#          tests and the ffmpeg regression tests.
# --build: build ffmpeg configs for all platforms, then generate gn config.
# --all:   do everything.  Right now, this is the same as "--test --build".

import getopt
import os
import sys

import robo_branch
from robo_lib import log
import robo_lib
import robo_build
import robo_setup

def main(argv):
  robo_configuration = robo_lib.RoboConfiguration()
  robo_configuration.chdir_to_ffmpeg_home();

  parsed, remaining = getopt.getopt(argv, "",
          ["setup", "test", "build", "auto-merge", "auto-merge-test"])

  for opt, arg in parsed:
    if opt == "--setup":
      robo_setup.InstallPrereqs(robo_configuration)
      robo_setup.EnsureToolchains(robo_configuration)
      robo_setup.EnsureASANDirWorks(robo_configuration)
    elif opt == "--test":
      robo_build.BuildAndImportFFmpegConfigForHost(robo_configuration)
      robo_build.RunTests(robo_configuration)
    elif opt == "--build":
      # Unconditionally build all the configs and import them.
      robo_build.BuildAndImportAllFFmpegConfigs(robo_configuration)
    elif opt == "--auto-merge" or opt == "--auto-merge-test":
      # Start a branch (if needed), merge (if needed), and try to verify it.
      # TODO: Verify that the working directory is clean.
      robo_branch.CreateAndCheckoutDatedSushiBranchIfNeeded(robo_configuration)
      robo_branch.MergeUpstreamToSushiBranchIfNeeded(robo_configuration)
      # We want to push the merge and make the local branch track it, so that
      # future 'git cl upload's don't try to review the merge commit, and spam
      # the ffmpeg committers.
      robo_branch.PushToOriginWithoutReviewAndTrack(robo_configuration)

      # Try to get everything to build.
      # auto-merge-test is just to make this quicker while i'm developing it
      # TODO: Make it skip these if they're already done.
      if opt == "--auto-merge-test":
        robo_build.BuildAndImportFFmpegConfigForHost(robo_configuration)
      else:
        robo_build.BuildAndImportAllFFmpegConfigs(robo_configuration)
      robo_branch.HandleAutorename(robo_configuration)
      robo_branch.AddAndCommit(robo_configuration, "GN Configuration")
      robo_branch.CheckMerge(robo_configuration)
      robo_branch.WritePatchesReadme(robo_configuration)
      robo_branch.AddAndCommit(robo_configuration, "Chromium patches file")
      robo_build.RunTests(robo_configuration)

      # TODO: Start a fake deps roll.  To do this, we would:
      # Create new remote branch from the current remote sushi branch.
      # Create and check out a new local branch at the current local branch.
      # Make the new local branch track the new remote branch.
      # Push to origin/new remote branch.
      # Start a fake deps roll CL that runs the *san bots.
      # Switch back to original local branch.
      # For extra points, include a pointer to the fake deps roll CL in the
      # local branch, so that when it's pushed for review, it'll point the
      # reviewer at it.

      # TODO: git cl upload for review.
    else:
      raise Exception("Unknown option '%s'" % opt);

if __name__ == "__main__":
  main(sys.argv[1:])
