# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various utilities to build Chrome with AFDO.

For a description of AFDO see gcc.gnu.org/wiki/AutoFDO.
"""

from __future__ import print_function

import bisect
import collections
import datetime
import glob
import json
import os
import re

from chromite.lib import constants, cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import (failures_lib, git, gs, osutils, path_util,
                          timeout_util)

# AFDO-specific constants.
AFDO_SUFFIX = '.afdo'
COMPRESSION_SUFFIX = '.bz2'

# Chrome URL where AFDO data is stored.
_gsurls = {}
AFDO_CHROOT_ROOT = os.path.join('%(build_root)s', constants.DEFAULT_CHROOT_DIR)
AFDO_LOCAL_DIR = os.path.join('%(root)s', 'tmp')
AFDO_BUILDROOT_LOCAL = AFDO_LOCAL_DIR % {'root': AFDO_CHROOT_ROOT}
CHROME_ARCH_VERSION = '%(package)s-%(arch)s-%(version)s'
CHROME_PERF_AFDO_FILE = '%s.perf.data' % CHROME_ARCH_VERSION
CHROME_AFDO_FILE = '%s%s' % (CHROME_ARCH_VERSION, AFDO_SUFFIX)
CHROME_ARCH_RELEASE = '%(package)s-%(arch)s-%(release)s'
LATEST_CHROME_AFDO_FILE = 'latest-%s%s' % (CHROME_ARCH_RELEASE, AFDO_SUFFIX)
CHROME_DEBUG_BIN = os.path.join('%(root)s', 'build/%(board)s/usr/lib/debug',
                                'opt/google/chrome/chrome.debug')
# regex to find AFDO file for specific architecture within the ebuild file.
CHROME_EBUILD_AFDO_REGEX = (
    r'^(?P<bef>AFDO_FILE\["%s"\]=")(?P<name>.*)(?P<aft>")')
# and corresponding replacement string.
CHROME_EBUILD_AFDO_REPL = r'\g<bef>%s\g<aft>'

GSURL_BASE_BENCH = 'gs://chromeos-prebuilt/afdo-job/llvm'
GSURL_BASE_CWP = 'gs://chromeos-prebuilt/afdo-job/cwp/chrome'
GSURL_BASE_RELEASE = 'gs://chromeos-prebuilt/afdo-job/release-merged'
GSURL_CHROME_PERF = os.path.join(GSURL_BASE_BENCH,
                                 CHROME_PERF_AFDO_FILE + COMPRESSION_SUFFIX)
GSURL_CHROME_AFDO = os.path.join(GSURL_BASE_BENCH,
                                 CHROME_AFDO_FILE + COMPRESSION_SUFFIX)
GSURL_LATEST_CHROME_AFDO = os.path.join(GSURL_BASE_BENCH,
                                        LATEST_CHROME_AFDO_FILE)
GSURL_CHROME_DEBUG_BIN = os.path.join(
    GSURL_BASE_BENCH, CHROME_ARCH_VERSION + '.debug' + COMPRESSION_SUFFIX)

AFDO_GENERATE_LLVM_PROF = '/usr/bin/create_llvm_prof'

# An AFDO data is considered stale when BOTH of the following two metrics don't
# meet. For example, if an AFDO data is generated 20 days ago but only 5 builds
# away, it is considered valid.

# How old can the AFDO data be? (in days).
AFDO_ALLOWED_STALE_DAYS = 30

# How old can the AFDO data be? (in difference of builds).
AFDO_ALLOWED_STALE_BUILDS = 7

# How old can the Kernel AFDO data be? (in days).
KERNEL_ALLOWED_STALE_DAYS = 42

# How old can the Kernel AFDO data be before sheriff got noticed? (in days).
KERNEL_WARN_STALE_DAYS = 14

# Set of boards that can generate the AFDO profile (can generate 'perf'
# data with LBR events). Currently, it needs to be a device that has
# at least 4GB of memory.
#
# This must be consistent with the definitions in autotest.
AFDO_DATA_GENERATORS_LLVM = ('chell')

AFDO_ALERT_RECIPIENTS = [
    'chromeos-toolchain-sheriff@grotations.appspotmail.com'
]

KERNEL_PROFILE_URL = 'gs://chromeos-prebuilt/afdo-job/cwp/kernel/'
KERNEL_PROFILE_LS_PATTERN = '*/*.gcov.xz'
KERNEL_PROFILE_NAME_PATTERN = (
    r'([0-9]+\.[0-9]+)/R([0-9]+)-([0-9]+)\.([0-9]+)-([0-9]+)\.gcov\.xz')
KERNEL_PROFILE_MATCH_PATTERN = (
    r'^AFDO_PROFILE_VERSION="R[0-9]+-[0-9]+\.[0-9]+-[0-9]+"$')
KERNEL_PROFILE_WRITE_PATTERN = 'AFDO_PROFILE_VERSION="R%d-%d.%d-%d"'
KERNEL_EBUILD_ROOT = os.path.join(
    constants.SOURCE_ROOT, 'src/third_party/chromiumos-overlay/sys-kernel')

# Kernels that we can't generate afdo anymore because of reasons like
# too few samples etc.
KERNEL_SKIP_AFDO_UPDATE = ['3.8']

GSURL_CWP_SUBDIR = {
    'silvermont': '',
    'airmont': 'airmont',
    'broadwell': 'broadwell',
}

# Relative weights we should use when merging our 'release' profiles. The
# counters in our benchmark/cwp profiles end up being multiplied by these
# numbers, so they can technically be anything, but we have them sum to 100 for
# ease of understanding.
_RELEASE_BENCHMARK_MERGE_WEIGHT = 25
_RELEASE_CWP_MERGE_WEIGHT = 75

# Filename pattern of CWP profiles for Chrome
CWP_CHROME_PROFILE_NAME_PATTERN = r'R%s-%s.%s-%s' + AFDO_SUFFIX + '.xz'

BENCHMARK_PROFILE_NAME_RE = re.compile(
    r"""
       ^chromeos-chrome-amd64-
       (\d+)\.                    # Major
       (\d+)\.                    # Minor
       (\d+)\.                    # Build
       (\d+)                      # Patch
       (?:_rc)?-r(\d+)            # Revision
       (-merged)?\.
       afdo(?:\.bz2)?$            # We don't care about the presence of .bz2,
                                  # so we use the ignore-group '?:' operator.
     """, re.VERBOSE)

BenchmarkProfileVersion = collections.namedtuple(
    'BenchmarkProfileVersion',
    ['major', 'minor', 'build', 'patch', 'revision', 'is_merged'])


class MissingAFDOData(failures_lib.StepFailure):
  """Exception thrown when necessary AFDO data is missing."""


class MissingAFDOMarkers(failures_lib.StepFailure):
  """Exception thrown when necessary ebuild markers for AFDO are missing."""


class UnknownKernelVersion(failures_lib.StepFailure):
  """Exception thrown when the Kernel version can't be inferred."""


class NoValidProfileFound(failures_lib.StepFailure):
  """Exception thrown when there is no valid profile found."""


def CompressAFDOFile(to_compress, buildroot):
  """Compress file used by AFDO process.

  Args:
    to_compress: File to compress.
    buildroot: buildroot where to store the compressed data.

  Returns:
    Name of the compressed data file.
  """
  local_dir = AFDO_BUILDROOT_LOCAL % {'build_root': buildroot}
  dest = os.path.join(local_dir, os.path.basename(to_compress)) + \
      COMPRESSION_SUFFIX
  cros_build_lib.CompressFile(to_compress, dest)
  return dest


def UncompressAFDOFile(to_decompress, buildroot):
  """Decompress file used by AFDO process.

  Args:
    to_decompress: File to decompress.
    buildroot: buildroot where to store the decompressed data.
  """
  local_dir = AFDO_BUILDROOT_LOCAL % {'build_root': buildroot}
  basename = os.path.basename(to_decompress)
  dest_basename = basename.rsplit('.', 1)[0]
  dest = os.path.join(local_dir, dest_basename)
  cros_build_lib.UncompressFile(to_decompress, dest)
  return dest


def GSUploadIfNotPresent(gs_context, src, dest):
  """Upload a file to GS only if the file does not exist.

  Will not generate an error if the file already exist in GS. It will
  only emit a warning.

  I could use GSContext.Copy(src,dest,version=0) here but it does not seem
  to work for large files. Using GSContext.Exists(dest) instead. See
  crbug.com/395858.

  Args:
    gs_context: GS context instance.
    src: File to copy.
    dest: Destination location.

  Returns:
    True if file was uploaded. False otherwise.
  """
  if gs_context.Exists(dest):
    logging.warning('File %s already in GS', dest)
    return False
  else:
    gs_context.Copy(src, dest, acl='public-read')
    return True


def GetAFDOPerfDataURL(cpv, arch):
  """Return the location URL for the AFDO per data file.

  Build the URL for the 'perf' data file given the release and architecture.

  Args:
    cpv: The portage_util.CPV object for chromeos-chrome.
    arch: architecture we're going to build Chrome for.

  Returns:
    URL of the location of the 'perf' data file.
  """

  # The file name of the perf data is based only in the chrome version.
  # The test case that produces it does not know anything about the
  # revision number.
  # TODO(llozano): perf data filename should include the revision number.
  version_number = cpv.version_no_rev.split('_')[0]
  chrome_spec = {
      'package': cpv.package,
      'arch': arch,
      'version': version_number
  }
  return GSURL_CHROME_PERF % chrome_spec


def CheckAFDOPerfData(cpv, arch, gs_context):
  """Check whether AFDO perf data exists for the given architecture.

  Check if 'perf' data file for this architecture and release is available
  in GS.

  Args:
    cpv: The portage_util.CPV object for chromeos-chrome.
    arch: architecture we're going to build Chrome for.
    gs_context: GS context to retrieve data.

  Returns:
    True if AFDO perf data is available. False otherwise.
  """
  url = GetAFDOPerfDataURL(cpv, arch)
  if not gs_context.Exists(url):
    logging.info('Could not find AFDO perf data at %s', url)
    return False

  logging.info('Found AFDO perf data at %s', url)
  return True


def WaitForAFDOPerfData(cpv,
                        arch,
                        buildroot,
                        gs_context,
                        timeout=constants.AFDO_GENERATE_TIMEOUT):
  """Wait for AFDO perf data to show up (with an appropriate timeout).

  Wait for AFDO 'perf' data to show up in GS and copy it into a temp
  directory in the buildroot.

  Args:
    arch: architecture we're going to build Chrome for.
    cpv: CPV object for Chrome.
    buildroot: buildroot where AFDO data should be stored.
    gs_context: GS context to retrieve data.
    timeout: How long to wait total, in seconds.

  Returns:
    True if found the AFDO perf data before the timeout expired.
    False otherwise.
  """
  try:
    timeout_util.WaitForReturnTrue(
        CheckAFDOPerfData,
        func_args=(cpv, arch, gs_context),
        timeout=timeout,
        period=constants.SLEEP_TIMEOUT)
  except timeout_util.TimeoutError:
    logging.info('Could not find AFDO perf data before timeout')
    return False

  url = GetAFDOPerfDataURL(cpv, arch)
  dest_dir = AFDO_BUILDROOT_LOCAL % {'build_root': buildroot}
  dest_path = os.path.join(dest_dir, url.rsplit('/', 1)[1])
  gs_context.Copy(url, dest_path)

  UncompressAFDOFile(dest_path, buildroot)
  logging.info('Retrieved AFDO perf data to %s', dest_path)
  return True


def _BuildrootToWorkDirs(buildroot):
  chroot_root = AFDO_CHROOT_ROOT % {'build_root': buildroot}
  local_dir = AFDO_LOCAL_DIR % {'root': chroot_root}
  in_chroot_local_dir = AFDO_LOCAL_DIR % {'root': ''}
  return chroot_root, local_dir, in_chroot_local_dir


def _EnumerateMostRecentProfiles(gs_context, milestones, glob_url,
                                 parse_profile_name):
  """Enumerates the most recent AFDO profiles for the given Chrome releases.

  Args:
    gs_context: How we talk to gs://
    milestones: A list of ints; each one is a major Chrome version. We'll
      try to get the most recent profile for each of these.
    glob_url: A URL to query gsutil with.
    parse_profile_name: A callable that transforms a profile's filename into
      an object that:
      - is orderable such that |a < b| implies that |a| is an older profile
        than |b|
      - has a |major| attribute that indicates Chrome's major version number

      Alternatively, if it returns None, we skip the given profile.

  Returns:
    A dict of {milestone_number: latest_profile_gs_url}. The keys in this
    milestone are a (not-strict) subset of the values in |milestones|.
  """
  profile_listing = gs_context.List(glob_url)
  if not profile_listing:
    raise ValueError('No profiles matched %s' % glob_url)

  parsed_profiles = []
  for profile in profile_listing:
    url = profile.url
    parsed = parse_profile_name(os.path.basename(url))
    if parsed is not None:
      parsed_profiles.append((parsed, url))

  newest = {}
  for version in milestones:
    profiles = [(v, url) for v, url in parsed_profiles if v.major == version]
    if not profiles:
      continue

    _, url = max(profiles)
    newest[version] = url

  return newest


def _EnumerateMostRecentCWPProfiles(gs_context, milestones):
  """Enumerates the most recent CWP AFDO profiles for Chrome releases.

  See _EnumerateMostRecentProfiles for info about args/return value.
  """
  profile_suffix = AFDO_SUFFIX + '.xz'
  glob_url = os.path.join(GSURL_BASE_CWP, '*' + profile_suffix)

  # e.g. R75-3729.38-1554716539.afdo.xz
  profile_name_re = re.compile(
      r"""
         ^R(\d+)-      # Major
         (\d+)\.       # Build
         (\d+)-        # Patch
         (\d+)         # Clock; breaks ties sometimes.
         \.afdo\.xz$
       """, re.VERBOSE)

  ProfileVersion = collections.namedtuple('ProfileVersion',
                                          ['major', 'build', 'patch', 'clock'])

  def parse_profile_name(url_basename):
    match = profile_name_re.match(url_basename)
    if not match:
      raise ValueError('Unparseable CWP profile name: %s' % url_basename)
    return ProfileVersion(*[int(x) for x in match.groups()])

  return _EnumerateMostRecentProfiles(gs_context, milestones, glob_url,
                                      parse_profile_name)


def _ParseBenchmarkProfileName(profile_name):
  match = BENCHMARK_PROFILE_NAME_RE.match(profile_name)
  if not match:
    raise ValueError('Unparseable benchmark profile name: %s' % profile_name)

  groups = match.groups()
  version_groups = groups[:-1]
  is_merged = groups[-1]
  return BenchmarkProfileVersion(
      *[int(x) for x in version_groups], is_merged=bool(is_merged))


def _EnumerateMostRecentBenchmarkProfiles(gs_context, milestones):
  """Enumerates the most recent benchmark AFDO profiles for Chrome releases.

  See _EnumerateMostRecentProfiles for info about args/return value.
  """
  profile_suffix = AFDO_SUFFIX + COMPRESSION_SUFFIX
  glob_url = os.path.join(GSURL_BASE_BENCH, '*' + profile_suffix)

  def parse_profile_name(url_basename):
    parsed = _ParseBenchmarkProfileName(url_basename)
    # We don't want to merge a merged profile; merged profiles are primarily
    # for stability, and we have CWP to provide us that.
    return None if parsed.is_merged else parsed

  return _EnumerateMostRecentProfiles(gs_context, milestones, glob_url,
                                      parse_profile_name)


def GenerateReleaseProfileMergePlan(gs_context, milestones):
  """Generates a plan to merge release profiles for Chrome milestones.

  Args:
    gs_context: How we talk to gs://
    milestones: A list of ints; Chrome milestones

  Returns:
    A tuple (a, b), where:
      - |b| is a dict of {milestone: (cwp_profile, benchmark_profile)}, where
        |benchmark_profile| and |cwp_profile| are paths in gs:// that point to
        the most recent benchmark and CWP profiles for |milestone|.
      - |a| is a sorted list of milestones that aren't present in |b|, but are
        present in |milestones|.
  """
  benchmark_profiles = _EnumerateMostRecentBenchmarkProfiles(
      gs_context, milestones)
  cwp_profiles = _EnumerateMostRecentCWPProfiles(gs_context, milestones)

  planned_merges = {
      version: (cwp_profiles[version], benchmark_profile)
      for version, benchmark_profile in benchmark_profiles.items()
      if version in cwp_profiles
  }
  skipped = sorted(set(milestones) - set(planned_merges))
  return skipped, planned_merges


def ExecuteReleaseProfileMergePlan(gs_context, buildroot, merge_plan):
  """Generates release profiles, given a release profile merge plan.

  Args:
    gs_context: How we talk to gs://
    buildroot: Our buildroot
    merge_plan: The second result of GenerateReleaseProfileMergePlan. This
      determines the profiles we pull and merge.
  """
  _, work_dir, chroot_work_dir = _BuildrootToWorkDirs(buildroot)

  def path_pair(suffix):
    outside_chroot = os.path.join(work_dir, suffix)
    in_chroot = os.path.join(chroot_work_dir, suffix)
    return in_chroot, outside_chroot

  chroot_work_dir, work_dir = path_pair('afdo_data_merge')

  def copy_profile(gs_path, local_path):
    assert local_path.endswith('.afdo'), local_path
    assert not gs_path.endswith('.afdo'), gs_path

    compression_suffix = os.path.splitext(gs_path)[1]
    temp_path = local_path + compression_suffix
    gs_context.Copy(gs_path, temp_path)
    cros_build_lib.UncompressFile(temp_path, local_path)

  merge_results = {}
  for version, (cwp_profile, benchmark_profile) in merge_plan.items():
    chroot_benchmark_path, benchmark_path = path_pair('benchmark.afdo')
    copy_profile(benchmark_profile, benchmark_path)

    chroot_cwp_path, cwp_path = path_pair('cwp.afdo')
    copy_profile(cwp_profile, cwp_path)

    chroot_merged_path, merged_path = path_pair('m%d.afdo' % version)
    merge_weights = [
        (chroot_cwp_path, _RELEASE_CWP_MERGE_WEIGHT),
        (chroot_benchmark_path, _RELEASE_BENCHMARK_MERGE_WEIGHT),
    ]
    _MergeAFDOProfiles(merge_weights, chroot_merged_path, use_compbinary=True)

    comp_merged_path = merged_path + COMPRESSION_SUFFIX
    cros_build_lib.CompressFile(merged_path, comp_merged_path)
    merge_results[version] = comp_merged_path

  return merge_results


def UploadReleaseProfiles(gs_context, run_id, merge_plan, merge_results):
  """Uploads the profiles in merge_results to our release profile bucket.

  Args:
    gs_context: Our GS context
    run_id: A unique identifier for this run. Generally recommended to be the
      number of seconds since the unix epoch, or something similarly difficult
      to 'collide' with other runs. This is used in paths to guarantee
      uniqueness.
    merge_plan: The merge plan that generated the given |merge_results|. Only
      used to write to a metadata file, so we know what went into this profile.
    merge_results: A map describing the profiles to upload; you can get one
      from ExecuteReleaseProfileMergePlan.
  """
  gs_url_base = os.path.join(GSURL_BASE_RELEASE, run_id)

  def copy_file_to_gs(local_path, remote_path):
    # Note that version=0 implies that we'll never overwrite anything. If
    # run_id is truly unique, this should never make a difference.
    gs_context.Copy(local_path, remote_path, acl='public-read', version=0)

  for version, profile in merge_results.items():
    suffix = os.path.splitext(profile)[1]
    assert suffix != '.afdo', 'All profiles should be compressed.'
    output_path = os.path.join(gs_url_base,
                               'profiles/m%d.afdo%s' % (version, suffix))
    copy_file_to_gs(profile, output_path)

  # Write a map describing the profiles that have been uploaded. Not
  # compressed, because it's expected to be <500 bytes. At the time of writing,
  # no automated system relies on these; we just write them so it's easier to
  # understand what 'gs://path/to/profiles/m75.afdo' actually consists of.
  temp_dir = osutils.GetGlobalTempDir()
  meta_file_path = os.path.join(temp_dir, 'meta.json')
  osutils.WriteFile(meta_file_path, json.dumps(merge_plan))
  copy_file_to_gs(meta_file_path, os.path.join(gs_url_base, 'meta.json'))


def _MergeAFDOProfiles(chroot_profile_list,
                       chroot_output_profile,
                       use_compbinary=False):
  """Merges the given profile list.

  Args:
    chroot_profile_list: a list of (profile_path, profile_weight).
      Profile_weight is an int that tells us how to weight the profile compared
      to everything else.
    chroot_output_profile: where to store the result profile.
    use_compbinary: whether to use the new compressed binary AFDO profile
      format.
  """
  if not chroot_profile_list:
    raise ValueError('Need profiles to merge')

  # A regular llvm-profdata command looks like:
  # llvm-profdata merge [-sample] -output=/path/to/output input1 [input2
  #                                                               [input3 ...]]
  #
  # Alternatively, we can specify inputs by `-weighted-input=A,file`, where A
  # is a multiplier of the sample counts in the profile.
  merge_command = [
      'llvm-profdata',
      'merge',
      '-sample',
      '-output=' + chroot_output_profile,
  ]

  merge_command += [
      '-weighted-input=%d,%s' % (weight, name)
      for name, weight in chroot_profile_list
  ]

  if use_compbinary:
    merge_command.append('-compbinary')

  cros_build_lib.RunCommand(
      merge_command, enter_chroot=True, capture_output=True, print_cmd=True)


def CreateAndUploadMergedAFDOProfile(gs_context,
                                     buildroot,
                                     unmerged_name,
                                     recent_to_merge=5,
                                     max_age_days=14):
  """Create a merged AFDO profile from recent AFDO profiles and upload it.

  If the upload would overwrite an existing merged file, this skips the upload.

  Args:
    gs_context: GS Context
    buildroot: The build root
    unmerged_name: name of the AFDO profile we've just uploaded. No profiles
      whose names are lexicographically ordered after this are candidates for
      selection.
    recent_to_merge: The maximum number of profiles to merge
    max_age_days: Don't merge profiles older than max_age_days days old.

  Returns:
    A (str, bool) of:
      - The name of a merged profile in GSURL_BASE_BENCH if the AFDO profile is
        a candidate for merging. Otherwise, None.
      - Whether we uploaded a merged profile.
  """
  _, work_dir, chroot_work_dir = _BuildrootToWorkDirs(buildroot)
  profile_suffix = AFDO_SUFFIX + COMPRESSION_SUFFIX
  merged_suffix = '-merged'

  glob_url = os.path.join(GSURL_BASE_BENCH, '*' + profile_suffix)
  benchmark_listing = gs_context.List(glob_url, details=True)

  unmerged_version = _ParseBenchmarkProfileName(unmerged_name)

  def is_merge_candidate(x):
    version = _ParseBenchmarkProfileName(os.path.basename(x.url))
    # Exclude merged profiles, because merging merged profiles into merged
    # profiles is likely bad.
    return unmerged_version >= version and not version.is_merged

  benchmark_profiles = [p for p in benchmark_listing if is_merge_candidate(p)]

  if not benchmark_profiles:
    logging.warning('Skipping merged profile creation: no merge candidates '
                    'found')
    return None, False

  latest_profile = benchmark_profiles[-1]
  latest_profile_version = _ParseBenchmarkProfileName(
      os.path.basename(benchmark_profiles[-1].url))

  # crbug.com/954978: branch profiles don't play nicely with non-branch
  # profiles. Hence, we want to avoid merging branch profiles into ToT
  # profiles. Merging ToT profiles -> branch is fine, since that's all we
  # realistically have in our past, but ToT should always prefer profiles
  # collected from ToT only.
  def is_differing_branch_profile(x):
    parsed = _ParseBenchmarkProfileName(os.path.basename(x.url))
    # Branches bump patch levels.
    if parsed.patch == 0:
      return False
    return parsed.major != latest_profile_version.major

  benchmark_profiles = [
      p for p in benchmark_profiles if not is_differing_branch_profile(p)
  ]
  # We'll always have |latest_profile| in |benchmark_profiles|. If not,
  # |is_differing_branch_profile| is broken, since it should only filter out
  # branch profiles with a major version != |latest_profile|'s.
  assert benchmark_profiles

  base_time = latest_profile.creation_time
  time_cutoff = base_time - datetime.timedelta(days=max_age_days)
  merge_candidates = [
      p for p in benchmark_profiles if p.creation_time >= time_cutoff
  ]

  merge_candidates = merge_candidates[-recent_to_merge:]

  # This should never happen, but be sure we're not merging a profile into
  # itself anyway. It's really easy for that to silently slip through, and can
  # lead to overrepresentation of a single profile, which just causes more
  # noise.
  assert len(set(p.url for p in merge_candidates)) == len(merge_candidates)

  # Merging a profile into itself is pointless.
  if len(merge_candidates) == 1:
    logging.warning('Skipping merged profile creation: we only have a single '
                    'merge candidate.')
    return None, False

  chroot_afdo_files = []
  for candidate in merge_candidates:
    # It would be slightly less complex to just name these off as
    # profile-1.afdo, profile-2.afdo, ... but the logs are more readable if we
    # keep the basename from gs://.
    candidate_name = os.path.basename(candidate.url)
    candidate_uncompressed_name = candidate_name[:-len(COMPRESSION_SUFFIX)]

    copy_from = candidate.url
    copy_to = os.path.join(work_dir, candidate_name)
    copy_to_uncompressed = os.path.join(work_dir, candidate_uncompressed_name)
    chroot_file = os.path.join(chroot_work_dir, candidate_uncompressed_name)

    gs_context.Copy(copy_from, copy_to)
    cros_build_lib.UncompressFile(copy_to, copy_to_uncompressed)
    chroot_afdo_files.append(chroot_file)

  merged_basename = os.path.basename(chroot_afdo_files[-1])
  assert merged_basename.endswith(AFDO_SUFFIX)
  merged_basename = merged_basename[:-len(AFDO_SUFFIX)] + merged_suffix + \
      AFDO_SUFFIX

  merged_output_path = os.path.join(work_dir, merged_basename)
  chroot_merged_output_path = os.path.join(chroot_work_dir, merged_basename)

  # Weight all profiles equally.
  _MergeAFDOProfiles([(profile, 1) for profile in chroot_afdo_files],
                     chroot_merged_output_path)

  compressed_path = CompressAFDOFile(merged_output_path, buildroot)
  compressed_basename = os.path.basename(compressed_path)
  gs_target = os.path.join(GSURL_BASE_BENCH, compressed_basename)
  uploaded = GSUploadIfNotPresent(gs_context, compressed_path, gs_target)
  return merged_basename, uploaded


def PatchChromeEbuildAFDOFile(ebuild_file, profiles):
  """Patch the Chrome ebuild with the dictionary of {arch: afdo_file} pairs.

  Args:
    ebuild_file: path of the ebuild file within the chroot.
    profiles: {source: afdo_file} pairs to put into the ebuild.
  """
  original_ebuild = path_util.FromChrootPath(ebuild_file)
  modified_ebuild = '%s.new' % original_ebuild

  patterns = {}
  repls = {}
  markers = {}
  for source in profiles.keys():
    patterns[source] = re.compile(CHROME_EBUILD_AFDO_REGEX % source)
    repls[source] = CHROME_EBUILD_AFDO_REPL % profiles[source]
    markers[source] = False

  with open(original_ebuild, 'r') as original:
    with open(modified_ebuild, 'w') as modified:
      for line in original:
        for source in profiles.keys():
          matched = patterns[source].match(line)
          if matched:
            markers[source] = True
            modified.write(patterns[source].sub(repls[source], line))
            break
        else:  # line without markers, just copy it.
          modified.write(line)

  for source, found in markers.items():
    if not found:
      raise MissingAFDOMarkers('Chrome ebuild file does not have appropriate '
                               'AFDO markers for source %s' % source)

  os.rename(modified_ebuild, original_ebuild)


def UpdateManifest(ebuild_file, ebuild_prog='ebuild'):
  """Regenerate the Manifest file.

  Args:
    ebuild_file: path to the ebuild file
    ebuild_prog: the ebuild command; can be board specific
  """
  gen_manifest_cmd = [ebuild_prog, ebuild_file, 'manifest', '--force']
  cros_build_lib.RunCommand(gen_manifest_cmd, enter_chroot=True, print_cmd=True)


def CommitIfChanged(ebuild_dir, message):
  """If there are changes to ebuild or Manifest, commit them.

  Args:
    ebuild_dir: the path to the directory of ebuild in the chroot
    message: commit message
  """
  # Check if anything changed compared to the previous version.
  modifications = git.RunGit(
      ebuild_dir, ['status', '--porcelain', '-uno'],
      capture_output=True,
      print_cmd=True).output
  if not modifications:
    logging.info('AFDO info for the ebuilds did not change. '
                 'Nothing to commit')
    return

  git.RunGit(ebuild_dir, ['commit', '-a', '-m', message], print_cmd=True)


def UpdateChromeEbuildAFDOFile(board, profiles):
  """Update chrome ebuild with the dictionary of {arch: afdo_file} pairs.

  Modifies the Chrome ebuild to set the appropriate AFDO file for each
  given architecture. Regenerates the associated Manifest file and
  commits the new ebuild and Manifest.

  Args:
    board: board we are building Chrome for.
    profiles: {arch: afdo_file} pairs to put into the ebuild.
              These are profiles from selected benchmarks.
  """
  # Find the Chrome ebuild file.
  equery_prog = 'equery'
  ebuild_prog = 'ebuild'
  if board:
    equery_prog += '-%s' % board
    ebuild_prog += '-%s' % board

  equery_cmd = [equery_prog, 'w', 'chromeos-chrome']
  ebuild_file = cros_build_lib.RunCommand(
      equery_cmd, enter_chroot=True, redirect_stdout=True).output.rstrip()

  # Patch the ebuild file with the names of the available afdo_files.
  PatchChromeEbuildAFDOFile(ebuild_file, profiles)

  # Also patch the 9999 ebuild. This is necessary because the uprev
  # process starts from the 9999 ebuild file and then compares to the
  # current version to see if the uprev is really necessary. We dont
  # want the names of the available afdo_files to show as differences.
  # It also allows developers to do USE=afdo_use when using the 9999
  # ebuild.
  ebuild_9999 = os.path.join(
      os.path.dirname(ebuild_file), 'chromeos-chrome-9999.ebuild')
  PatchChromeEbuildAFDOFile(ebuild_9999, profiles)

  UpdateManifest(ebuild_9999, ebuild_prog)

  ebuild_dir = path_util.FromChrootPath(os.path.dirname(ebuild_file))
  CommitIfChanged(ebuild_dir, 'Update profiles and manifests for Chrome.')


def VerifyLatestAFDOFile(afdo_release_spec, buildroot, gs_context):
  """Verify that the latest AFDO profile for a release is suitable.

  Find the latest AFDO profile file for a particular release and check
  that it is not too stale. The latest AFDO profile name for a release
  can be found in a file in GS under the name
  latest-chrome-<arch>-<release>.afdo.

  Args:
    afdo_release_spec: architecture and release to find the latest AFDO
        profile for.
    buildroot: buildroot where AFDO data should be stored.
    gs_context: GS context to retrieve data.

  Returns:
    The first return value is the name of the AFDO profile file found. None
    otherwise.
    The second return value indicates whether the profile found is expired or
    not. False when no profile is found.
  """
  latest_afdo_url = GSURL_LATEST_CHROME_AFDO % afdo_release_spec

  # Check if latest-chrome-<arch>-<release>.afdo exists.
  try:
    latest_detail = gs_context.List(latest_afdo_url, details=True)
  except gs.GSNoSuchKey:
    logging.info('Could not find latest AFDO info file %s', latest_afdo_url)
    return None, False

  # Then get the name of the latest valid AFDO profile file.
  local_dir = AFDO_BUILDROOT_LOCAL % {'build_root': buildroot}
  latest_afdo_file = LATEST_CHROME_AFDO_FILE % afdo_release_spec
  latest_afdo_path = os.path.join(local_dir, latest_afdo_file)
  gs_context.Copy(latest_afdo_url, latest_afdo_path)

  cand = osutils.ReadFile(latest_afdo_path).strip()
  cand_build = int(cand.split('.')[2])
  curr_build = int(afdo_release_spec['build'])

  # Verify the AFDO profile file is not too stale.
  mod_date = latest_detail[0].creation_time
  curr_date = datetime.datetime.now()
  allowed_stale_days = datetime.timedelta(days=AFDO_ALLOWED_STALE_DAYS)
  if ((curr_date - mod_date) > allowed_stale_days and
      (curr_build - cand_build) > AFDO_ALLOWED_STALE_BUILDS):
    logging.info('Found latest AFDO info file %s but it is too old',
                 latest_afdo_url)
    return cand, True

  return cand, False


def GetBenchmarkProfile(cpv, _source, buildroot, gs_context):
  """Try to find the latest suitable AFDO profile file.

  Try to find the latest AFDO profile generated for current release
  and architecture. If there is none, check the previous release (mostly
  in case we have just branched).

  Args:
    cpv: cpv object for Chrome.
    source: benchmark source for which we are looking
    buildroot: buildroot where AFDO data should be stored.
    gs_context: GS context to retrieve data.

  Returns:
    Name of latest suitable AFDO profile file if one is found.
    None otherwise.
  """

  # Currently, benchmark based profiles can only be generated on amd64.
  arch = 'amd64'
  version_numbers = cpv.version.split('.')
  current_release = version_numbers[0]
  current_build = version_numbers[2]
  afdo_release_spec = {
      'package': cpv.package,
      'arch': arch,
      'release': current_release,
      'build': current_build
  }
  afdo_file, expired = VerifyLatestAFDOFile(afdo_release_spec, buildroot,
                                            gs_context)
  if afdo_file and not expired:
    return afdo_file

  # The profile found in current release is too old. This clearly is a sign of
  # problem. Therefore, don't try to find another one in previous branch.
  if expired:
    return None

  # Could not find suitable AFDO file for the current release.
  # Let's see if there is one from the previous release.
  previous_release = str(int(current_release) - 1)
  prev_release_spec = {
      'package': cpv.package,
      'arch': arch,
      'release': previous_release,
      'build': current_build
  }
  afdo_file, expired = VerifyLatestAFDOFile(prev_release_spec, buildroot,
                                            gs_context)
  if expired:
    return None
  return afdo_file


def UpdateLatestAFDOProfileInGS(cpv, arch, buildroot, profile_name, gs_context):
  """Updates our 'latest profile' file in GS to point to `profile_name`.

  Args:
    cpv: cpv object for Chrome.
    arch: architecture for which we are looking for AFDO profile.
    buildroot: buildroot where AFDO data should be stored.
    profile_name: Name of the profile to point the 'latest profile' file at.
    gs_context: GS context.
  """
  _, local_dir, _ = _BuildrootToWorkDirs(buildroot)

  # Create latest-chrome-<arch>-<release>.afdo pointing to the name
  # of the AFDO profile file and upload to GS.
  current_release = cpv.version.split('.')[0]
  afdo_release_spec = {
      'package': cpv.package,
      'arch': arch,
      'release': current_release
  }
  latest_afdo_file = LATEST_CHROME_AFDO_FILE % afdo_release_spec
  latest_afdo_path = os.path.join(local_dir, latest_afdo_file)
  osutils.WriteFile(latest_afdo_path, profile_name)
  gs_context.Copy(
      latest_afdo_path,
      GSURL_LATEST_CHROME_AFDO % afdo_release_spec,
      acl='public-read')


def GenerateAFDOData(cpv, arch, board, buildroot, gs_context):
  """Generate AFDO profile data from 'perf' data.

  Given the 'perf' profile, generate an AFDO profile using create_llvm_prof.
  It also creates a latest-chrome-<arch>-<release>.afdo file pointing
  to the generated AFDO profile.
  Uploads the generated data to GS for retrieval by the chrome ebuild
  file when doing an 'afdo_use' build.
  It is possible the generated data has previously been uploaded to GS
  in which case this routine will not upload the data again. Uploading
  again may cause verication failures for the ebuild file referencing
  the previous contents of the data.

  Args:
    cpv: cpv object for Chrome.
    arch: architecture for which we are looking for AFDO profile.
    board: board we are building for.
    buildroot: buildroot where AFDO data should be stored.
    gs_context: GS context to retrieve/store data.

  Returns:
    Name of the AFDO profile file generated if successful, and whether or not
    it was uploaded.
  """
  CHROME_UNSTRIPPED_NAME = 'chrome.unstripped'

  version_number = cpv.version
  afdo_spec = {'package': cpv.package, 'arch': arch, 'version': version_number}
  chroot_root, local_dir, in_chroot_local_dir = _BuildrootToWorkDirs(buildroot)

  # Upload compressed chrome debug binary to GS for triaging purposes.
  # TODO(llozano): This simplifies things in case of need of triaging
  # problems but is it really necessary?
  debug_bin = CHROME_DEBUG_BIN % {'root': chroot_root, 'board': board}
  comp_debug_bin_path = CompressAFDOFile(debug_bin, buildroot)
  GSUploadIfNotPresent(gs_context, comp_debug_bin_path,
                       GSURL_CHROME_DEBUG_BIN % afdo_spec)

  # create_llvm_prof demands the name of the profiled binary exactly matches
  # the name of the unstripped binary or it is named 'chrome.unstripped'.
  # So create a symbolic link with the appropriate name.
  local_debug_sym = os.path.join(local_dir, CHROME_UNSTRIPPED_NAME)
  in_chroot_debug_bin = CHROME_DEBUG_BIN % {'root': '', 'board': board}
  osutils.SafeUnlink(local_debug_sym)
  os.symlink(in_chroot_debug_bin, local_debug_sym)

  # Call create_llvm_prof tool to generated AFDO profile from 'perf' profile
  # and upload it to GS. Need to call from within chroot since this tool
  # was built inside chroot.
  debug_sym = os.path.join(in_chroot_local_dir, CHROME_UNSTRIPPED_NAME)
  # The name of the 'perf' file is based only on the version of chrome. The
  # revision number is not included.
  afdo_spec_no_rev = {
      'package': cpv.package,
      'arch': arch,
      'version': cpv.version_no_rev.split('_')[0]
  }
  perf_afdo_file = CHROME_PERF_AFDO_FILE % afdo_spec_no_rev
  perf_afdo_path = os.path.join(in_chroot_local_dir, perf_afdo_file)
  afdo_file = CHROME_AFDO_FILE % afdo_spec
  afdo_path = os.path.join(in_chroot_local_dir, afdo_file)
  afdo_cmd = [
      AFDO_GENERATE_LLVM_PROF,
      '--binary=%s' % debug_sym,
      '--profile=%s' % perf_afdo_path,
      '--out=%s' % afdo_path
  ]
  cros_build_lib.RunCommand(
      afdo_cmd, enter_chroot=True, capture_output=True, print_cmd=True)

  afdo_local_path = os.path.join(local_dir, afdo_file)
  comp_afdo_path = CompressAFDOFile(afdo_local_path, buildroot)
  uploaded_afdo_file = GSUploadIfNotPresent(gs_context, comp_afdo_path,
                                            GSURL_CHROME_AFDO % afdo_spec)
  return afdo_file, uploaded_afdo_file


def CanGenerateAFDOData(board):
  """Does this board has the capability of generating its own AFDO data?."""
  return board in AFDO_DATA_GENERATORS_LLVM


def FindLatestProfile(target, versions):
  """Find latest profile that is usable by the target.

  Args:
    target: the target version
    versions: a list of versions

  Returns:
    latest profile that is older than the target
  """
  cand = bisect.bisect(versions, target) - 1
  if cand >= 0:
    return versions[cand]
  return None


def PatchKernelEbuild(filename, version):
  """Update the AFDO_PROFILE_VERSION string in the given kernel ebuild file.

  Args:
    filename: name of the ebuild
    version: e.g., [61, 9752, 0, 0]
  """
  contents = []
  for line in osutils.ReadFile(filename).splitlines():
    if re.match(KERNEL_PROFILE_MATCH_PATTERN, line):
      contents.append(KERNEL_PROFILE_WRITE_PATTERN % tuple(version) + '\n')
    else:
      contents.append(line + '\n')
  osutils.WriteFile(filename, contents, atomic=True)


def CWPProfileToVersionTuple(url):
  """Convert a CWP profile url to a version tuple

  Args:
    url: for example, gs://chromeos-prebuilt/afdo-job/cwp/chrome/
                      R65-3325.65-1519323840.afdo.xz

  Returns:
    A tuple of (milestone, major, minor, timestamp)
  """
  fn_mat = (
      CWP_CHROME_PROFILE_NAME_PATTERN % tuple(
          r'([0-9]+)' for _ in range(0, 4)))
  fn_mat.replace('.', '\\.')
  return [int(x) for x in re.match(fn_mat, os.path.basename(url)).groups()]


def GetCWPProfile(cpv, source, _buildroot, gs_context):
  """Try to find the latest suitable AFDO profile file for cwp.

  Try to find the latest AFDO profile generated for current release
  and architecture.

  Args:
    cpv: cpv object for Chrome.
    source: profile source
    buildroot: buildroot where AFDO data should be stored.
    gs_context: GS context to retrieve data.

  Returns:
    Name of latest suitable AFDO profile file if one is found.
    None otherwise.
  """
  ver_mat = r'([0-9]+)\.[0-9]+\.([0-9]+)\.([0-9]+)_rc-r[0-9]+'
  target = [int(x) for x in re.match(ver_mat, cpv.version).groups()]

  # Check 2 most recent milestones.
  #
  # When a branch just happens, the milestone of master increases by 1. There
  # will be no profile from that milestone until a dev release is pushed for a
  # short period of time. Therefore, a profile from previous branches must be
  # picked instead.
  #
  # Originally, we search toward root in the branch tree for a profile. Now we
  # prefer to look at the previous milestone if there's no profile from current
  # milestone, because:
  #
  # 1. dev channel has few samples. The profile quality is much better from
  #    beta, which is always in a branch.
  #
  # 2. Master is actually closer to the branch tip than to the branch point,
  #    assuming that most of the changes on a branch are cherry-picked from
  #    master.
  versions = []
  for milestone in (target[0], target[0] - 1):
    gs_ls_url = os.path.join(
        GSURL_BASE_CWP, GSURL_CWP_SUBDIR[source],
        CWP_CHROME_PROFILE_NAME_PATTERN % (milestone, '*', '*', '*'))
    try:
      res = gs_context.List(gs_ls_url)
      versions.extend(CWPProfileToVersionTuple(x) for x in [r.url for r in res])
    except gs.GSNoSuchKey:
      pass

  if not versions:
    logging.info('profile not found for: %s', cpv.version)
    return None

  versions.sort()
  cand = FindLatestProfile(target, versions)
  # reconstruct the filename and strip .xz
  return (CWP_CHROME_PROFILE_NAME_PATTERN % tuple(cand))[:-3]


def GetAvailableKernelProfiles():
  """Get available profiles on specified gsurl.

  Returns:
    a dictionary that maps kernel version, e.g. "4_4" to a list of
    [milestone, major, minor, timestamp]. E.g,
    [62, 9901, 21, 1506581147]
  """

  gs_context = gs.GSContext()
  gs_ls_url = os.path.join(KERNEL_PROFILE_URL, KERNEL_PROFILE_LS_PATTERN)
  gs_match_url = os.path.join(KERNEL_PROFILE_URL, KERNEL_PROFILE_NAME_PATTERN)
  try:
    res = gs_context.List(gs_ls_url)
  except gs.GSNoSuchKey:
    logging.info('gs files not found: %s', gs_ls_url)
    return {}

  all_matches = [re.match(gs_match_url, x.url) for x in res]
  matches = [x for x in all_matches if x]
  versions = {}
  for m in matches:
    versions.setdefault(m.group(1), []).append(
        [int(x) for x in m.groups()[1:]])
  for v in versions:
    versions[v].sort()
  return versions


def FindKernelEbuilds():
  """Find all ebuilds that specify AFDO_PROFILE_VERSION.

  The only assumption is that the ebuild files are named as the match pattern
  in kver(). If it fails to recognize the ebuild filename, an error will be
  thrown.

  equery is not used because that would require enumerating the boards, which
  is no easier than enumerating the kernel versions or ebuilds.

  Returns:
    a list of (ebuilds, kernel rev)
  """

  def kver(ebuild):
    matched = re.match(r'.*/chromeos-kernel-([0-9]+_[0-9]+)-.+\.ebuild$',
                       ebuild)
    if matched:
      return matched.group(1).replace('_', '.')
    raise UnknownKernelVersion(
        'Kernel version cannot be inferred from ebuild filename "%s".' % ebuild)

  for fn in glob.glob(os.path.join(KERNEL_EBUILD_ROOT, '*', '*.ebuild')):
    for line in osutils.ReadFile(fn).splitlines():
      if re.match(KERNEL_PROFILE_MATCH_PATTERN, line):
        yield (fn, kver(fn))
        break


def ProfileAge(profile_version):
  """Tell the age of profile_version in days.

  Args:
    profile_version: [chrome milestone, cros major, cros minor, timestamp]
                     e.g., [61, 9752, 0, 1500000000]

  Returns:
    Age of profile_version in days.
  """
  return (datetime.datetime.utcnow() - datetime.datetime.utcfromtimestamp(
      profile_version[3])).days


PROFILE_SOURCES = {
    'benchmark': GetBenchmarkProfile,
    'silvermont': GetCWPProfile,
    'airmont': GetCWPProfile,
    'broadwell': GetCWPProfile,
}
