# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

SOURCE_ROOT = os.path.join(os.path.dirname(__file__), '..', '..')
CROSUTILS_LIB_DIR = os.path.join(SOURCE_ROOT, 'src/scripts/lib')

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
CHROME_REV_TOT = 'tot'
CHROME_REV_LATEST = 'latest_release'
CHROME_REV_STICKY = 'stable_release'
VALID_CHROME_REVISIONS = [CHROME_REV_TOT, CHROME_REV_LATEST, CHROME_REV_STICKY]

VERSION_FILE = os.path.join('src/third_party/chromiumos-overlay',
                            'chromeos/config/chromeos_version.sh')
