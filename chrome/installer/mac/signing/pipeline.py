# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The pipeline module orchestrates the entire signing process, which includes:
    1. Customizing build products for release channels.
    2. Code signing the application bundle and all of its nested code.
    3. Producing a packaged DMG.
    4. Signing and packaging the installer tools.
"""

import os.path

from . import commands, model, modification, signing


def _package_dmg(paths, dist, config):
    """Packages a Chrome application bundle into a DMG.

    Args:
        paths: A |model.Paths| object.
        dist: The |model.Distribution| for which the product was customized.
        config: The |config.CodeSignConfig| object.

    Returns:
        A path to the produced DMG file.
    """
    packaging_dir = paths.packaging_dir(config)

    if dist.channel_customize:
        dsstore_file = 'chrome_{}_dmg_dsstore'.format(dist.channel)
        icon_file = 'chrome_{}_dmg_icon.icns'.format(dist.channel)
    else:
        dsstore_file = 'chrome_dmg_dsstore'
        icon_file = 'chrome_dmg_icon.icns'

    dmg_path = os.path.join(paths.output, '{}.dmg'.format(config.dmg_basename))
    app_path = os.path.join(paths.work, config.app_dir)

    # A locally-created empty directory is more trustworthy than /var/empty.
    empty_dir = os.path.join(paths.work, 'empty')
    commands.make_dir(empty_dir)

    # Make the disk image. Don't include any customized name fragments in
    # --volname because the .DS_Store expects the volume name to be constant.
    # Don't put a name on the /Applications symbolic link because the same disk
    # image is used for all languages.
    # yapf: disable
    commands.run_command([
        os.path.join(packaging_dir, 'pkg-dmg'),
        '--verbosity', '0',
        '--tempdir', paths.work,
        '--source', empty_dir,
        '--target', dmg_path,
        '--format', 'UDBZ',
        '--volname', config.app_product,
        '--icon', os.path.join(packaging_dir, icon_file),
        '--copy', '{}:/'.format(app_path),
        '--copy',
            '{}/keystone_install.sh:/.keystone_install'.format(packaging_dir),
        '--mkdir', '.background',
        '--copy',
            '{}/chrome_dmg_background.png:/.background/background.png'.format(
                packaging_dir),
        '--copy', '{}/{}:/.DS_Store'.format(packaging_dir, dsstore_file),
        '--symlink', '/Applications:/ ',
    ])
    # yapf: enable

    return dmg_path


def _package_installer_tools(paths, config):
    """Signs and packages all the installer tools, which are not shipped to end-
    users.

    Args:
        paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.
    """
    DIFF_TOOLS = 'diff_tools'

    tools_to_sign = signing.get_installer_tools(config)
    other_tools = (
        'dirdiffer.sh',
        'dirpatcher.sh',
        'dmgdiffer.sh',
        'keystone_install.sh',
        'pkg-dmg',
    )

    with commands.WorkDirectory(paths) as paths:
        diff_tools_dir = os.path.join(paths.work, DIFF_TOOLS)
        commands.make_dir(diff_tools_dir)

        for part in tools_to_sign.values():
            commands.copy_files(
                os.path.join(paths.input, part.path), diff_tools_dir)
            part.path = os.path.join(DIFF_TOOLS, os.path.basename(part.path))
            signing.sign_part(paths, config, part)

        for part in tools_to_sign.values():
            signing.verify_part(paths, part)

        for tool in other_tools:
            commands.copy_files(
                os.path.join(paths.packaging_dir(config), tool), diff_tools_dir)

        zip_file = os.path.join(paths.output, DIFF_TOOLS + '.zip')
        commands.run_command(['zip', '-9ry', zip_file, DIFF_TOOLS],
                             cwd=paths.work)


def sign_all(orig_paths, config, package_dmg=True):
    """For each distribution in |config|, performs customization, signing, and
    DMG packaging and places the resulting signed DMG in |orig_paths.output|.
    The |paths.input| must contain the products to customize and sign.

    Args:
        oring_paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.
        package_dmg: If True, the signed application bundle will be packaged
            into a DMG, which will also be signed. If False, the signed app
            bundle will be copied to |paths.output|.
    """
    for dist in config.distributions:
        with commands.WorkDirectory(orig_paths) as paths:
            dist_config = dist.to_config(config)

            # Copy the app to sign into the work dir.
            commands.copy_files(
                os.path.join(paths.input, config.app_dir), paths.work)

            # Customize the app bundle.
            modification.customize_distribution(paths, dist, dist_config)

            signing.sign_chrome(paths, dist_config)

            # If not packaging into a DMG, simply copy the signed bundle to the
            # output directory.
            if not package_dmg:
                dest_dir = os.path.join(paths.output, dist_config.dmg_basename)
                commands.make_dir(dest_dir)
                commands.copy_files(
                    os.path.join(paths.work, dist_config.app_dir), dest_dir)
                continue

            dmg_path = _package_dmg(paths, dist, dist_config)

            # dmg_identifier is like dmg_name but without the .dmg suffix. If a
            # brand code is in use, use the actual brand code instead of the
            # name fragment, to avoid leaking the association between brand
            # codes and their meanings.
            dmg_identifier = dist_config.dmg_basename
            if dist.branding_code:
                dmg_identifier = dist_config.dmg_basename.replace(
                    dist.dmg_name_fragment, dist.branding_code)

            product = model.CodeSignedProduct(
                dmg_path, dmg_identifier, sign_with_identifier=True)
            signing.sign_part(paths, dist_config, product)
            signing.verify_part(paths, product)

    _package_installer_tools(orig_paths, config)
