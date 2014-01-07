# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provide a BuilderRun class for collecting info on one builder run."""

import os

from chromite.buildbot import manifest_version


class RunAttributes(object):
  """Hold all run attributes for a particular builder run."""

  # A word of warning about the way run attributes currently work.  The
  # value set (or altered) by a given stage can be seen by any subsequent
  # stage, including stages on child processes.  Any value set (or
  # altered) by a stage in a subprocess will not be seen by co-subprocesses
  # (other stages being run in parallel at the same time) and will
  # be *discarded* when that subprocess ends (and parent process resumes).
  # This is expected behavior for forked processes.
  # TODO(mtennant): Possibly design a mechanism to preserve run attribute
  # changes in subprocess stages for subsequent stages (but probably not for
  # co-subprocess stages).

  __slots__ = (
      'manifest_manager', # Set by ManifestVersionedSyncStage.
      'release_tag',      # Set by cbuildbot after sync stage.
  )


class BuilderRun(object):
  """Class to represent one run of a builder."""

  # Class-level dict of RunAttributes objects to make it less
  # problematic to send BuilderRun objects between processes through
  # pickle.  The 'attrs' attribute on a BuilderRun object will look
  # up the RunAttributes for that particular BuilderRun here.
  _ATTRS = {}

  __slots__ = (
      'config',          # BuildConfig for this run.
      'options',         # The cbuildbot options object for this run.

      # Run attributes set/accessed by stages during the run.  To add support
      # for a new run attribute add it to the RunAttributes class above.
      '_attrs_id',

      # Some pre-computed run configuration values.
      'buildnumber',     # The build number for this run.
      'buildroot',       # The build root path for this run.
      'bot_id',          # Effective name of builder for this run.
      'manifest_branch', # The manifest branch to build and test for this run.

      # TODO(mtennant): Other candidates here include:
      # trybot, buildbot, remote_trybot, chrome_root, chrome_version,
      # test = (config build_tests AND option tests)
  )

  def __init__(self, options, build_config):
    self.options = options
    self.config = build_config

    # Create the RunAttributes object for this BuilderRun and save
    # the id number for it in order to look it up via attrs property.
    attrs = RunAttributes()
    self._ATTRS[id(attrs)] = attrs
    self._attrs_id = id(attrs)

    # Fill in values for all pre-computed "run configs" now, which are frozen
    # by this time.

    # TODO(mtennant): Should this use os.path.abspath like builderstage does?
    self.buildroot = self.options.buildroot
    self.buildnumber = self.options.buildnumber
    self.manifest_branch = self.options.branch
    self.bot_id = self.config.GetBotId(remote_trybot=self.options.remote_trybot)

  @property
  def attrs(self):
    """Look up the RunAttributes object for this BuilderRun object."""
    return self._ATTRS[self._attrs_id]

  def ShouldUploadPrebuilts(self):
    """Return True if this run should upload prebuilts."""
    return self.options.prebuilts and self.config.prebuilts

  def ShouldReexecAfterSync(self):
    """Return True if this run should re-exec itself after sync stage."""
    if self.options.postsync_reexec and self.config.postsync_reexec:
      # Return True if this source is not in designated buildroot.
      abs_buildroot = os.path.abspath(self.buildroot)
      return not os.path.abspath(__file__).startswith(abs_buildroot)

    return False

  def ShouldPatchAfterSync(self):
    """Return True if this run should patch changes after sync stage."""
    return self.options.postsync_patch and self.config.postsync_patch

  @classmethod
  def GetVersionInfo(cls, buildroot):
    """Helper for picking apart various version bits.

    This method only exists so that tests can override it.
    """
    return manifest_version.VersionInfo.from_repo(buildroot)

  def GetVersion(self):
    """Calculate full R<chrome_version>-<chromeos_version> version string.

    It is required that the sync stage be run before this method is called.

    Returns:
      The version string for this run.

    Raises:
      AssertionError if the sync stage has not been run first.
    """
    # This method should never be called before the sync stage has run, or
    # it would return a confusing value.
    assert hasattr(self.attrs, 'release_tag'), 'Sync stage must run first.'

    verinfo = self.GetVersionInfo(self.buildroot)
    release_tag = self.attrs.release_tag
    if release_tag:
      calc_version = 'R%s-%s' % (verinfo.chrome_branch, release_tag)
    else:
      # Non-versioned builds need the build number to uniquify the image.
      calc_version = 'R%s-%s-b%s' % (verinfo.chrome_branch,
                                     verinfo.VersionString(),
                                     self.buildnumber)

    return calc_version


class ChildBuilderRun(object):
  """A BuilderRun for a "child" build config, overriding self.config."""

  __slots__ = BuilderRun.__slots__ + (
      '_builder_run',    # The full (master) BuilderRun.
      'config',          # The child BuildConfig for this ChildBuilderRun.
      'child_index',     # Index into self.full_config.child_configs.
  )

  def __init__(self, builder_run, child_index):
    self._builder_run = builder_run
    self.child_index = child_index
    self.config = builder_run.config.child_configs[child_index]

  def __getattr__(self, attr):
    # Remember, __getattr__ only called if attribute was not found normally.
    # Point at self._builder_run, but access self._builder_run in a way that
    # does not cause infinite recursion.
    full_builder_run = self.__getattribute__('_builder_run')
    return getattr(full_builder_run, attr)
