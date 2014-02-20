# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains constants used by cbuildbot and related code."""

import os

def _FindSourceRoot():
  """Try and find the root check out of the chromiumos tree"""
  source_root = path = os.path.realpath(os.path.join(
      os.path.abspath(__file__), '..', '..', '..'))
  while True:
    if os.path.isdir(os.path.join(path, '.repo')):
      return path
    elif path == '/':
      break
    path = os.path.dirname(path)
  return source_root

SOURCE_ROOT = _FindSourceRoot()
CHROOT_SOURCE_ROOT = '/mnt/host/source'

CROSUTILS_DIR = os.path.join(SOURCE_ROOT, 'src/scripts')
CHROMITE_DIR = os.path.join(SOURCE_ROOT, 'chromite')
CHROMITE_BIN_SUBDIR = 'chromite/bin'
CHROMITE_BIN_DIR = os.path.join(SOURCE_ROOT, CHROMITE_BIN_SUBDIR)
PATH_TO_CBUILDBOT = os.path.join(CHROMITE_BIN_SUBDIR, 'cbuildbot')
DEFAULT_CHROOT_DIR = 'chroot'
SDK_TOOLCHAINS_OUTPUT = 'tmp/toolchain-pkgs'
AUTOTEST_BUILD_PATH = 'usr/local/build/autotest'

# TODO: Eliminate these or merge with manifest_version.py:STATUS_PASSED
# crbug.com/318930
FINAL_STATUS_PASSED = 'passed'
FINAL_STATUS_FAILED = 'failed'

# Re-execution API constants.
# Used by --resume and --bootstrap to decipher which options they
# can pass to the target cbuildbot (since it may not have that
# option).
# Format is Major:Minor.  Minor is used for tracking new options added
# that aren't critical to the older version if it's not ran.
# Major is used for tracking heavy API breakage- for example, no longer
# supporting the --resume option.
REEXEC_API_MAJOR = 0
REEXEC_API_MINOR = 2
REEXEC_API_VERSION = '%i.%i' % (REEXEC_API_MAJOR, REEXEC_API_MINOR)

GOOGLE_EMAIL = '@google.com'
CHROMIUM_EMAIL = '@chromium.org'

CORP_DOMAIN = 'corp.google.com'
GOLO_DOMAIN = 'golo.chromium.org'

GOB_HOST = '%s.googlesource.com'

EXTERNAL_GOB_INSTANCE = 'chromium'
EXTERNAL_GERRIT_INSTANCE = 'chromium-review'
EXTERNAL_GOB_HOST = GOB_HOST % EXTERNAL_GOB_INSTANCE
EXTERNAL_GERRIT_HOST = GOB_HOST % EXTERNAL_GERRIT_INSTANCE
EXTERNAL_GOB_URL = 'https://%s' % EXTERNAL_GOB_HOST
EXTERNAL_GERRIT_URL = 'https://%s' % EXTERNAL_GERRIT_HOST

INTERNAL_GOB_INSTANCE = 'chrome-internal'
INTERNAL_GERRIT_INSTANCE = 'chrome-internal-review'
INTERNAL_GOB_HOST = GOB_HOST % INTERNAL_GOB_INSTANCE
INTERNAL_GERRIT_HOST = GOB_HOST % INTERNAL_GERRIT_INSTANCE
INTERNAL_GOB_URL = 'https://%s' % INTERNAL_GOB_HOST
INTERNAL_GERRIT_URL = 'https://%s' % INTERNAL_GERRIT_HOST

REPO_PROJECT = 'external/repo'
REPO_URL = '%s/%s' % (EXTERNAL_GOB_URL, REPO_PROJECT)

CHROMITE_PROJECT = 'chromiumos/chromite'
CHROMITE_URL = '%s/%s' % (EXTERNAL_GOB_URL, CHROMITE_PROJECT)
CHROMIUM_SRC_PROJECT = 'chromium/src'
CHROMIUM_GOB_URL = '%s/%s.git' % (EXTERNAL_GOB_URL, CHROMIUM_SRC_PROJECT)

MANIFEST_PROJECT = 'chromiumos/manifest'
MANIFEST_INT_PROJECT = 'chromeos/manifest-internal'
MANIFEST_PROJECTS = (MANIFEST_PROJECT, MANIFEST_INT_PROJECT)

MANIFEST_URL = '%s/%s' % (EXTERNAL_GOB_URL, MANIFEST_PROJECT)
MANIFEST_INT_URL = '%s/%s' % (INTERNAL_GERRIT_URL, MANIFEST_INT_PROJECT)

DEFAULT_MANIFEST = 'default.xml'
OFFICIAL_MANIFEST = 'official.xml'
SHARED_CACHE_ENVVAR = 'CROS_CACHEDIR'

# CrOS remotes specified in the manifests.
EXTERNAL_REMOTE = 'cros'
INTERNAL_REMOTE = 'cros-internal'
CHROMIUM_REMOTE = 'chromium'
CHROME_REMOTE = 'chrome'

CROS_REMOTES = {
    EXTERNAL_REMOTE: EXTERNAL_GOB_URL,
    INTERNAL_REMOTE: INTERNAL_GOB_URL,
}

GIT_REMOTES = {
    CHROMIUM_REMOTE: EXTERNAL_GOB_URL,
    CHROME_REMOTE: INTERNAL_GOB_URL,
}
GIT_REMOTES.update(CROS_REMOTES)

# List of remotes that are ok to include in the external manifest.
EXTERNAL_REMOTES = (EXTERNAL_REMOTE, CHROMIUM_REMOTE)

# Mapping 'remote name' -> regexp that matches names of repositories on that
# remote that can be branched when creating CrOS branch. Branching script will
# actually create a new git ref when branching these projects. It won't attempt
# to create a git ref for other projects that may be mentioned in a manifest.
BRANCHABLE_PROJECTS = {
    EXTERNAL_REMOTE: r'chromiumos/(.+)',
    INTERNAL_REMOTE: r'chromeos/(.+)',
}

# TODO(sosa): Move to manifest-versions-external once its created
MANIFEST_VERSIONS_SUFFIX = '/chromiumos/manifest-versions'
MANIFEST_VERSIONS_INT_SUFFIX = '/chromeos/manifest-versions'
MANIFEST_VERSIONS_GS_URL = 'gs://chromeos-manifest-versions'
TRASH_BUCKET = 'gs://chromeos-throw-away-bucket'

STREAK_COUNTERS = 'streak_counters'

PATCH_BRANCH = 'patch_branch'
STABLE_EBUILD_BRANCH = 'stabilizing_branch'
MERGE_BRANCH = 'merge_branch'

# These branches are deleted at the beginning of every buildbot run.
CREATED_BRANCHES = [
    PATCH_BRANCH,
    STABLE_EBUILD_BRANCH,
    MERGE_BRANCH
]

# Constants for uprevving Chrome

# Portage category and package name for Chrome.
CHROME_PN = 'chromeos-chrome'
CHROME_CP = 'chromeos-base/%s' % CHROME_PN

# Chrome URL where PGO data is stored.
CHROME_PGO_URL = ('gs://chromeos-prebuilt/pgo-job/canonicals/'
                  '%(package)s-%(arch)s-%(version_no_rev)s.pgo.tar.bz2')

# Chrome use flags
USE_CHROME_INTERNAL = 'chrome_internal'
USE_CHROME_PDF = 'chrome_pdf'
USE_PGO_GENERATE = 'pgo_generate'
USE_PGO_USE = 'pgo_use'

# PGO-specific constants.
PGO_GENERATE_DISK_LAYOUT = '2gb-rootfs-updatable'
PGO_USE_TIMEOUT = 180 * 60

# Builds and validates _alpha ebuilds.  These builds sync to the latest
# revsion of the Chromium src tree and build with that checkout.
CHROME_REV_TOT = 'tot'

# Builds and validates chrome at a given revision through cbuildbot
# --chrome_version
CHROME_REV_SPEC = 'spec'

# Builds and validates the latest Chromium release as defined by
# ~/trunk/releases in the Chrome src tree.  These ebuilds are suffixed with rc.
CHROME_REV_LATEST = 'latest_release'

# Builds and validates the latest Chromium release for a specific Chromium
# branch that we want to watch.  These ebuilds are suffixed with rc.
CHROME_REV_STICKY = 'stable_release'

# Builds and validates Chromium for a pre-populated directory.
# Also uses _alpha, since portage doesn't have anything lower.
CHROME_REV_LOCAL = 'local'
VALID_CHROME_REVISIONS = [CHROME_REV_TOT, CHROME_REV_LATEST,
                          CHROME_REV_STICKY, CHROME_REV_LOCAL, CHROME_REV_SPEC]


# Build types supported.

# TODO(sosa): Deprecate PFQ type.
# Incremental builds that are built using binary packages when available.
# These builds have less validation than other build types.
INCREMENTAL_TYPE = 'binary'

# These builds serve as PFQ builders.  This is being deprecated.
PFQ_TYPE = 'pfq'

# Hybrid Commit and PFQ type.  Ultimate protection.  Commonly referred to
# as simply "commit queue" now.
PALADIN_TYPE = 'paladin'

# A builder that kicks off Pre-CQ builders that bless the purest CLs.
PRE_CQ_LAUNCHER_TYPE = 'priest'

# A builder that cuts and prunes branches.
CREATE_BRANCH_TYPE = 'gardener'

# Chrome PFQ type.  Incremental build type that builds and validates new
# versions of Chrome.  Only valid if set with CHROME_REV.  See
# VALID_CHROME_REVISIONS for more information.
CHROME_PFQ_TYPE = 'chrome'

# Builds from source and non-incremental.  This builds fully wipe their
# chroot before the start of every build and no not use a BINHOST.
BUILD_FROM_SOURCE_TYPE = 'full'

# Full but with versioned logic.
CANARY_TYPE = 'canary'

BRANCH_UTIL_CONFIG = 'branch-util'

# Special build type for Chroot builders.  These builds focus on building
# toolchains and validate that they work.
CHROOT_BUILDER_TYPE = 'chroot'
CHROOT_BUILDER_BOARD = 'amd64-host'

# Build that refreshes the online Portage package status spreadsheet.
REFRESH_PACKAGES_TYPE = 'refresh_packages'

VALID_BUILD_TYPES = (
    PALADIN_TYPE,
    INCREMENTAL_TYPE,
    BUILD_FROM_SOURCE_TYPE,
    CANARY_TYPE,
    CHROOT_BUILDER_TYPE,
    CHROOT_BUILDER_BOARD,
    CHROME_PFQ_TYPE,
    PFQ_TYPE,
    PRE_CQ_LAUNCHER_TYPE,
    REFRESH_PACKAGES_TYPE,
    CREATE_BRANCH_TYPE,
)

# The name of the builder used to launch the pre-CQ.
PRE_CQ_BUILDER_NAME = 'pre-cq-group'

# The name of the Pre-CQ launcher on the waterfall.
PRE_CQ_LAUNCHER_NAME = 'Pre-CQ Launcher'

# Define pool of machines for Hardware tests.
HWTEST_DEFAULT_NUM = 6
HWTEST_TRYBOT_NUM = 3
HWTEST_MACH_POOL = 'bvt'
HWTEST_PALADIN_POOL = 'cq'
HWTEST_PFQ_POOL = 'pfq'
HWTEST_SUITES_POOL = 'suites'
HWTEST_CHROME_PERF_POOL = 'chromeperf'
HWTEST_TRYBOT_POOL = 'try-bot'
HWTEST_AU_SUITE = 'au'
HWTEST_QAV_SUITE = 'qav'

HWTEST_TIMEOUT_EXTENSION = 10 * 60
HWTEST_DEFAULT_PRIORITY = 'DEFAULT'
HWTEST_CQ_PRIORITY = 'CQ'
HWTEST_BUILD_PRIORITY = 'Build'
HWTEST_PFQ_PRIORITY = 'PFQ'
HWTEST_VALID_PRIORITIES = ['Weekly',
                           'Daily',
                           'PostBuild',
                           HWTEST_DEFAULT_PRIORITY,
                           HWTEST_BUILD_PRIORITY,
                           HWTEST_PFQ_PRIORITY,
                           HWTEST_CQ_PRIORITY]

# Defines VM Test types.
FULL_AU_TEST_TYPE = 'full_suite'
SIMPLE_AU_TEST_TYPE = 'pfq_suite'
SMOKE_SUITE_TEST_TYPE = 'smoke_suite'
TELEMETRY_SUITE_TEST_TYPE = 'telemetry_suite'

VALID_VM_TEST_TYPES = [FULL_AU_TEST_TYPE, SIMPLE_AU_TEST_TYPE,
                       SMOKE_SUITE_TEST_TYPE, TELEMETRY_SUITE_TEST_TYPE]

CHROMIUMOS_OVERLAY_DIR = 'src/third_party/chromiumos-overlay'
VERSION_FILE = os.path.join(CHROMIUMOS_OVERLAY_DIR,
                            'chromeos/config/chromeos_version.sh')
SDK_VERSION_FILE = os.path.join(CHROMIUMOS_OVERLAY_DIR,
                                'chromeos/binhost/host/sdk_version.conf')
SDK_GS_BUCKET = 'chromiumos-sdk'

PUBLIC = 'public'
PRIVATE = 'private'

BOTH_OVERLAYS = 'both'
PUBLIC_OVERLAYS = PUBLIC
PRIVATE_OVERLAYS = PRIVATE
VALID_OVERLAYS = [BOTH_OVERLAYS, PUBLIC_OVERLAYS, PRIVATE_OVERLAYS, None]

# Common default logging settings for use with the logging module.
LOGGER_FMT = '%(asctime)s: %(levelname)s: %(message)s'
LOGGER_DATE_FMT = '%H:%M:%S'

# Used by remote patch serialization/deserialzation.
INTERNAL_PATCH_TAG = 'i'
EXTERNAL_PATCH_TAG = 'e'
PATCH_TAGS = (INTERNAL_PATCH_TAG, EXTERNAL_PATCH_TAG)

# Tree status strings
TREE_OPEN = 'open'
TREE_THROTTLED = 'throttled'
TREE_CLOSED = 'closed'

_GERRIT_QUERY_TEMPLATE = ('status:open AND '
                          'label:Code-Review=+2 AND '
                          'label:Verified=+1 AND '
                          'label:Commit-Queue>=%+i AND '
                          'NOT ( label:CodeReview=-2 OR label:Verified=-1 OR '
                          'is:draft )')

# Default gerrit query used to find changes for CQ.
# Permits CQ+1 or CQ+2 changes.
DEFAULT_CQ_READY_QUERY = _GERRIT_QUERY_TEMPLATE % 1

# Gerrit query used to find changes for CQ when tree is throttled.
# Permits only CQ+2 changes.
THROTTLED_CQ_READY_QUERY = _GERRIT_QUERY_TEMPLATE % 2

# Default filter rules for verifying that Gerrit returned results that matched
# our query. This used for working around Gerrit bugs.
DEFAULT_CQ_READY_FIELDS = {
    'CRVW': '2',
    'VRIF': '1',
    'COMR': ('1', '2'),
}

DEFAULT_CQ_SHOULD_REJECT_FIELDS = {
    'CRVW': '-2',
    'VRIF': '-1',
}

GERRIT_ON_BORG_LABELS = {
    'Code-Review': 'CRVW',
    'Commit-Queue': 'COMR',
    'Verified': 'VRIF',
    'Trybot-Verified': 'TBVF',
}

# Environment variables that should be exposed to all children processes
# invoked via cros_build_lib.RunCommand.
ENV_PASSTHRU = ('CROS_SUDO_KEEP_ALIVE', SHARED_CACHE_ENVVAR)

# List of variables to proxy into the chroot from the host, and to
# have sudo export if existent. Anytime this list is modified, a new
# chroot_version_hooks.d upgrade script that symlinks to 45_rewrite_sudoers.d
# should be created.
CHROOT_ENVIRONMENT_WHITELIST = (
  'CHROMEOS_OFFICIAL',
  'CHROMEOS_VERSION_AUSERVER',
  'CHROMEOS_VERSION_DEVSERVER',
  'CHROMEOS_VERSION_TRACK',
  'GCC_GITHASH',
  'GIT_AUTHOR_EMAIL',
  'GIT_AUTHOR_NAME',
  'GIT_COMMITTER_EMAIL',
  'GIT_COMMITTER_NAME',
  'GIT_PROXY_COMMAND',
  'GIT_SSH',
  'RSYNC_PROXY',
  'SSH_AGENT_PID',
  'SSH_AUTH_SOCK',
  'USE',
  'all_proxy',
  'ftp_proxy',
  'http_proxy',
  'https_proxy',
  'no_proxy',
)

# Paths for Chrome LKGM which are relative to the Chromium base url.
CHROME_LKGM_FILE = 'CHROMEOS_LKGM'
PATH_TO_CHROME_LKGM = 'chromeos/%s' % CHROME_LKGM_FILE
SVN_CHROME_LKGM = 'trunk/src/%s' % PATH_TO_CHROME_LKGM

# Cache constants.
COMMON_CACHE = 'common'

# Artifact constants.
def _SlashToUnderscore(string):
  return string.replace('/', '_')

DEFAULT_ARCHIVE_BUCKET = 'gs://chromeos-image-archive'
RELEASE_BUCKET = 'gs://chromeos-releases'
TRASH_BUCKET = 'gs://chromeos-throw-away-bucket'
CHROME_SYSROOT_TAR = 'sysroot_%s.tar.xz' % _SlashToUnderscore(CHROME_CP)
CHROME_ENV_TAR = 'environment_%s.tar.xz' % _SlashToUnderscore(CHROME_CP)
CHROME_ENV_FILE = 'environment'
BASE_IMAGE_NAME = 'chromiumos_base_image'
BASE_IMAGE_TAR = '%s.tar.xz' % BASE_IMAGE_NAME
BASE_IMAGE_BIN = '%s.bin' % BASE_IMAGE_NAME
IMAGE_SCRIPTS_NAME = 'image_scripts'
IMAGE_SCRIPTS_TAR = '%s.tar.xz' % IMAGE_SCRIPTS_NAME
VM_IMAGE_NAME = 'chromiumos_qemu_image'
VM_IMAGE_BIN = '%s.bin' % VM_IMAGE_NAME
VM_DISK_PREFIX = 'chromiumos_qemu_disk.bin'
VM_MEM_PREFIX = 'chromiumos_qemu_mem.bin'
VM_TEST_RESULTS = 'vm_test_results_%(attempt)s.tgz'

METADATA_JSON = 'metadata.json'
METADATA_STAGE_JSON = 'metadata_%(stage)s.json'
DELTA_SYSROOT_TAR = 'delta_sysroot.tar.xz'
DELTA_SYSROOT_BATCH = 'batch'

# Global configuration constants.
CHROMITE_CONFIG_DIR = os.path.expanduser('~/.chromite')
CHROME_SDK_BASHRC = os.path.join(CHROMITE_CONFIG_DIR, 'chrome_sdk.bashrc')
SYNC_RETRIES = 2
SLEEP_TIMEOUT = 30

# Lab status url.
LAB_STATUS_URL = 'http://chromiumos-lab.appspot.com/current?format=json'

GOLO_SMTP_SERVER = 'mail.golo.chromium.org'

# URLs to the various waterfalls.
BUILD_DASHBOARD = 'http://build.chromium.org/p/chromiumos'
BUILD_INT_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromeos'
TRYBOT_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromiumos.tryserver'

# Email validation regex. Not quite fully compliant with RFC 2822, but good
# approximation.
EMAIL_REGEX = r'[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,4}'
