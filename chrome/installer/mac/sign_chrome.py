#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

sys.path.append(os.path.dirname(__file__))

from signing import config_factory, commands, logger, model, pipeline


def _link_stdout_and_stderr():
    """This script's output is entirely log messages and debugging information,
    so there is not a useful distinction between stdout and stderr. Because some
    subcommands this script runs output to one stream or the other, link the
    two streams so that any buffering done by Python, or the invoker of this
    script, does not get incorrectly interleaved.
    """
    stdout_fileno = sys.stdout.fileno()
    sys.stdout.close()
    sys.stdout = sys.stderr
    os.dup2(sys.stderr.fileno(), stdout_fileno)


def create_config(config_args, development):
    """Creates the |model.CodeSignConfig| for the signing operations.

    If |development| is True, the config will be modified to not require
    restricted internal assets, nor will the products be required to match
    specific certificate hashes.

    Args:
        config_args: List of args to expand to the config class's constructor.
        development: Boolean indicating whether or not to modify the chosen
            config for development testing.

    Returns:
        An instance of |model.CodeSignConfig|.
    """
    config_class = config_factory.get_class()

    if development:

        class DevelopmentCodeSignConfig(config_class):
            @property
            def codesign_requirements_basic(self):
                return ''

            @property
            def provisioning_profile_basename(self):
                return None

            @property
            def run_spctl_assess(self):
                # Self-signed or ad-hoc signed signing identities won't pass
                # spctl assessment so don't do it.
                return False

        config_class = DevelopmentCodeSignConfig

    return config_class(*config_args)


def _show_tool_versions():
    logger.info('Showing macOS and tool versions.')
    commands.run_command(['sw_vers'])
    commands.run_command(['xcodebuild', '-version'])
    commands.run_command(['xcrun', '-show-sdk-path'])


def main():
    parser = argparse.ArgumentParser(
        description='Code sign and package Chrome for channel distribution.')
    parser.add_argument(
        '--identity',
        required=True,
        help='The identity to sign everything but PKGs with.')
    parser.add_argument(
        '--installer-identity', help='The identity to sign PKGs with.')
    parser.add_argument(
        '--notary-user',
        help='The username used to authenticate to the Apple notary service.')
    parser.add_argument(
        '--notary-password',
        help='The password or password reference (e.g. @keychain, see '
        '`xcrun altool -h`) used to authenticate to the Apple notary service.')
    parser.add_argument(
        '--notary-asc-provider',
        help='The ASC provider string to be used as the `--asc-provider` '
        'argument to `xcrun altool`, to be used when --notary-user is '
        'associated with multiple Apple developer teams. See `xcrun altool -h. '
        'Run `iTMSTransporter -m provider -account_type itunes_connect -v off '
        '-u USERNAME -p PASSWORD` to list valid providers.')
    parser.add_argument(
        '--development',
        action='store_true',
        help='The specified identity is for development. Certain codesign '
        'requirements will be omitted.')
    parser.add_argument(
        '--input',
        required=True,
        help='Path to the input directory. The input directory should '
        'contain the products to sign, as well as the Packaging directory.')
    parser.add_argument(
        '--output',
        required=True,
        help='Path to the output directory. The signed (possibly packaged) '
        'products and installer tools will be placed here.')
    parser.add_argument(
        '--disable-packaging',
        action='store_true',
        help='Disable creating any packaging (.dmg/.pkg) specified by the '
        'configuration.')
    parser.add_argument(
        '--skip-brand',
        dest='skip_brands',
        action='append',
        default=[],
        help='Causes any distribution whose brand code matches to be skipped.')

    group = parser.add_mutually_exclusive_group(required=False)
    group.add_argument(
        '--notarize',
        dest='notarize',
        action='store_true',
        help='Defaults to False. Submit the signed application and DMG to '
        'Apple for notarization.')
    group.add_argument('--no-notarize', dest='notarize', action='store_false')

    _link_stdout_and_stderr()

    parser.set_defaults(notarize=False)
    args = parser.parse_args()

    if args.notarize:
        if not args.notary_user or not args.notary_password:
            parser.error('The --notary-user and --notary-password arguments '
                         'are required with --notarize.')

    config = create_config(
        (args.identity, args.installer_identity, args.notary_user,
         args.notary_password, args.notary_asc_provider), args.development)
    paths = model.Paths(args.input, args.output, None)

    if not os.path.exists(paths.output):
        os.mkdir(paths.output)

    _show_tool_versions()

    pipeline.sign_all(
        paths,
        config,
        disable_packaging=args.disable_packaging,
        do_notarization=args.notarize,
        skip_brands=args.skip_brands)


if __name__ == '__main__':
    main()
