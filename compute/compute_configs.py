# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains Google Compute Engine configurations."""

from __future__ import print_function

from chromite.cbuildbot import constants


PROJECT = 'chromeos-bot'
DEFAULT_BASE_IMAGE = 'ubuntu-14-04-server-v20141016'
DEFAULT_IMAGE_NAME = 'chromeos-bot-v2'
DEFAULT_ZONE = 'us-east1-a'
DEFAULT_SCOPES = ('https://www.googleapis.com/auth/devstorage.full_control',
                  'https://www.googleapis.com/auth/gerritcodereview')

# TODO: We do not archive the official images to Google Storage yet
# because the imaging creating process for this path does not allow
# the rootfs to be larger than 10GB.
GS_IMAGE_ARCHIVE_BASE_URL = '%s/gce-images' % constants.DEFAULT_ARCHIVE_BUCKET
IMAGE_SUFFIX = '.tar.gz'

BOOT_DISK = '/dev/sda'
# TODO: Automatically detects the partitions.
DRIVES = ('sda1',)


configs = {}

# The default config for Chrome OS builders.
configs['cros-bot'] = dict(
    machine_type='n1-highmem-16',
    zone=DEFAULT_ZONE,
    image=DEFAULT_IMAGE_NAME,
    scopes=DEFAULT_SCOPES,
)

# The default config for Chrome OS PreCQ builders.
configs['cros-precq-bot'] = dict(
    machine_type='n1-highmem-8',
    zone=DEFAULT_ZONE,
    image=DEFAULT_IMAGE_NAME,
    scopes=DEFAULT_SCOPES,
)

# A wimpy config for testing purposes.
configs['cros-test'] = dict(
    machine_type='n1-standard-1',
    zone=DEFAULT_ZONE,
    image=DEFAULT_IMAGE_NAME,
    scopes=DEFAULT_SCOPES,
)
