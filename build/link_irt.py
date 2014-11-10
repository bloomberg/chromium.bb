#!/usr/bin/python
# Copyright 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for linking the NaCl IRT and IRT core.

This module will take care of linking the NaCl IRT.  Compiling the libraries
and object files that go into the NaCl IRT is done outside of this script.

Linking is factored out because the IRT has specific requirements for
where to place the text vs data segments, and also requires editting how
TLS access is done.  Thus, it's more complicated than the usual linking.
"""

import argparse
import os
import sys

from build_nexe_tools import (CommandRunner, Error, FixPath, MakeDir)


class IRTLinker(CommandRunner):
  """Builder object that generates build command-lines and runs them.
  """

  def __init__(self, options):
    super(IRTLinker, self).__init__(options)
    # IRT constraints for auto layout.
    # IRT text can only go up to 256MB. Addresses after that are for data.
    # Reserve an extra page because:
    # * sel_ldr requires a HLT sled at the end of the dynamic code area;
    # * dynamic_load_test currently tests loading at the end of the dynamic
    #   code area.
    self.irt_text_max = 0x10000000 - 0x10000
    # Data can only go up to the sandbox_top - sizeof(stack).
    # NaCl allocates 16MB for the initial thread's stack (see
    # NACL_DEFAULT_STACK_MAX in sel_ldr.h).
    # Assume sandbox_top is 1GB, since even on x86-64 the limit would
    # only be 2GB (rip-relative references can only be +/- 2GB).
    sandbox_top = 0x40000000
    self.irt_data_max = sandbox_top - (16 << 20)
    self.output = options.output
    self.link_cmd = options.link_cmd
    self.readelf_cmd = options.readelf_cmd
    self.tls_edit = options.tls_edit
    self.SetCommandsAreScripts(options.commands_are_scripts)

  def GetIRTLayout(self, irt_file):
    """Check if the IRT's data and text segment fit layout constraints and
       get sizes of the IRT's text and data segments.

    Returns a tuple containing:
      * whether the IRT data/text top addresses fit within the max limit
      * current data/text top addrs
      * size of text and data segments
    """
    cmd_line = [self.readelf_cmd, '-W', '--segments', irt_file]
    # Put LC_ALL=C in the environment for readelf, so that its messages
    # will reliably match what we're looking for rather than being in some
    # other language and/or character set.
    env = dict(os.environ)
    env['LC_ALL'] = 'C'
    segment_info = self.Run(cmd_line, get_output=True, env=env)
    lines = segment_info.splitlines()
    ph_start = -1
    for i, line in enumerate(lines):
      if line == 'Program Headers:':
        ph_start = i + 1
        break
    if ph_start == -1:
      raise Error('Could not find Program Headers start: %s\n' % lines)
    seg_lines = lines[ph_start:]
    text_bottom = 0
    text_top = 0
    data_bottom = 0
    data_top = 0
    for line in seg_lines:
      pieces = line.split()
      # Type, Offset, Vaddr, Paddr, FileSz, MemSz, Flg(multiple), Align
      if len(pieces) >= 8 and pieces[0] == 'LOAD':
        # Vaddr + MemSz
        segment_bottom = int(pieces[2], 16)
        segment_top = segment_bottom + int(pieces[5], 16)
        if pieces[6] == 'R' and pieces[7] == 'E':
          text_top = max(segment_top, text_top)
          if text_bottom == 0:
            text_bottom = segment_bottom
          else:
            text_bottom = min(segment_bottom, text_bottom)
          continue
        if pieces[6] == 'R' or pieces[6] == 'RW':
          data_top = max(segment_top, data_top)
          if data_bottom == 0:
            data_bottom = segment_bottom
          else:
            data_bottom = min(segment_bottom, data_bottom)
          continue
    if text_top == 0 or data_top == 0 or text_bottom == 0 or data_bottom == 0:
      raise Error('Could not parse IRT Layout: text_top=0x%x text_bottom=0x%x\n'
                  '                            data_top=0x%x data_bottom=0x%x\n'
                  'readelf output: %s\n' % (text_top, text_bottom,
                                            data_top, data_bottom, lines))
    return ((text_top <= self.irt_text_max and
            data_top <= self.irt_data_max), text_top, data_top,
            text_top - text_bottom, data_top - data_bottom)

  def GetIRTLayoutFlags(self, text_size, data_size):
    """Get additional the linker flags to place IRT's data and text segment."""
    def RoundDownToAlign(x):
      return x - (x % 0x10000)
    def GetFlag(flag_name, size, expected_max):
      self.Log('IRT %s size is %s' % (flag_name, size))
      new_start = RoundDownToAlign(expected_max - size)
      self.Log('Choosing link flag %s to %s' % (flag_name,
                                                hex(new_start)))
      return "-Wl,%s=%s" % (flag_name, hex(new_start))
    flags = []
    flags.append(GetFlag('-Ttext-segment', text_size, self.irt_text_max))
    flags.append(GetFlag('-Trodata-segment', data_size, self.irt_data_max))
    return flags

  def RunLink(self, cmd_line, link_out):
    self.CleanOutput(link_out)
    err = self.Run(cmd_line)
    if err:
      raise Error('FAILED with %d: %s' % (err, ' '.join(cmd_line)))

  def Link(self, link_args):
    """Link the IRT with the given link_args."""
    out = self.output
    self.Log('\nLinking IRT: %s' % out)
    pre_tls_edit_out = out + '.raw'

    MakeDir(os.path.dirname(pre_tls_edit_out))

    cmd_line = [self.link_cmd, '-o', pre_tls_edit_out, '-Wl,--as-needed']
    cmd_line += link_args

    # Do an initial link of the IRT, without segment layout parameters
    # to determine the segment sizes.
    self.RunLink(cmd_line, pre_tls_edit_out)

    # Then grab the segment sizes and re-link w/ the right layout.
    # 'fits' is ignored after the first link, since correct layout parameters
    # were not present in the command line.
    (fits, text_top, data_top,
     text_size, data_size) = self.GetIRTLayout(pre_tls_edit_out)
    cmd_line += self.GetIRTLayoutFlags(text_size, data_size)
    self.RunLink(cmd_line, pre_tls_edit_out)
    (fits, text_top, data_top,
     text_size, data_size) = self.GetIRTLayout(pre_tls_edit_out)
    if not fits:
      raise Error('Already re-linked IRT and it still does not fit:\n'
                  'text_top=0x%x and data_top=0x%x\n' % (
                      text_top, data_top))
    self.Log('IRT layout fits: text_top=0x%x and data_top=0x%x' %
             (text_top, data_top))

    tls_edit_cmd = [FixPath(self.tls_edit), pre_tls_edit_out, out]
    tls_edit_err = self.Run(tls_edit_cmd, possibly_script=False)
    if tls_edit_err:
      raise Error('FAILED with %d: %s' % (err, ' '.join(tls_edit_cmd)))


def Main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-o', '--output', dest='output', required=True,
                      help='Output filename')
  parser.add_argument('--tls-edit', dest='tls_edit', required=True,
                      help='Path of tls edit utility')
  parser.add_argument('--link-cmd', dest='link_cmd', required=True,
                      help='Path of linker utility')
  parser.add_argument('--readelf-cmd', dest='readelf_cmd', required=True,
                      help='Path of readelf utility')
  parser.add_argument('-v', '--verbose', dest='verbose', default=False,
                      help='Enable verbosity', action='store_true')
  parser.add_argument('--commands-are-scripts', dest='commands_are_scripts',
                      action='store_true', default=False,
                      help='Indicate that toolchain commands are scripts')
  args, remaining_args = parser.parse_known_args()
  linker = IRTLinker(args)
  linker.Link(remaining_args)
  return 0


if __name__ == '__main__':
  sys.exit(Main())
