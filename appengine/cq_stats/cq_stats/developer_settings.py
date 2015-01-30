# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Cofiguration constants for local development of this app."""

from __future__ import print_function

import os

# This file contains the few configuration parameters that users need to set in
# their development environment to work on this app.
_BASE_DIR = os.path.dirname(os.path.dirname(__file__))

# Absolute path to the directory containing cidb credentials to use.
# Make sure you develop against a local mysql server / test cidb instance.
# DO NOT DEVELOP AGAINST DEBUG / PROD CIDB instance.
# We recommend creating a symlink to your cidb test credentials directory at
# chromite/appengine/cq_stats/test_cidb_credentials.
CIDB_CREDS_DIR = os.path.join(_BASE_DIR, 'test_cidb_credentials')

# Determines which mysql server is used.  Options are:
#   local_mysql: Use a mysql server running at localhost. It's the developer's
#       responsibility to setup the database to have the right schema.
#       TODO(pprabhu): Support creating schema using cidb_integration_test.
#   cidb_instance: Use the cidb instance pointed to by CIDB_CREDS_DIR. Again,
#       please be mindful of only develop against the test instance.
#       Your local IP must be in a whitelisted block by cidb instance you're
#       working against for this mode.
SETTINGS_MODE = 'local_mysql'

# Set this to True in development to allow running the app from localhost, and
# to get more logging.
FORCE_DEBUG = False
