# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions used to provision Fuchsia boot images."""

import common
import logging
import os
import subprocess
import tempfile
import time
import uuid

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
  ssh_config_path = GetSSHConfigPath(output_dir)

  logging.debug('Generating SSH credentials.')
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


def GetKernelPath(target_arch):
  return os.path.join(_TargetCpuToSdkBinPath(target_arch), 'zircon.bin')


def GetSSHConfigPath(output_dir):
  return output_dir + '/ssh_config'


def CreateBootdata(output_dir, target_arch):
  """Creates a bootdata image ready for SSH remote access.

  Returns a path to the bootdata.bin file."""

  base_boot_data = os.path.join(
      _TargetCpuToSdkBinPath(target_arch), 'bootdata.bin')
  ssh_config, ssh_data = _ProvisionSSH(output_dir)
  ssh_manifest = tempfile.NamedTemporaryFile(delete=False)
  for key, val in ssh_data:
    ssh_manifest.write("%s=%s\n" % (key, val))
  ssh_manifest.close()
  mkbootfs_path = os.path.join(common.SDK_ROOT, 'tools', 'mkbootfs')
  bootfs_path = output_dir + '/image.bootfs'
  args = [mkbootfs_path, '-o', bootfs_path,
          '--target=boot', base_boot_data,
          '--target=system', ssh_manifest.name]

  logging.debug(' '.join(args))
  subprocess.check_call(args)
  os.remove(ssh_manifest.name)

  return bootfs_path


def GetNodeName(output_dir):
  """Returns the cached Zircon node name, or generates one if it doesn't
  already exist. The node name is used by Discover to find the prior
  deployment on the LAN."""

  nodename_file = os.path.join(output_dir, 'nodename')
  if not os.path.exists(nodename_file):
    nodename = uuid.uuid4()
    f = open(nodename_file, 'w')
    f.write(str(nodename))
    f.flush()
    f.close()
    return str(nodename)
  else:
    f = open(nodename_file, 'r')
    return f.readline()


def GetKernelArgs(output_dir):
  return ['devmgr.epoch=%d' % time.time(),
          'zircon.nodename=' + GetNodeName(output_dir)]
