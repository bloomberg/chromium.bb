# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains constants used by cbuildbot and related code."""

import os

SOURCE_ROOT = os.path.dirname(os.path.abspath(__file__))
SOURCE_ROOT = os.path.realpath(os.path.join(SOURCE_ROOT, '..', '..'))
CROSUTILS_DIR = os.path.join(SOURCE_ROOT, 'src/scripts')
CHROMITE_BIN_SUBDIR = 'chromite/bin'
CHROMITE_BIN_DIR = os.path.join(SOURCE_ROOT, CHROMITE_BIN_SUBDIR)
PATH_TO_CBUILDBOT = os.path.join(CHROMITE_BIN_SUBDIR, 'cbuildbot')
DEFAULT_CHROOT_DIR = 'chroot'
SDK_TOOLCHAINS_OUTPUT = 'tmp/toolchain-pkgs'

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

GERRIT_PORT = '29418'
GERRIT_INT_PORT = '29419'

GERRIT_HOST = 'gerrit.chromium.org'
GERRIT_INT_HOST = 'gerrit-int.chromium.org'
GIT_HOST = 'git.chromium.org'

GERRIT_SSH_URL = 'ssh://%s:%s' % (GERRIT_HOST, GERRIT_PORT)
GERRIT_INT_SSH_URL = 'ssh://%s:%s' % (GERRIT_INT_HOST, GERRIT_INT_PORT)
GERRIT_HTTP_URL = 'https://%s' % GERRIT_HOST
GIT_HTTP_URL = 'http://%s/git' % GIT_HOST
CHROMIUM_GOOGLESOURCE_URL = 'https://chromium.googlesource.com/'

REPO_PROJECT = 'external/repo'
REPO_URL = '%s/%s' % (GIT_HTTP_URL, REPO_PROJECT)

CHROMITE_PROJECT = 'chromiumos/chromite'
CHROMITE_URL = '%s/%s' % (GIT_HTTP_URL, CHROMITE_PROJECT)
CHROMIUM_SRC_PROJECT = 'chromium/src'

MANIFEST_PROJECT = 'chromiumos/manifest'
MANIFEST_INT_PROJECT = 'chromeos/manifest-internal'

MANIFEST_URL = '%s/%s' % (GIT_HTTP_URL, MANIFEST_PROJECT)
MANIFEST_INT_URL = '%s/%s' % (GERRIT_INT_SSH_URL, MANIFEST_INT_PROJECT)

DEFAULT_MANIFEST = 'default.xml'
SHARED_CACHE_ENVVAR = 'CROS_CACHEDIR'

# CrOS remotes specified in the manifests.
EXTERNAL_REMOTE = 'cros'
INTERNAL_REMOTE = 'cros-internal'
CROS_REMOTES = {
    EXTERNAL_REMOTE : GERRIT_SSH_URL,
    INTERNAL_REMOTE : GERRIT_INT_SSH_URL
}

# TODO(sosa): Move to manifest-versions-external once its created
MANIFEST_VERSIONS_SUFFIX = '/chromiumos/manifest-versions'
MANIFEST_VERSIONS_INT_SUFFIX = '/chromeos/manifest-versions'

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

# TODO(sosa): Deprecate CQ type.
# Commit Queue type that is similar to PFQ_TYPE but uses Commit Queue sync
# logic.
COMMIT_QUEUE_TYPE = 'commit-queue'

# Hybrid Commit and PFQ type.  Ultimate protection.
PALADIN_TYPE = 'paladin'

# Chrome PFQ type.  Incremental build type that builds and validates new
# versions of Chrome.  Only valid if set with CHROME_REV.  See
# VALID_CHROME_REVISIONS for more information.
CHROME_PFQ_TYPE = 'chrome'

# Builds from source and non-incremental.  This builds fully wipe their
# chroot before the start of every build and no not use a BINHOST.
BUILD_FROM_SOURCE_TYPE = 'full'

# Full but with versioned logic.
CANARY_TYPE = 'canary'

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
    REFRESH_PACKAGES_TYPE
)


# Define pool of machines for Hardware tests.
HWTEST_DEFAULT_NUM = 6
HWTEST_TRYBOT_NUM = 1
HWTEST_MACH_POOL = 'bvt'
HWTEST_PALADIN_POOL = 'cq'
HWTEST_CHROME_PFQ_POOL = 'chromepfq'
HWTEST_CHROME_PERF_POOL = 'chromeperf'
HWTEST_TRYBOT_POOL = 'try-bot'
# Currently supported hwtest boards.
HWTEST_BOARD_WHITELIST = ['x86-mario', 'lumpy']
HWTEST_AU_SUITE = 'au'

# Defines VM Test types.
SMOKE_SUITE_TEST_TYPE = 'smoke_suite'
SIMPLE_AU_TEST_TYPE = 'pfq_suite'
FULL_AU_TEST_TYPE = 'full_suite'

VALID_AU_TEST_TYPES = [SMOKE_SUITE_TEST_TYPE, SIMPLE_AU_TEST_TYPE,
                       FULL_AU_TEST_TYPE]

VERSION_FILE = os.path.join('src/third_party/chromiumos-overlay',
                            'chromeos/config/chromeos_version.sh')
SDK_VERSION_FILE = os.path.join('src/third_party/chromiumos-overlay',
                                'chromeos/binhost/host/sdk_version.conf')
SDK_GS_BUCKET = 'chromiumos-sdk'

BOTH_OVERLAYS = 'both'
PUBLIC_OVERLAYS = 'public'
PRIVATE_OVERLAYS = 'private'
VALID_OVERLAYS = [BOTH_OVERLAYS, PUBLIC_OVERLAYS, PRIVATE_OVERLAYS, None]

# Common default logging settings for use with the logging module.
LOGGER_FMT = '%(asctime)s: %(levelname)s: %(message)s'
LOGGER_DATE_FMT = '%H:%M:%S'

# Used by remote patch serialization/deserialzation.
INTERNAL_PATCH_TAG = 'i'
EXTERNAL_PATCH_TAG = 'e'
PATCH_TAGS = (INTERNAL_PATCH_TAG, EXTERNAL_PATCH_TAG)

# Default gerrit query used to find changes for CQ.
DEFAULT_CQ_READY_QUERY = ('status:open AND CodeReview=+2 AND Verified=+1 '
                          'AND CommitQueue=+1 '
                          'AND NOT ( CodeReview=-2 OR Verified=-1 )')

# Some files need permissions set for several distinct groups. A google storage
# acl (xml) file will be necessary in those cases. Make available well known
# locations and standardize.
KNOWN_ACL_FILES = {'slave': os.path.expanduser('~/slave_archive_acl')}

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
PATH_TO_CHROME_LKGM = 'src/chromeos/%s' % CHROME_LKGM_FILE
SVN_CHROME_LKGM = 'trunk/%s' % PATH_TO_CHROME_LKGM

# Cache constants.
COMMON_CACHE = 'common'

# Artifact constants.
def _SlashToUnderscore(string):
  return string.replace('/', '_')

DEFAULT_ARCHIVE_BUCKET = 'gs://chromeos-image-archive'
CHROME_SYSROOT_TAR = 'sysroot_%s.tar.xz' % _SlashToUnderscore(CHROME_CP)
CHROME_ENV_TAR = 'environment_%s.tar.xz' % _SlashToUnderscore(CHROME_CP)
CHROME_ENV_FILE = 'environment'
BASE_IMAGE_NAME = 'chromiumos_base_image'
BASE_IMAGE_TAR = '%s.tar.xz' % BASE_IMAGE_NAME
BASE_IMAGE_BIN = '%s.bin' % BASE_IMAGE_NAME
IMAGE_SCRIPTS_NAME = 'image_scripts'
IMAGE_SCRIPTS_TAR = '%s.tar.xz' % IMAGE_SCRIPTS_NAME
METADATA_JSON = 'metadata.json'

# Global configuration constants.
CHROMITE_CONFIG_DIR = os.path.expanduser('~/.chromite')
CHROME_SDK_BASHRC = os.path.join(CHROMITE_CONFIG_DIR, 'chrome_sdk.bashrc')
SYNC_RETRIES = 2
SLEEP_TIMEOUT = 30
