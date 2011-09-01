# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains constants used by cbuildbot and related code."""

import os

SOURCE_ROOT = os.path.join(os.path.dirname(__file__), '..', '..')
CROSUTILS_LIB_DIR = os.path.join(SOURCE_ROOT, 'src/scripts/lib')

REPO_URL = 'http://git.chromium.org/external/tools/repo.git'

GERRIT_PORT = '29418'
GERRIT_INT_PORT = '29419'

GERRIT_HOST = 'gerrit.chromium.org'
GERRIT_INT_HOST = 'gerrit-int.chromium.org'

GERRIT_SSH_URL = 'ssh://%s:%s' % (GERRIT_HOST, GERRIT_PORT)
GERRIT_INT_SSH_URL = 'ssh://%s:%s' % (GERRIT_INT_HOST, GERRIT_INT_PORT)
GERRIT_HTTP_URL = 'http://%s' % GERRIT_HOST

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

# Builds and validates _alpha ebuilds.  Thesee builds sync to a particular
# revsion of the Chromium src tree and build with that checkout.
CHROME_REV_TOT = 'tot'

# Builds and validates the latest Chromium release as defined by
# ~/trunk/releases in the Chrome src tree.  These ebuilds are suffixed with rc.
CHROME_REV_LATEST = 'latest_release'

# Builds and validates the latest Chromium release for a specific Chromium
# branch that we want to watch.  These ebuilds are suffixed with rc.
CHROME_REV_STICKY = 'stable_release'
VALID_CHROME_REVISIONS = [CHROME_REV_TOT, CHROME_REV_LATEST,
                          CHROME_REV_STICKY]

# Build types supported.

# Incremental builds that are built using binary packages when available.
# These builds have less validation than other build types.  If manifest_version
# is True, these builds serve as PFQ builders.  Use of this type without
# manifest_version is deprecated.
PFQ_TYPE = 'binary'

# Commit Queue type that is similar to PFQ_TYPE but uses Commit Queue sync
# logic.
COMMIT_QUEUE_TYPE = 'paladin'

# Chrome PFQ type.  Incremental build type that builds and validates new
# versions of Chrome.  Only valid if set with CHROME_REV.  See
# VALID_CHROME_REVISIONS for more information.
CHROME_PFQ_TYPE = 'chrome'

# Builds from source and non-incremental.  This builds fully wipe their
# chroot before the start of every build and no not use a BINHOST.
BUILD_FROM_SOURCE_TYPE = 'full'

# Special build type for Chroot builders.  These builds focus on building
# toolchains and validate that they work.
CHROOT_BUILDER_TYPE = 'chroot'

# Build that refreshes the online Portage package status spreadsheet.
REFRESH_PACKAGES_TYPE = 'refresh_packages'

VALID_BUILD_TYPES = [PFQ_TYPE, COMMIT_QUEUE_TYPE, CHROME_PFQ_TYPE,
                     BUILD_FROM_SOURCE_TYPE, CHROOT_BUILDER_TYPE,
                     REFRESH_PACKAGES_TYPE]

VERSION_FILE = os.path.join('src/third_party/chromiumos-overlay',
                            'chromeos/config/chromeos_version.sh')
