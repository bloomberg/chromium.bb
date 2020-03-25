# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A library to generate and store the manifests for cros builders to use."""

from __future__ import print_function

import codecs
import os
import re
from xml.dom import minidom

from chromite.lib import config_lib
from chromite.lib import constants
from chromite.cbuildbot import manifest_version
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git


# Paladin constants for manifest names.
PALADIN_COMMIT_ELEMENT = 'pending_commit'

ANDROID_ELEMENT = 'android'
ANDROID_VERSION_ATTR = 'version'
CHROME_ELEMENT = 'chrome'
CHROME_VERSION_ATTR = 'version'


class PromoteCandidateException(Exception):
  """Exception thrown for failure to promote manifest candidate."""


class _LKGMCandidateInfo(manifest_version.VersionInfo):
  """Class to encapsualte the Chrome OS LKGM candidate info."""
  LKGM_RE = r'(\d+\.\d+\.\d+)(?:-rc(\d+))?'

  def __init__(self, version_string=None, chrome_branch=None, incr_type=None,
               version_file=None):
    """Initialize.

    You can instantiate this in a few ways:
    1) Using |version_file|, specifically chromeos_version.sh,
       which contains the version information.
    2) Just passing in |version_string| with 3 or 4 version components
       e.g. 41.0.0-r1.

    Args:
      version_string: Optional 3 component version string to parse.  Contains:
          build_number: release build number.
          branch_build_number: current build number on a branch.
          patch_number: patch number.
          revision_number: version revision
      chrome_branch: If version_string specified, specify chrome_branch i.e. 13.
      incr_type: How we should increment this version - build|branch|patch.
      version_file: version file location.
    """
    self.revision_number = 1
    if version_string:
      match = re.search(self.LKGM_RE, version_string)
      assert match, 'LKGM did not re %s' % self.LKGM_RE
      super(_LKGMCandidateInfo, self).__init__(match.group(1), chrome_branch,
                                               incr_type=incr_type)
      if match.group(2):
        self.revision_number = int(match.group(2))

    else:
      super(_LKGMCandidateInfo, self).__init__(version_file=version_file,
                                               incr_type=incr_type)

  def VersionString(self):
    """returns the full version string of the lkgm candidate"""
    return '%s.%s.%s-rc%s' % (self.build_number, self.branch_build_number,
                              self.patch_number, self.revision_number)

  def VersionComponents(self):
    """Return an array of ints of the version fields for comparing."""
    return [int(x) for x in [self.build_number, self.branch_build_number,
                             self.patch_number, self.revision_number]]

  def IncrementVersion(self):
    """Increments the version by incrementing the revision #."""
    self.revision_number += 1
    return self.VersionString()

  # pylint: disable=arguments-differ
  def UpdateVersionFile(self, *args, **kwargs):
    """Update the version file on disk.

    For LKGMCandidateInfo there is no version file so this function is a no-op.
    """


class LKGMManager(manifest_version.BuildSpecsManager):
  """A Class to manage lkgm candidates and their states.

  Vars:
    lkgm_subdir:  Subdirectory within manifest repo to store candidates.
  """
  # Sub-directories for LKGM and Chrome LKGM's.
  LKGM_SUBDIR = 'LKGM-candidates'
  ANDROID_PFQ_SUBDIR = 'android-LKGM-candidates'
  TOOLCHAIN_SUBDIR = 'toolchain'
  FULL_SUBDIR = 'full'
  INCREMENTAL_SUBDIR = 'incremental'

  def __init__(self, source_repo, manifest_repo, build_names, build_type,
               incr_type, force, branch, manifest=constants.DEFAULT_MANIFEST,
               dry_run=True, lkgm_path_rel=constants.LKGM_MANIFEST,
               config=None, metadata=None, buildstore=None,
               buildbucket_client=None):
    """Initialize an LKGM Manager.

    Args:
      source_repo: Repository object for the source code.
      manifest_repo: Manifest repository for manifest versions/buildspecs.
      build_names: Identifiers for the build. Must match config_lib
          entries. If multiple identifiers are provided, the first item in the
          list must be an identifier for the group.
      build_type: Type of build.  Must be a pfq type.
      incr_type: How we should increment this version - build|branch|patch
      force: Create a new manifest even if there are no changes.
      branch: Branch this builder is running on.
      manifest: Manifest to use for checkout. E.g. 'full' or 'buildtools'.
      dry_run: Whether we actually commit changes we make or not.
      master: Whether we are the master builder.
      lkgm_path_rel: Path to the LKGM symlink, relative to manifest dir.
      config: Instance of config_lib.BuildConfig. Config dict of this builder.
      metadata: Instance of metadata_lib.CBuildbotMetadata. Metadata of this
                builder.
      buildstore: BuildStore instance to make DB calls.
      buildbucket_client: Instance of buildbucket_lib.buildbucket_client.
    """
    super(LKGMManager, self).__init__(
        source_repo=source_repo, manifest_repo=manifest_repo,
        manifest=manifest, build_names=build_names, incr_type=incr_type,
        force=force, branch=branch, dry_run=dry_run,
        config=config, metadata=metadata, buildstore=buildstore,
        buildbucket_client=buildbucket_client)

    self.lkgm_path = os.path.join(self.manifest_dir, lkgm_path_rel)
    self.compare_versions_fn = _LKGMCandidateInfo.VersionCompare
    self.build_type = build_type
    # Chrome PFQ and PFQ's exist at the same time and version separately so they
    # must have separate subdirs in the manifest-versions repository.
    if self.build_type == constants.ANDROID_PFQ_TYPE:
      self.rel_working_dir = self.ANDROID_PFQ_SUBDIR
    elif self.build_type == constants.TOOLCHAIN_TYPE:
      self.rel_working_dir = self.TOOLCHAIN_SUBDIR
    elif self.build_type == constants.FULL_TYPE:
      self.rel_working_dir = self.FULL_SUBDIR
    elif self.build_type == constants.INCREMENTAL_TYPE:
      self.rel_working_dir = self.INCREMENTAL_SUBDIR
    else:
      assert config_lib.IsPFQType(self.build_type)
      self.rel_working_dir = self.LKGM_SUBDIR

  def GetCurrentVersionInfo(self):
    """Returns the lkgm version info from the version file."""
    version_info = super(LKGMManager, self).GetCurrentVersionInfo()
    return _LKGMCandidateInfo(version_info.VersionString(),
                              chrome_branch=version_info.chrome_branch,
                              incr_type=self.incr_type)

  def _WriteXml(self, dom_instance, file_path):
    """Wrapper function to write xml encoded in a proper way.

    Args:
      dom_instance: A DOM document instance contains contents to be written.
      file_path: Path to the file to write into.
    """
    with codecs.open(file_path, 'w+', 'utf-8') as f:
      dom_instance.writexml(f)

  def _AddAndroidVersionToManifest(self, manifest, android_version):
    """Adds the Android element with version |android_version| to |manifest|.

    The manifest file should contain the Android version to build for
    PFQ slaves.

    Args:
      manifest: Path to the manifest
      android_version: A string representing the version of Android
    """
    manifest_dom = minidom.parse(manifest)
    android = manifest_dom.createElement(ANDROID_ELEMENT)
    android.setAttribute(ANDROID_VERSION_ATTR, android_version)
    manifest_dom.documentElement.appendChild(android)
    self._WriteXml(manifest_dom, manifest)

  def _AddChromeVersionToManifest(self, manifest, chrome_version):
    """Adds the chrome element with version |chrome_version| to |manifest|.

    The manifest file should contain the Chrome version to build for
    PFQ slaves.

    Args:
      manifest: Path to the manifest
      chrome_version: A string representing the version of Chrome
        (e.g. 35.0.1863.0).
    """
    manifest_dom = minidom.parse(manifest)
    chrome = manifest_dom.createElement(CHROME_ELEMENT)
    chrome.setAttribute(CHROME_VERSION_ATTR, chrome_version)
    manifest_dom.documentElement.appendChild(chrome)
    self._WriteXml(manifest_dom, manifest)

  def CreateNewCandidate(self,
                         android_version=None,
                         chrome_version=None,
                         retries=manifest_version.NUM_RETRIES,
                         build_id=None):
    """Creates, syncs to, and returns the next candidate manifest.

    Args:
      android_version: The Android version to write in the manifest. Defaults
        to None, in which case no version is written.
      chrome_version: The Chrome version to write in the manifest. Defaults
        to None, in which case no version is written.
      retries: Number of retries for updating the status. Defaults to
        manifest_version.NUM_RETRIES.
      build_id: Optional integer cidb id of the build that is creating
                this candidate.

    Raises:
      GenerateBuildSpecException in case of failure to generate a buildspec
    """
    self.CheckoutSourceCode()

    # Refresh manifest logic from manifest_versions repository to grab the
    # LKGM to generate the blamelist.
    version_info = self.GetCurrentVersionInfo()
    self.RefreshManifestCheckout()
    self.InitializeManifestVariables(version_info)

    new_manifest = self.CreateManifest()

    # For Android PFQ, add the version of Android to use.
    if android_version:
      self._AddAndroidVersionToManifest(new_manifest, android_version)

    # For Chrome PFQ, add the version of Chrome to use.
    if chrome_version:
      self._AddChromeVersionToManifest(new_manifest, chrome_version)

    last_error = None
    for attempt in range(0, retries + 1):
      try:
        # Refresh manifest logic from manifest_versions repository.
        # Note we don't need to do this on our first attempt as we needed to
        # have done it to get the LKGM.
        if attempt != 0:
          self.RefreshManifestCheckout()
          self.InitializeManifestVariables(version_info)

        # If we don't have any valid changes to test, make sure the checkout
        # is at least different.
        if not self.force and self.HasCheckoutBeenBuilt():
          return None

        # Check whether the latest spec available in manifest-versions is
        # newer than our current version number. If so, use it as the base
        # version number. Otherwise, we default to 'rc1'.
        if self.latest:
          latest = max(self.latest, version_info.VersionString(),
                       key=self.compare_versions_fn)
          version_info = _LKGMCandidateInfo(
              latest, chrome_branch=version_info.chrome_branch,
              incr_type=self.incr_type)

        git.CreatePushBranch(manifest_version.PUSH_BRANCH, self.manifest_dir,
                             sync=False)
        version = self.GetNextVersion(version_info)
        self.PublishManifest(new_manifest, version, build_id=build_id)
        self.current_version = version
        return self.GetLocalManifest(version)
      except cros_build_lib.RunCommandError as e:
        err_msg = 'Failed to generate LKGM Candidate. error: %s' % e
        logging.error(err_msg)
        last_error = err_msg

    raise manifest_version.GenerateBuildSpecException(last_error)

  def CreateFromManifest(self, manifest, retries=manifest_version.NUM_RETRIES,
                         build_id=None):
    """Sets up an lkgm_manager from the given manifest.

    This method sets up an LKGM manager and publishes a new manifest to the
    manifest versions repo based on the passed in manifest but filtering
    internal repositories and changes out of it.

    Args:
      manifest: A manifest that possibly contains private changes/projects. It
        is named with the given version we want to create a new manifest from
        i.e R20-1920.0.1-rc7.xml where R20-1920.0.1-rc7 is the version.
      retries: Number of retries for updating the status.
      build_id: Optional integer cidb build id of the build publishing the
                manifest.

    Returns:
      Path to the manifest version file to use.

    Raises:
      GenerateBuildSpecException in case of failure to check-in the new
        manifest because of a git error or the manifest is already checked-in.
    """
    last_error = None
    new_manifest = manifest_version.FilterManifest(
        manifest,
        whitelisted_remotes=config_lib.GetSiteParams().EXTERNAL_REMOTES)
    version_info = self.GetCurrentVersionInfo()
    for _attempt in range(0, retries + 1):
      try:
        self.RefreshManifestCheckout()
        self.InitializeManifestVariables(version_info)

        git.CreatePushBranch(manifest_version.PUSH_BRANCH, self.manifest_dir,
                             sync=False)
        version = os.path.splitext(os.path.basename(manifest))[0]
        logging.info('Publishing filtered build spec')
        self.PublishManifest(new_manifest, version, build_id=build_id)
        self.current_version = version
        return self.GetLocalManifest(version)
      except cros_build_lib.RunCommandError as e:
        err_msg = 'Failed to generate LKGM Candidate. error: %s' % e
        logging.error(err_msg)
        last_error = err_msg

    raise manifest_version.GenerateBuildSpecException(last_error)

  def PromoteCandidate(self, retries=manifest_version.NUM_RETRIES):
    """Promotes the current LKGM candidate to be a real versioned LKGM."""
    assert self.current_version, 'No current manifest exists.'

    last_error = None
    path_to_candidate = self.GetLocalManifest(self.current_version)
    assert os.path.exists(path_to_candidate), 'Candidate not found locally.'

    # This may potentially fail for not being at TOT while pushing.
    for attempt in range(0, retries + 1):
      try:
        if attempt > 0:
          self.RefreshManifestCheckout()
        git.CreatePushBranch(manifest_version.PUSH_BRANCH,
                             self.manifest_dir, sync=False)
        manifest_version.CreateSymlink(path_to_candidate, self.lkgm_path)
        git.RunGit(self.manifest_dir, ['add', self.lkgm_path])
        self.PushSpecChanges(
            'Automatic: %s promoting %s to LKGM' % (self.build_names[0],
                                                    self.current_version))
        return
      except cros_build_lib.RunCommandError as e:
        last_error = 'Failed to promote manifest. error: %s' % e
        logging.info(last_error)
        logging.info('Retrying to promote manifest:  Retry %d/%d', attempt + 1,
                     retries)

    raise PromoteCandidateException(last_error)

  def GetLatestPassingSpec(self):
    """Get the last spec file that passed in the current branch."""
    raise NotImplementedError()
