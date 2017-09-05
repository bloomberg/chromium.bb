# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import uuid

from devil.utils import reraiser_thread
from pylib import constants


_MINIUMUM_TIMEOUT = 5.0  # Large enough to account for process start-up.
_PER_LINE_TIMEOUT = .002  # Should be able to process 500 lines per second.


class Deobfuscator(object):
  def __init__(self, mapping_path):
    self._reader_thread = None
    script_path = os.path.join(
        constants.GetOutDirectory(), 'bin', 'java_deobfuscate')
    cmd = [script_path, mapping_path]
    # Start process eagerly to hide start-up latency.
    self._proc = subprocess.Popen(
        cmd, bufsize=1, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        close_fds=True)
    self._logged_error = False

  def IsClosed(self):
    return self._proc.returncode is not None

  def IsBusy(self):
    return bool(self._reader_thread)

  def IsReady(self):
    return not self.IsClosed() and not self.IsBusy()

  def TransformLines(self, lines):
    """Deobfuscates obfuscated names found in the given lines.

    If anything goes wrong (process crashes, timeout, etc), returns |lines|.

    Args:
      lines: A list of strings without trailing newlines.

    Returns:
      A list of strings without trailing newlines.
    """
    if not lines:
      return []

    # Allow only one thread to communicate with the subprocess at a time.
    if self._reader_thread:
      logging.warning('Having to wait for Java deobfuscation.')
      self._reader_thread.join()

    if self._proc.returncode is not None:
      if not self._logged_error:
        logging.warning('java_deobfuscate process exited with code=%d.',
                        self._proc.returncode)
        self._logged_error = True
      return lines

    out_lines = []
    eof_line = uuid.uuid4().hex

    def deobfuscate_reader():
      while True:
        line = self._proc.stdout.readline()[:-1]
        # Due to inlining, deobfuscated stacks may contain more frames than
        # obfuscated ones. To account for the variable number of lines, keep
        # reading until eof_line.
        if line == eof_line:
          break
        out_lines.append(line)

    # TODO(agrieve): Can probably speed this up by only sending lines through
    #     that might contain an obfuscated name.
    self._reader_thread = reraiser_thread.ReraiserThread(deobfuscate_reader)
    self._reader_thread.start()
    try:
      self._proc.stdin.write('\n'.join(lines))
      self._proc.stdin.write('\n{}\n'.format(eof_line))
      self._proc.stdin.flush()
      timeout = max(_MINIUMUM_TIMEOUT, len(lines) * _PER_LINE_TIMEOUT)
      self._reader_thread.join(timeout)
      if self._reader_thread.is_alive():
        logging.error('java_deobfuscate timed out.')
        self.Close()
      self._reader_thread = None
      return out_lines
    except IOError:
      logging.exception('Exception during java_deobfuscate')
      self.Close()
      return lines

  def Close(self):
    if not self.IsClosed():
      self._proc.stdin.close()
      self._proc.kill()
      self._proc.wait()
    self._reader_thread = None

  def __del__(self):
    if not self.IsClosed():
      logging.error('Forgot to Close() deobfuscator')


class DeobfuscatorPool(object):
  def __init__(self, mapping_path, pool_size=4):
    self._mapping_path = mapping_path
    self._pool = [Deobfuscator(mapping_path) for _ in xrange(pool_size)]

  def TransformLines(self, lines):
    target_instance = next((x for x in self._pool if x.IsReady()), None)

    # Restart any closed ones.
    for i, d in enumerate(self._pool):
      if d.IsClosed():
        logging.warning('Restarting closed Deobfuscator instance.')
        self._pool[i] = Deobfuscator(self._mapping_path)

    if not target_instance:
      # No idle ones. Use the first one and cycle so as to not choose it again.
      target_instance = self._pool[0]
      self._pool.append(self._pool.pop(0))

    return target_instance.TransformLines(lines)

  def Close(self):
    for d in self._pool:
      d.Close()
