# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains a helper function for deploying and executing a packaged
executable on a Target."""

import common
import json
import logging
import os
import shutil
import subprocess
import tempfile
import uuid

from symbolizer import FilterStream

FAR = os.path.join(common.SDK_ROOT, 'tools', 'far')
PM = os.path.join(common.SDK_ROOT, 'tools', 'pm')


def _Deploy(target, output_dir, archive_path):
  """Converts the FAR archive at |archive_path| into a Fuchsia package
  and deploys it to |target|'s blobstore. If |incremental| is set,
  then the remote target's blobstore is queried and only the changed blobs
  are copied.

  Returns: the name of the package, which can be run by executing
           'run <package_name>' on the target."""

  logging.info('Deploying package to target.')

  staging_path = None
  blob_link_dir = None
  far_contents_dir = None
  try:
    package_name = os.path.basename(archive_path)

    logging.debug('Extracting archive contents.')
    far_contents_dir = tempfile.mkdtemp()
    subprocess.check_call([FAR, 'extract', '--archive=%s' % archive_path,
                           '--output=%s' % far_contents_dir])

    logging.debug('Building package metadata.')
    with open(os.path.join(far_contents_dir, 'meta', 'package.json'), 'w') \
        as package_json:
      json.dump({'version': '0', 'name': package_name}, package_json)
    manifest = tempfile.NamedTemporaryFile()
    for root, _, files in os.walk(far_contents_dir):
      for f in files:
        path = os.path.join(root, f)
        manifest.write('%s=%s\n' %
                       (os.path.relpath(path, far_contents_dir), path))
    manifest.flush()

    # Generate the package signing key.
    signing_key_path = os.path.join(output_dir, 'signing-key')
    if not os.path.exists(signing_key_path):
      subprocess.check_call([PM, '-k', signing_key_path, 'genkey'])

    # Build the package metadata archive.
    staging_path = tempfile.mkdtemp()
    subprocess.check_call([PM, '-o', staging_path, '-k', signing_key_path,
                           '-m', manifest.name, 'build'])

    # If the target is already warm, then it's possible that some time can be
    # saved by only sending the sending the changed blobs.
    # If the target is newly booted, however, it can be safely assumed that the
    # target doesn't contain the blobs, so the query step can be skipped.
    existing_blobs = set()
    if not target.IsNewInstance():
      logging.debug('Querying the target blobstore\'s state.')
      ls = target.RunCommandPiped(['ls', '/blobstore'], stdout=subprocess.PIPE)
      for blob in ls.stdout:
        existing_blobs.add(blob.strip())
      ls.wait()

    # Locally stage the blobs and metadata into a local temp dir to prepare for
    # a one-shot 'scp' copy.
    blob_link_dir = tempfile.mkdtemp()
    os.symlink(os.path.join(staging_path, 'meta.far'),
               os.path.join(blob_link_dir, 'meta.far'))
    for next_line in open(os.path.join(staging_path, 'meta', 'contents')):
      rel_path, blob = next_line.strip().split('=')
      dest_path = os.path.join(blob_link_dir, blob)
      if blob not in existing_blobs:
        existing_blobs.add(blob)
        os.symlink(os.path.join(far_contents_dir, rel_path),
                   os.path.join(blob_link_dir, blob))

    # Copy the package metadata and contents into tmpfs, and then execute
    # remote commands to copy the data into pkgfs/incoming.
    # We can't use 'scp' to copy directly into /pkgfs/incoming due to a bug in
    # Fuchsia (see bug PKG-9).
    logging.debug('Deploying to target blobstore.')
    target.PutFile(blob_link_dir, '/tmp', recursive=True)

    # Copy everything into target's /pkgfs/incoming and then clean up.
    # The command is joined with double ampersands so that all the steps can
    # be done over the lifetime of a single SSH connection.
    # Note that 'mv' moves to pkgfs are unsupported (Fuchsia bug PKG-9).
    blob_tmpdir = os.path.join('/tmp/', os.path.basename(blob_link_dir))
    result = target.RunCommand([
        # Speed things up by using builtin commands, which don't suffer the
        # dynamic library loading speed penalty that's present in the
        # executables.
        'unset PATH', '&&'

        # Separate meta.far from the blobs so we can add the blobs in
        # bulk in advance of registering the package (faster).
        'mv', os.path.join(blob_tmpdir, 'meta.far'), '/tmp', '&&',

        # Register the package.
        'cp', '/tmp/meta.far', '/pkgfs/incoming', '&&',

        # Install the blobs into /pkgfs.
        'for', 'f', 'in', os.path.join(blob_tmpdir, '*'), ';', 'do',
               'cp', '$f', '/pkgfs/incoming', ';',
        'done', '&&',

        'rm', '-rf', blob_tmpdir, '&&',

        'rm', '/tmp/meta.far'])


    if result != 0:
      raise Exception('Deployment failed.')

    return package_name

  finally:
    if blob_link_dir:
      shutil.rmtree(blob_link_dir)
    if staging_path:
      shutil.rmtree(staging_path)
    if far_contents_dir:
      shutil.rmtree(far_contents_dir)


def RunPackage(output_dir, target, package_path, run_args,
               symbolizer_config=None):
  """Copies the Fuchsia package at |package_path| to the target,
  executes it with |run_args|, and symbolizes its output.

  output_dir: The path containing the build output files.
  target: The deployment Target object that will run the package.
  package_path: The path to the .far package file.
  run_args: The command-linearguments which will be passed to the Fuchsia process.
  symbolizer_config: A newline delimited list of source files contained in the
                     package. Omitting this parameter will disable symbolization.

  Returns the exit code of the remote package process."""


  package_name = _Deploy(target, output_dir, package_path)
  command = ['run', package_name] + run_args
  process = target.RunCommandPiped(command,
                                   stdin=open(os.devnull, 'r'),
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)

  if symbolizer_config:
    # Decorate the process output stream with the symbolizer.
    output = FilterStream(process.stdout, package_name,
                          symbolizer_config, output_dir)
  else:
    output = process.stdout

  for next_line in output:
    print next_line

  process.wait()
  if process.returncode != 0:
    # The test runner returns an error status code if *any* tests fail,
    # so we should proceed anyway.
    logging.warning('Command exited with non-zero status code %d.' %
                    process.returncode)

  return process.returncode
