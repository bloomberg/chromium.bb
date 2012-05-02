# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import errno
import signal
import subprocess
import cros_build_lib


class SudoKeepAlive(cros_build_lib.MasterPidContextManager):

  """
  This refreshes the sudo auth cookie; this is implemented this
  way to ensure that sudo has access to both invoking tty, and
  will update the user's tty-less cookie.
  see crosbug/18393.
  """

  def __init__(self, repeat_interval=4):
    """Run sudo with a noop, to reset the sudo timestamp.
    Args:
     repeat_interval: In minutes, the frequency to run the update.
    """
    cros_build_lib.MasterPidContextManager.__init__(self)
    self._repeat_interval = repeat_interval
    self._proc = None

  @staticmethod
  def _IdentifyTTY():
    for source in (sys.stdin, sys.stdout, sys.stderr):
      try:
        return os.ttyname(source.fileno())
      except EnvironmentError, e:
        if e.errno not in (errno.EINVAL, errno.ENOTTY):
          raise

    return 'unknown'

  def _DaemonNeeded(self):
    """
    Discern if we need to run the sudo keep alive code.

    Returns:
     None if the daemon isn't needed, else the string of the pts needed.
    """
    existing = os.environ.get("CROS_SUDO_KEEP_ALIVE")
    tty = self._IdentifyTTY()

    if existing is None:
      return tty
    elif tty == existing:
      # Same auth ticket as our parent; we can use it.
      return None
    elif tty == 'unknown':
      # The existing instance still covers us, despite us being bound to no pts.
      return None

    # Reaching here means that somehow, we're on a literal different pts than
    # the original.  This requires a daemon.
    return tty

  def _enter(self):
    start_for_tty = self._DaemonNeeded()
    if start_for_tty is None or os.getuid() == 0:
      # We're root; we don't need the sudo keep alive hack.
      return

    # Note despite the impulse to use 'sudo -v' instead of 'sudo true', the
    # builder's sudoers configuration is slightly whacked resulting in it
    # asking for password everytime.  As such use 'sudo true' instead.

    # First check to see if we're already authed.  If so, then we don't
    # need to prompt the user for their password.

    ret = cros_build_lib.RunCommand(
        'sudo -n true 2> /dev/null && ' +
        'sudo -n true < /dev/null > /dev/null 2>&1',
        print_cmd=False, shell=True, error_code_ok=True)

    if ret.returncode != 0:
      # We need to go interactive and allow sudo to ask for credentials.

      cros_build_lib.Info('Launching sudo keepalive process. '
                          'This may ask for your password twice.')

      cros_build_lib.RunCommand(
          'sudo true; sudo true < /dev/null > /dev/null 2>&1',
          shell=True, print_cmd=False)

    repeat_interval = self._repeat_interval * 60
    cmd = 'sudo -n true && sudo -n true < /dev/null > /dev/null 2>&1'
    # Anything other than a timeout results in us shutting down.
    cmd = ('while :; do read -t %i; [ $? -le 128 ] && exit; %s; done' %
           (repeat_interval, cmd))

    def ignore_sigint():
      # We don't want our sudo process shutdown till we shut it down;
      # since it's part of the session group it however gets SIGINT.
      # Thus suppress it (which bash then inherits).
      signal.signal(signal.SIGINT, signal.SIG_IGN)

    self._proc = subprocess.Popen(['bash', '-c', cmd], shell=False,
                                  close_fds=True, preexec_fn=ignore_sigint,
                                  stdin=subprocess.PIPE)

    os.environ["CROS_SUDO_KEEP_ALIVE"] = start_for_tty

  # pylint: disable=W0613
  def _exit(self, exc_type, exc_value, traceback):
    if self._proc is None:
      return

    try:
      self._proc.terminate()
      self._proc.wait()
    except EnvironmentError, e:
      if e.errno != errno.ESRCH:
        raise

    os.environ.pop("CROS_SUDO_KEEP_ALIVE", None)


def SetFileContents(path, value):
  """Set a given filepath contents w/ the passed in value."""
  cros_build_lib.SudoRunCommand(['tee', path], redirect_stdout=True,
                                print_cmd=False, input=value)
