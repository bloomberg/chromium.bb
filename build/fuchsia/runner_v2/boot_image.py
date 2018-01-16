# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions used to provision Fuchsia boot images."""

import common
import os
import subprocess
import tempfile

_SSH_CONFIG_TEMPLATE = """
Host *
  CheckHostIP no
  StrictHostKeyChecking no
  ForwardAgent no
  ForwardX11 no
  GSSAPIDelegateCredentials no
  UserKnownHostsFile {known_hosts}
  User fuchsia
  IdentitiesOnly yes
  IdentityFile {identity}
  ServerAliveInterval 1
  ServerAliveCountMax 1"""


def _GetKernelPath(target_arch):
  return os.path.join(_TargetCpuToSdkBinPath(target_arch), 'zircon.bin')


def _TargetCpuToSdkBinPath(target_arch):
  """Returns the path to the kernel & bootfs .bin files for |target_cpu|."""
  return os.path.join(common.SDK_ROOT, 'target', target_arch)


def _ProvisionSSH(output_dir):
  """Provisions the key files used by the SSH daemon, and generates a
  configuration file used by clients for connecting to SSH.

  Returns a tuple with:
  #0: the client configuration file
  #1: a list of file path pairs: (<path in image>, <path on build filesystem>).
  """

  host_key_path = output_dir + '/ssh_key'
  host_pubkey_path = host_key_path + '.pub'
  id_key_path = output_dir + '/id_ed25519'
  id_pubkey_path = id_key_path + '.pub'
  known_hosts_path = output_dir + '/known_hosts'
  ssh_config_path = output_dir + '/ssh_config'

  if not os.path.isfile(host_key_path):
    subprocess.check_call(['ssh-keygen', '-t', 'ed25519', '-h', '-f',
                           host_key_path, '-P', '', '-N', ''],
                          stdout=open(os.devnull))
  if not os.path.isfile(id_key_path):
    subprocess.check_call(['ssh-keygen', '-t', 'ed25519', '-f', id_key_path,
                           '-P', '', '-N', ''], stdout=open(os.devnull))

  with open(ssh_config_path, "w") as ssh_config:
    ssh_config.write(
        _SSH_CONFIG_TEMPLATE.format(identity=id_key_path,
                                    known_hosts=known_hosts_path))

  return (
      ssh_config_path,
      (('data/ssh/ssh_host_ed25519_key', host_key_path),
       ('data/ssh/ssh_host_ed25519_key.pub', host_pubkey_path),
       ('data/ssh/authorized_keys', id_pubkey_path))
  )


def CreateBootFS(output_dir, target_arch):
  """Creates a bootfs image provisoned with the credentials necessary
  for SSH remote access.

  Returns a tuple with the path to SSH config and the path to the boot
  image."""

  boot_image = os.path.join(
      _TargetCpuToSdkBinPath(target_arch), 'bootdata.bin')
  ssh_manifest = tempfile.NamedTemporaryFile(delete=False)
  ssh_config, ssh_data = _ProvisionSSH(output_dir)
  for key, val in ssh_data:
    ssh_manifest.write("%s=%s\n" % (key, val))
  ssh_manifest.close()
  mkbootfs_path = os.path.join(common.SDK_ROOT, 'tools', 'mkbootfs')
  bootfs_name = output_dir + '/image.bootfs'
  args = [mkbootfs_path, '-o', bootfs_name,
          '--target=boot', boot_image,
          '--target=system', ssh_manifest.name]

  subprocess.check_call(args)
  os.remove(ssh_manifest.name)

  return ssh_config, bootfs_name
