# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains Google Compute Engine configurations."""

from __future__ import print_function

from chromite.cbuildbot import config_lib


# Metadata keys to tag our GCE artifacts with.
METADATA_IMAGE_NAME = 'cros-image'

PROJECT = 'chromeos-bot'
DEFAULT_BASE_IMAGE = 'ubuntu-14-04-server-v20150324'
DEFAULT_IMAGE_NAME = 'chromeos-bot-v5'
DEFAULT_ZONE = 'us-east1-a'
DEFAULT_MACHINE_TYPE_16 = 'n1-highmem-16'
DEFAULT_MACHINE_TYPE_32 = 'n1-highmem-32'
DEFAULT_MACHINE_TYPE_64 = 'n1-standard-64'
DEFAULT_SCOPES = ('https://www.googleapis.com/auth/devstorage.full_control',
                  'https://www.googleapis.com/auth/gerritcodereview')

# TODO: We do not archive the official images to Google Storage yet
# because the imaging creating process for this path does not allow
# the rootfs to be larger than 10GB.
GS_IMAGE_ARCHIVE_BASE_URL = ('%s/gce-images' %
                             config_lib.GetConfig().params.ARCHIVE_URL)
IMAGE_SUFFIX = '.tar.gz'

BOOT_DISK = '/dev/sda'
# TODO: Automatically detects the partitions.
DRIVES = ('sda1',)


configs = {}

configs['image-creation'] = dict(
    zone=DEFAULT_ZONE,
    scopes=DEFAULT_SCOPES,
)
IMAGE_CREATION_CONFIG = configs['image-creation']

# The default config for Chrome OS builders.
configs['cros-bot'] = dict(
    machine_type=DEFAULT_MACHINE_TYPE_16,
    zone=DEFAULT_ZONE,
    image=DEFAULT_IMAGE_NAME,
    scopes=DEFAULT_SCOPES,
)

# The default config for Chrome OS PreCQ builders.
configs['cros-precq-bot'] = dict(
    machine_type=DEFAULT_MACHINE_TYPE_16,
    zone=DEFAULT_ZONE,
    image=DEFAULT_IMAGE_NAME,
    scopes=DEFAULT_SCOPES,
)

# A light-weight config for light jobs, like boardless masters.
configs['cros-master'] = dict(
    machine_type='n1-standard-8',
    zone=DEFAULT_ZONE,
    image=DEFAULT_IMAGE_NAME,
    scopes=DEFAULT_SCOPES,
)

# A 32-core config for resource intensive builders (such as chrome continuous).
configs['cros-chrome-continuous-bot'] = dict(
    machine_type=DEFAULT_MACHINE_TYPE_32,
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

# Config to use to launch an instance with the image created for the purposes of
# testing changes to cros_compute.
configs['cros-bot-testing'] = dict(
    machine_type=DEFAULT_MACHINE_TYPE_16,
    zone=DEFAULT_ZONE,
    image='%s-testing' % DEFAULT_IMAGE_NAME,
    scopes=DEFAULT_SCOPES,
)

# Same as cros-bot, but launch in the staging network.
configs['cros-bot-staging'] = dict(
    machine_type=DEFAULT_MACHINE_TYPE_16,
    zone=DEFAULT_ZONE,
    image=DEFAULT_IMAGE_NAME,
    scopes=DEFAULT_SCOPES,
    network='staging-network',
)

# Same as cros-bot, but launch in the staging network.
configs['cros-bot-release'] = dict(
    machine_type=DEFAULT_MACHINE_TYPE_64,
    zone='us-east1-b',
    image=DEFAULT_IMAGE_NAME,
    scopes=DEFAULT_SCOPES,
)
