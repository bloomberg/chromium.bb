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

  if os.path.exists(known_hosts_path):
    os.remove(known_hosts_path)

  return (
      ssh_config_path,
      (('ssh/ssh_host_ed25519_key', host_key_path),
       ('ssh/ssh_host_ed25519_key.pub', host_pubkey_path),
       ('ssh/authorized_keys', id_pubkey_path))
  )


def GetTargetFile(target_arch, filename):
  """Computes a path to |filename| in the Fuchsia target directory specific to
  |target_arch|."""

  return os.path.join(_TargetCpuToSdkBinPath(target_arch), filename)


def GetSSHConfigPath(output_dir):
  return output_dir + '/ssh_config'


def ConfigureDataFVM(output_dir, sparse):
  """Builds the FVM image for the persistent /data volume and prepopulates it
  with SSH keys.

  output_dir: Path to the output directory which will contain the FVM file.
  sparse: If true, then a netboot-friendly sparse file will be generated.

  Returns the path to the new FVM file."""

  logging.debug('Building persistent data FVM file.')
  data_file = os.path.join(output_dir, 'data.minfs.bin')
  try:
    # Build up the minfs partition data and install keys into it.
    ssh_config, ssh_data = _ProvisionSSH(output_dir)
    manifest = tempfile.NamedTemporaryFile()
    for dest, src in ssh_data:
      manifest.write('%s=%s\n' % (dest, src))
    manifest.flush()
    minfs_path = os.path.join(common.SDK_ROOT, 'tools', 'minfs')
    subprocess.check_call([minfs_path, '%s@10m' % data_file, 'create'])
    subprocess.check_call([minfs_path, data_file, 'manifest', manifest.name])

    # Wrap the minfs partition in a FVM container.
    fvm_path = os.path.join(common.SDK_ROOT, 'tools', 'fvm')
    fvm_output_path = os.path.join(output_dir, 'fvm.data.blk')
    if os.path.exists(fvm_output_path):
      os.remove(fvm_output_path)

    if sparse:
      cmd = [fvm_path, fvm_output_path, 'sparse', '--compress', 'lz4',
             '--data', data_file]
    else:
      cmd = [fvm_path, fvm_output_path, 'create', '--data', data_file]

    logging.debug(' '.join(cmd))

    subprocess.check_call(cmd)
    return fvm_output_path

  finally:
    os.remove(data_file)


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
