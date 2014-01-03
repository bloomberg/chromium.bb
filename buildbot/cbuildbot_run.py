# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provide a class for collecting info on one builder run.

There are two public classes, BuilderRun and ChildBuilderRun, that serve
this function.  The first is for most situations, the second is for "child"
configs within a builder config that has entries in "child_configs".

Almost all functionality is within the common _BuilderRunBase class.  The
only thing the BuilderRun and ChildBuilderRun classes are responsible for
is overriding the self.config value in the _BuilderRunBase object whenever
it is accessed.

It is important to note that for one overall run, there will be one
BuilderRun object and zero or more ChildBuilderRun objects, but they
will all share the same _BuilderRunBase *object*.  This means, for example,
that run attributes (e.g. self.attrs.release_tag) are shared between them
all, as intended.
"""

import functools
import os
import types

from chromite.buildbot import cbuildbot_archive
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
      'chrome_version',   # Set by SyncChromeStage, if it runs.
      'manifest_manager', # Set by ManifestVersionedSyncStage.
      'release_tag',      # Set by cbuildbot after sync stage.
  )


class _BuilderRunBase(object):
  """Class to represent one run of a builder.

  This class should never be instantiated directly, but instead be
  instantiated as part of a BuilderRun object.
  """

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
      '_attrs_id',       # Object ID for looking up self.attrs.

      # Some pre-computed run configuration values.
      'buildnumber',     # The build number for this run.
      'buildroot',       # The build root path for this run.
      'manifest_branch', # The manifest branch to build and test for this run.

      # Some attributes are available as properties.  In particular, attributes
      # that use self.config must be determined after __init__.
      # self.bot_id      # Effective name of builder for this run.

      # TODO(mtennant): Other candidates here include:
      # trybot, buildbot, remote_trybot, chrome_root, chrome_version,
      # test = (config build_tests AND option tests)
  )

  def __init__(self, options):
    self.options = options

    # Note that self.config is filled in dynamically by either of the classes
    # that are actually instantiated: BuilderRun and ChildBuilderRun.  In other
    # words, self.config can be counted on anywhere except in this __init__.
    # The implication is that any plain attributes that are calculated from
    # self.config contents must be provided as properties (or methods).
    # See the _RealBuilderRun class and its __getattr__ method for details.
    self.config = None

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

    # Certain run attributes have sensible defaults which can be set here.
    # This allows all code to safely assume that the run attribute exists.
    attrs.chrome_version = None

  @property
  def bot_id(self):
    """Return the bot_id for this run."""
    return self.config.GetBotId(remote_trybot=self.options.remote_trybot)

  @property
  def attrs(self):
    """Look up the RunAttributes object for this BuilderRun object."""
    return self._ATTRS[self._attrs_id]

  def GetArchive(self):
    """Create an Archive object for this BuilderRun object."""
    # The Archive class is very lightweight, and is read-only, so it
    # is ok to generate a new one on demand.  This also avoids worrying
    # about whether it can go through pickle.
    # Almost everything the Archive class does requires GetVersion(),
    # which means it cannot be used until the version has been settled on.
    # However, because it does have some use before then we provide
    # the GetVersion function itself to be called when needed later.
    return cbuildbot_archive.Archive(self.bot_id, self.GetVersion,
                                     self.options, self.config)

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


class _RealBuilderRun(object):
  """Base BuilderRun class that manages self.config access.

  For any builder run, sometimes the build config is the top-level config and
  sometimes it is a "child" config.  In either case, the config to use should
  override self.config for all cases.  This class provides a mechanism for
  overriding self.config access generally.
  """

  __slots__ = _BuilderRunBase.__slots__ + (
      '_run_base',  # The _BuilderRunBase object where most functionality is.
      '_config',    # Config to use for dynamically overriding self.config.
  )

  def __init__(self, run_base, build_config):
    self._run_base = run_base
    self._config = build_config

  def __getattr__(self, attr):
    # Remember, __getattr__ only called if attribute was not found normally.
    # In normal usage, the __init__ guarantees that self._run_base and
    # self._config will be present.  However, the unpickle process bypasses
    # __init__, and this object must be pickle-able.  That is why we access
    # self._run_base and self._config through __getattribute__ here, otherwise
    # unpickling results in infinite recursion.
    # TODO(mtennant): Revisit this if pickling support is changed to go through
    # the __init__ method, such as by supplying __reduce__ method.
    run_base = self.__getattribute__('_run_base')
    config = self.__getattribute__('_config')

    try:
      # run_base.config should always be None except when accessed through
      # this routine.  Override the value here, then undo later.
      run_base.config = config

      result = getattr(run_base, attr)
      if isinstance(result, types.MethodType):
        # Make sure run_base.config is also managed when the method is called.
        @functools.wraps(result)
        def FuncWrapper(*args, **kwargs):
          run_base.config = config
          try:
            return result(*args, **kwargs)
          finally:
            run_base.config = None

        # TODO(mtennant): Find a way to make the following actually work.  It
        # makes pickling more complicated, unfortunately.
        # Cache this function wrapper to re-use next time without going through
        # __getattr__ again.  This ensures that the same wrapper object is used
        # each time, which is nice for identity and equality checks.  Subtle
        # gotcha that we accept: if the function itself on run_base is replaced
        # then this will continue to provide the behavior of the previous one.
        #setattr(self, attr, FuncWrapper)

        return FuncWrapper
      else:
        return result

    finally:
      run_base.config = None


class BuilderRun(_RealBuilderRun):
  """A standard BuilderRun for a top-level build config."""

  def __init__(self, options, build_config):
    """Initialize.

    Args:
      options: Command line options from this cbuildbot run.
      build_config: Build config for this cbuildbot run.
    """
    run_base = _BuilderRunBase(options)
    super(BuilderRun, self).__init__(run_base, build_config)


class ChildBuilderRun(_RealBuilderRun):
  """A BuilderRun for a "child" build config."""

  def __init__(self, builder_run, child_index):
    """Initialize.

    Args:
      builder_run: BuilderRun for the parent (main) cbuildbot run.  Extract
        the _BuilderRunBase from it to make sure the same base is used for
        both the main cbuildbot run and any child runs.
      child_index: The child index of this child run, used to index into
        the main run's config.child_configs.
    """
    # pylint: disable=W0212
    run_base = builder_run._run_base
    config = builder_run.config.child_configs[child_index]
    super(ChildBuilderRun, self).__init__(run_base, config)
