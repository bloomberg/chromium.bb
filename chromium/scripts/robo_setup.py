#!/usr/bin/python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Set up everything for the roll.

import os
from subprocess import call
import subprocess
from robo_lib import log

def InstallUbuntuPackage(robo_configuration, package):
  """Install |package|.

    Args:
      robo_configuration: current RoboConfiguration.
      package: package name.
  """

  log("Installing package %s" % package)
  if call(["sudo", "apt-get", "install", package]):
    raise Exception("Could not install %s" % package)

def InstallPrereqs(robo_configuration):
    """Install prereqs needed to build ffmpeg.

    Args:
      robo_configuration: current RoboConfiguration.
    """
    robo_configuration.chdir_to_chrome_src();

    # Check out data for ffmpeg regression tests.
    media_directory = os.path.join("media", "test", "data", "internal")
    if not os.path.exists(media_directory):
      log("Checking out media internal test data")
      if call(["git", "clone",
              "https://chrome-internal.googlesource.com/chrome/data/media",
              media_directory]):
        raise Exception(
                "Could not check out chrome media internal test data %s" %
                media_directory)

    if robo_configuration.host_operating_system() == "linux":
      InstallUbuntuPackage(robo_configuration, "yasm")
      InstallUbuntuPackage(robo_configuration, "gcc-aarch64-linux-gnu")
    else:
      raise Exception("I don't know how to install deps for host os %s" %
          robo_configuration.host_operating_system())

def EnsureASANDirWorks(robo_configuration):
    """Create the asan out dir and config for ninja builds.

    Args:
      robo_configuration: current RoboConfiguration.
    """

    directory_name = robo_configuration.absolute_asan_directory()
    if os.path.exists(directory_name):
      return

    # Dir doesn't exist, so make it and generate the gn files.  Note that we
    # have no idea what state the ffmpeg config is, but that's okay.  gn will
    # re-build it when we run ninja later (after importing the ffmpeg config)
    # if it's changed.

    log("Creating asan build dir %s" % directory_name)
    os.mkdir(directory_name)

    # TODO(liberato): ffmpeg_branding Chrome vs ChromeOS.  also add arch
    # flags, etc.  Step 28.iii, and elsewhere.
    opts = ("is_debug=false", "is_clang=true", "proprietary_codecs=true",
            "media_use_libvpx=true", "media_use_ffmpeg=true",
            'ffmpeg_branding="Chrome"',
            "use_goma=true", "is_asan=true", "dcheck_always_on=true",
            "enable_mse_mpeg2ts_stream_parser=true",
            "enable_hevc_demuxing=true")
    print opts
    with open(os.path.join(directory_name, "args.gn"), "w") as f:
      for opt in opts:
        print opt
        f.write("%s\n" % opt)

    # Ask gn to generate build files.
    log("Running gn on %s" % directory_name)
    if call(["gn", "gen", robo_configuration.local_asan_directory()]):
      raise Exception("Unable to gn gen %s" %
              robo_configuration.local_asan_directory())
