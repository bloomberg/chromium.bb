# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from . import model, parts, signing, test_common, test_config

mock = test_common.import_mock()


class TestGetParts(unittest.TestCase):

    def test_get_parts_no_base(self):
        config = test_config.TestConfig()
        all_parts = parts.get_parts(config)
        self.assertEqual('test.signing.bundle_id', all_parts['app'].identifier)
        self.assertEqual('test.signing.bundle_id.framework',
                         all_parts['framework'].identifier)
        self.assertEqual(
            'test.signing.bundle_id.framework.AlertNotificationService',
            all_parts['notification-xpc'].identifier)
        self.assertEqual('test.signing.bundle_id.helper',
                         all_parts['helper-app'].identifier)

    def test_get_parts_no_customize(self):
        config = model.Distribution(channel='dev').to_config(
            test_config.TestConfig())
        all_parts = parts.get_parts(config)
        self.assertEqual('test.signing.bundle_id', all_parts['app'].identifier)
        self.assertEqual('test.signing.bundle_id.framework',
                         all_parts['framework'].identifier)
        self.assertEqual(
            'test.signing.bundle_id.framework.AlertNotificationService',
            all_parts['notification-xpc'].identifier)
        self.assertEqual('test.signing.bundle_id.helper',
                         all_parts['helper-app'].identifier)

    def test_get_parts_customize(self):
        config = model.Distribution(
            channel='canary',
            channel_customize=True).to_config(test_config.TestConfig())
        all_parts = parts.get_parts(config)
        self.assertEqual('test.signing.bundle_id.canary',
                         all_parts['app'].identifier)
        self.assertEqual('test.signing.bundle_id.framework',
                         all_parts['framework'].identifier)
        self.assertEqual(
            'test.signing.bundle_id.canary.framework.AlertNotificationService',
            all_parts['notification-xpc'].identifier)
        self.assertEqual('test.signing.bundle_id.helper',
                         all_parts['helper-app'].identifier)

    def test_part_options(self):
        all_parts = parts.get_parts(test_config.TestConfig())
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT +
                model.CodeSignOptions.LIBRARY_VALIDATION +
                model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(all_parts['app'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT +
                model.CodeSignOptions.LIBRARY_VALIDATION +
                model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(all_parts['helper-app'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT + model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(all_parts['helper-renderer-app'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT + model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(all_parts['helper-gpu-app'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT + model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(all_parts['helper-plugin-app'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT +
                model.CodeSignOptions.LIBRARY_VALIDATION +
                model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(all_parts['crashpad'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT +
                model.CodeSignOptions.LIBRARY_VALIDATION +
                model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(all_parts['notification-xpc'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT +
                model.CodeSignOptions.LIBRARY_VALIDATION +
                model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(all_parts['app-mode-app'].options))


def _get_plist_read(other_version):

    def _plist_read(*args):
        path = args[0]
        first_slash = path.find('/')
        path = path[first_slash + 1:]

        plists = {
            'App Product.app/Contents/Info.plist': {
                'KSVersion': '99.0.9999.99'
            },
            'App Product.app/Contents/Frameworks/Product Framework.framework/Resources/Info.plist':
                {
                    'CFBundleShortVersionString': other_version
                }
        }
        return plists[path]

    return _plist_read


@mock.patch.multiple('signing.signing',
                     **{m: mock.DEFAULT for m in ('sign_part', 'verify_part')})
@mock.patch.multiple('signing.commands', **{
    m: mock.DEFAULT
    for m in ('copy_files', 'move_file', 'make_dir', 'run_command')
})
class TestSignChrome(unittest.TestCase):

    def setUp(self):
        self.paths = model.Paths('$I', '$O', '$W')

    @mock.patch('signing.parts._sanity_check_version_keys')
    def test_sign_chrome(self, *args, **kwargs):
        manager = mock.Mock()
        for kwarg in kwargs:
            manager.attach_mock(kwargs[kwarg], kwarg)

        dist = model.Distribution()
        config = dist.to_config(test_config.TestConfig())

        parts.sign_chrome(self.paths, config, sign_framework=True)

        # No files should be moved.
        self.assertEqual(0, kwargs['move_file'].call_count)

        # Test that the provisioning profile is copied.
        self.assertEqual(kwargs['copy_files'].mock_calls, [
            mock.call.copy_files(
                '$I/Product Packaging/provisiontest.provisionprofile',
                '$W/App Product.app/Contents/embedded.provisionprofile')
        ])

        # Ensure that all the parts are signed.
        signed_paths = [
            call[1][2].path for call in kwargs['sign_part'].mock_calls
        ]
        self.assertEqual(
            set([p.path for p in parts.get_parts(config).values()]),
            set(signed_paths))

        # Make sure that the framework and the app are the last two parts that
        # are signed.
        self.assertEqual(signed_paths[-2:], [
            'App Product.app/Contents/Frameworks/Product Framework.framework',
            'App Product.app'
        ])

        self.assertEqual(kwargs['run_command'].mock_calls, [
            mock.call.run_command([
                'codesign', '--display', '--requirements', '-', '--verbose=5',
                '$W/App Product.app'
            ]),
            mock.call.run_command(
                ['spctl', '--assess', '-vv', '$W/App Product.app']),
        ])

    @mock.patch('signing.parts._sanity_check_version_keys')
    def test_sign_chrome_no_assess(self, *args, **kwargs):
        dist = model.Distribution()

        class Config(test_config.TestConfig):

            @property
            def run_spctl_assess(self):
                return False

        config = dist.to_config(Config())

        parts.sign_chrome(self.paths, config, sign_framework=True)

        self.assertEqual(kwargs['run_command'].mock_calls, [
            mock.call.run_command([
                'codesign', '--display', '--requirements', '-', '--verbose=5',
                '$W/App Product.app'
            ]),
        ])

    @mock.patch('signing.parts._sanity_check_version_keys')
    def test_sign_chrome_no_provisioning(self, *args, **kwargs):
        dist = model.Distribution()

        class Config(test_config.TestConfig):

            @property
            def provisioning_profile_basename(self):
                return None

        config = dist.to_config(Config())
        parts.sign_chrome(self.paths, config, sign_framework=True)

        self.assertEqual(0, kwargs['copy_files'].call_count)

    @mock.patch('signing.parts._sanity_check_version_keys')
    def test_sign_chrome_no_framework(self, *args, **kwargs):
        manager = mock.Mock()
        for kwarg in kwargs:
            manager.attach_mock(kwargs[kwarg], kwarg)

        dist = model.Distribution()
        config = dist.to_config(test_config.TestConfig())

        parts.sign_chrome(self.paths, config, sign_framework=False)

        # No files should be moved.
        self.assertEqual(0, kwargs['move_file'].call_count)

        # Test that the provisioning profile is copied.
        self.assertEqual(kwargs['copy_files'].mock_calls, [
            mock.call.copy_files(
                '$I/Product Packaging/provisiontest.provisionprofile',
                '$W/App Product.app/Contents/embedded.provisionprofile')
        ])

        # Ensure that only the app is signed.
        signed_paths = [
            call[1][2].path for call in kwargs['sign_part'].mock_calls
        ]
        self.assertEqual(signed_paths, ['App Product.app'])

        self.assertEqual(kwargs['run_command'].mock_calls, [
            mock.call.run_command([
                'codesign', '--display', '--requirements', '-', '--verbose=5',
                '$W/App Product.app'
            ]),
            mock.call.run_command(
                ['spctl', '--assess', '-vv', '$W/App Product.app']),
        ])

    @mock.patch(
        'signing.commands.plistlib.readPlist',
        side_effect=_get_plist_read('99.0.9999.99'))
    def test_sanity_check_ok(self, read_plist, **kwargs):
        config = model.Distribution().to_config(test_config.TestConfig())
        parts.sign_chrome(self.paths, config, sign_framework=True)

    @mock.patch(
        'signing.commands.plistlib.readPlist',
        side_effect=_get_plist_read('55.0.5555.55'))
    def test_sanity_check_bad(self, read_plist, **kwargs):
        config = model.Distribution().to_config(test_config.TestConfig())
        self.assertRaises(
            ValueError,
            lambda: parts.sign_chrome(self.paths, config, sign_framework=True))
