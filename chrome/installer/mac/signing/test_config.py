# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import imp

config = imp.load_source('signing.config', './signing/config.py.in')


class TestConfig(config.CodeSignConfig):

    def __init__(self, identity='[IDENTITY]', keychain='[KEYCHAIN]'):
        super(TestConfig, self).__init__(identity, keychain)

    @property
    def app_product(self):
        return 'App Product'

    @property
    def product(self):
        return 'Product'

    @property
    def version(self):
        return '99.0.9999.99'

    @property
    def base_bundle_id(self):
        return 'test.signing.bundle_id'

    @property
    def optional_parts(self):
        return set()

    @property
    def provisioning_profile_basename(self):
        return 'provisiontest'

    @property
    def run_spctl_assess(self):
        return True
