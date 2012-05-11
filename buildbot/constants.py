# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains constants used by cbuildbot and related code."""

import os

SOURCE_ROOT = os.path.dirname(os.path.abspath(__file__))
SOURCE_ROOT = os.path.realpath(os.path.join(SOURCE_ROOT, '..', '..'))
CROSUTILS_LIB_DIR = os.path.join(SOURCE_ROOT, 'src/scripts/lib')
CHROMITE_BIN_SUBDIR = 'chromite/bin'
CHROMITE_BIN_DIR = os.path.join(SOURCE_ROOT, CHROMITE_BIN_SUBDIR)
DEFAULT_CHROOT_DIR = 'chroot'

REPO_URL = 'http://git.chromium.org/git/external/repo'

GERRIT_PORT = '29418'
GERRIT_INT_PORT = '29419'

GERRIT_HOST = 'gerrit.chromium.org'
GERRIT_INT_HOST = 'gerrit-int.chromium.org'
GIT_HOST = 'git.chromium.org'

GERRIT_SSH_URL = 'ssh://%s:%s' % (GERRIT_HOST, GERRIT_PORT)
GERRIT_INT_SSH_URL = 'ssh://%s:%s' % (GERRIT_INT_HOST, GERRIT_INT_PORT)
GERRIT_HTTP_URL = 'https://%s' % GERRIT_HOST
GIT_HTTP_URL = 'http://%s/git' % GIT_HOST

MANIFEST_URL = GIT_HTTP_URL + '/chromiumos/manifest'
MANIFEST_INT_URL = GERRIT_INT_SSH_URL + '/chromeos/manifest-internal'

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

# Define pool of machines for Hardware tests.
HWTEST_MACH_POOL = 'bvt'

# Defines VM Test types.
SMOKE_SUITE_TEST_TYPE = 'smoke_suite'
SIMPLE_AU_TEST_TYPE = 'pfq_suite'
FULL_AU_TEST_TYPE = 'full_suite'

VALID_AU_TEST_TYPES = [SMOKE_SUITE_TEST_TYPE, SIMPLE_AU_TEST_TYPE,
                       FULL_AU_TEST_TYPE]

VERSION_FILE = os.path.join('src/third_party/chromiumos-overlay',
                            'chromeos/config/chromeos_version.sh')

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
