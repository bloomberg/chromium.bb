# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains constants for Chrome OS bots."""

from __future__ import print_function

import os


BUILDBOT_DIR = '/b'
BUILDBOT_USER = 'chrome-bot'

CHROMITE_URL = 'https://chromium.googlesource.com/chromiumos/chromite'
DEPOT_TOOLS_URL = ('https://chromium.googlesource.com/chromium/tools/'
                   'depot_tools.git')
BUILDBOT_SVN_REPO = ('svn://svn.chromium.org/chrome-internal/trunk/tools/build/'
                     'internal.DEPS')
CHROMIUM_BUILD_URL = 'https://chromium.googlesource.com/chromium/src/build'
GCOMPUTE_TOOLS_URL = 'https://gerrit.googlesource.com/gcompute-tools'

# The BOT_CREDS_DIR is required to set up a GCE bot. The directory
# should contain:
#   - SVN_PASSWORD_FILE_: password for svn.
#   - TREE_STATUS_PASSWORD_FILE: password for updating tree status.
#   - CIDB_CREDS_DIR: A directory containing cidb credentials.
#   - BUILDBOT_PASSWORD_FILE: password for buildbot.
#   - HOST_ENTRIES: entries to append to /etc/hosts
BOT_CREDS_DIR_ENV_VAR = 'BOT_CREDENTIALS_DIR'
SVN_PASSWORD_FILE = 'svn_password'
TREE_STATUS_PASSWORD_FILE = '.status_password_chromiumos'
CIDB_CREDS_DIR = '.cidb_creds'
BUILDBOT_PASSWORD_FILE = '.bot_password'
HOST_ENTRIES = 'host_entries'

# This path is used to store credentials on the GCE machine during botifying.
BOT_CREDS_TMP_PATH = os.path.join(os.path.sep, 'tmp', 'bot-credentials')

BUILDBOT_SVN_USER = '%s@google.com' % BUILDBOT_USER
CHROMIUM_SVN_HOSTS = ('svn.chromium.org',)
CHROMIUM_SVN_REPOS = ('chrome', 'chrome-internal', 'leapfrog-internal')

GIT_USER_NAME = 'chrome-bot'
GIT_USER_EMAIL = '%s@chromium.org' % GIT_USER_NAME
