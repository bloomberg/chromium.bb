#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to collect crash signatures for Chrome builds on Linux."""

import fnmatch
import optparse
import os
import shutil
import subprocess
import struct
import sys
import tempfile


def VerifySymbolAndCopyToTempDir(symbol_file, temp_dir, sym_module_name):
  """Verify the symbol file looks correct and copy it to the right place
  in temp_dir.

  Args:
    symbol_file: the path to the symbol file.
    temp_dir: the base of the temp directory where the symbol file will reside.
  Returns:
    True on success.
  """
  symbol = open(symbol_file)
  signature_line = symbol.readline().strip().split()
  symbol.close()
  # signature_line should look like:
  # MODULE Linux x86 28D8A79A426807B5462CBA24F56746750 chrome
  if (len(signature_line) == 5 and signature_line[0] == 'MODULE' and
      signature_line[1] == 'Linux' and signature_line[4] == sym_module_name and
      len(signature_line[3]) == 33):
    dest = os.path.join(temp_dir, signature_line[4], signature_line[3])
    os.makedirs(dest)
    dest_file = os.path.join(dest, '%s.sym' % signature_line[4])
    shutil.copyfile(symbol_file, dest_file)
    return True
  return False


def GetCommandOutput(command):
  """Runs the command list, returning its output.

  Prints the given command (which should be a list of one or more strings),
  then runs it and returns its output (stdout and stderr) as a string.

  If the command exits with an error, raises OSError.

  From chromium_utils.
  """
  proc = subprocess.Popen(command, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, bufsize=1)
  output = proc.communicate()[0]
  if proc.returncode:
    raise OSError('%s: %s' % (subprocess.list2cmdline(command), output))
  return output


def GetCrashDumpDir():
  """Returns the default crash dump directory used by Chromium."""
  config_home = os.environ.get('XDG_CONFIG_HOME')
  if not config_home:
    home = os.path.expanduser('~')
    if not home:
      return ''
    config_home = os.path.join(home, '.config')
  return os.path.join(config_home, 'chromium', 'Crash Reports')


def GetStackTrace(processor_bin, symbol_path, dump_file):
  """Gets and prints the stack trace from a crash dump file.

  Args:
    processor_bin: the path to the processor.
    symbol_path: root dir for the symbols.
    dump_file: the path to the dump file.
  Returns:
    A string representing the stack trace.
  """
  # Run processor to analyze crash dump.
  cmd = [processor_bin, '-m', dump_file, symbol_path]

  try:
    output = GetCommandOutput(cmd)
  except OSError:
    return 'Cannot get stack trace.'

  # Retrieve stack trace from processor output. Processor output looks like:
  # ----------------
  # Debug output
  # ...
  # Debug output
  # Module ...
  # ...
  # Module ...
  #
  # N|...     <--+
  # ...          |--- crashed thread stack trace
  # N|...     <--+
  # M|...
  # ...
  # ----------------
  # where each line of the stack trace looks like:
  # ThreadNumber|FrameNumber|ExeName|Function|SourceFile|LineNo|Offset

  stack_trace_frames = []
  idx = output.find('\nModule')
  if idx >= 0:
    output = output[idx+1:]
    idx = output.find('\n\n')
    if idx >= 0:
      output = output[idx+2:].splitlines()
      if output:
        first_line = output[0].split('|')
        if first_line:
          crashed_thread = first_line[0]
          for line in output:
            line_split = line.split('|')
            if not line_split:
              break
            if line_split[0] != crashed_thread:
              break
            stack_trace_frames.append(line_split)
  if not stack_trace_frames:
    return 'Cannot get stack trace.'
  stack_trace = []
  for frame in stack_trace_frames:
    if len(frame) != 7:
      continue
    (exe, func, source, line, offset) = frame[2:]
    if not exe or not source or not line or not offset:
      continue
    idx = func.find('(')
    if idx >= 0:
      func = func[:idx]
    if not func:
      continue
    frame_output = '%s!%s+%s [%s @ %s]' % (exe, func, offset, source, line)
    stack_trace.append(frame_output)
  return '\n'.join(stack_trace)


def LocateFiles(pattern, root=os.curdir):
  """Yields files matching pattern found in root and its subdirectories.

  An exception is thrown if root doesn't exist.

  From chromium_utils."""
  root = os.path.expanduser(root)
  for path, dirs, files in os.walk(os.path.abspath(root)):
    for filename in fnmatch.filter(files, pattern):
      yield os.path.join(path, filename)


def ProcessDump(dump_file, temp_dir):
  """Extracts the part of the dump file that minidump_stackwalk can read.

  Args:
    dump_file: the dump file that needs to be processed.
    temp_dir: the temp directory to put the dump file in.
  Returns:
    path of the processed dump file.
  """
  dump = open(dump_file, 'rb')
  dump_data = dump.read()
  dump.close()
  idx = dump_data.find('MDMP')
  if idx < 0:
    return ''

  dump_data = dump_data[idx:]
  if not dump_data:
    return ''
  (dump_fd, dump_name) = tempfile.mkstemp(suffix='chromedump', dir=temp_dir)
  os.write(dump_fd, dump_data)
  os.close(dump_fd)
  return dump_name


def main_linux(options, args):
  # minidump_stackwalk is part of Google Breakpad. You may need to checkout
  # the code and build your own copy. http://google-breakpad.googlecode.com/
  LINUX_PROCESSOR = 'minidump_stackwalk'
  processor_bin = None
  if options.processor_dir:
    bin = os.path.join(os.path.expanduser(options.processor_dir),
                       LINUX_PROCESSOR)
    if os.access(bin, os.X_OK):
      processor_bin = bin
  else:
    for path in os.environ['PATH'].split(':'):
      bin = os.path.join(path, LINUX_PROCESSOR)
      if os.access(bin, os.X_OK):
        processor_bin = bin
        break
  if not processor_bin:
    print 'Cannot find minidump_stackwalk.'
    return 1

  if options.symbol_filename:
    symbol_file = options.symbol_filename
  else:
    if options.architecture:
      bits = options.architecture
    else:
      bits = struct.calcsize('P') * 8
    if bits == 32:
      symbol_file = 'chrome.breakpad.ia32'
    elif bits == 64:
      symbol_file = 'chrome.breakpad.x64'
    else:
      print 'Unknown architecture'
      return 1

  symbol_dir = options.symbol_dir
  if not options.symbol_dir:
    symbol_dir = os.curdir
  symbol_dir = os.path.abspath(os.path.expanduser(symbol_dir))
  symbol_file = os.path.join(symbol_dir, symbol_file)
  if not os.path.exists(symbol_file):
    print 'Cannot find symbols.'
    return 1
  symbol_time = os.path.getmtime(symbol_file)

  dump_files = []
  if options.dump_file:
    dump_files.append(options.dump_file)
  else:
    dump_dir = options.dump_dir
    if not dump_dir:
      dump_dir = GetCrashDumpDir()
    if not dump_dir:
      print 'Cannot find dump files.'
      return 1
    for dump_file in LocateFiles(pattern='*.dmp', root=dump_dir):
      file_time = os.path.getmtime(dump_file)
      if file_time < symbol_time:
        # Ignore dumps older than symbol file.
        continue
      dump_files.append(dump_file)

  temp_dir = tempfile.mkdtemp(suffix='chromedump')
  if not VerifySymbolAndCopyToTempDir(symbol_file, temp_dir,
                                      options.sym_module_name):
    print 'Cannot parse symbols.'
    shutil.rmtree(temp_dir)
    return 1

  dump_count = 0
  for dump_file in dump_files:
    processed_dump_file = ProcessDump(dump_file, temp_dir)
    if not processed_dump_file:
      continue
    print '-------------------------'
    print GetStackTrace(processor_bin, temp_dir, processed_dump_file)
    print
    os.remove(processed_dump_file)
    dump_count += 1

  shutil.rmtree(temp_dir)
  print '%s dumps found' % dump_count
  return 0


def main():
  if not sys.platform.startswith('linux'):
    return 1
  parser = optparse.OptionParser()
  parser.add_option('', '--processor-dir', type='string', default='',
                    help='The directory where the processor is installed. '
                         'The processor is used to get stack trace from dumps. '
                          'Searches $PATH by default')
  parser.add_option('', '--dump-file', type='string', default='',
                    help='The path of the dump file to be processed. '
                         'Overwrites dump-path.')
  parser.add_option('', '--dump-dir', type='string', default='',
                    help='The directory where dump files are stored. '
                         'Searches this directory if dump-file is not '
                         'specified. Default is the Chromium crash directory.')
  parser.add_option('', '--symbol-dir', default='',
                    help='The directory with the symbols file. [Required]')
  parser.add_option('', '--symbol-filename', default='',
                    help='The name of the symbols file to use.  '
                         'This argument overrides --architecture.')
  parser.add_option('', '--architecture', type='int', default=None,
                    help='Override automatic x86/x86-64 detection. '
                         'Valid values are 32 and 64')
  parser.add_option('', '--sym-module-name', type='string', default='chrome',
                    help='The module name for the symbol file.  '
                         'Default: chrome')

  (options, args) = parser.parse_args()
  return main_linux(options, args)


if '__main__' == __name__:
  sys.exit(main())
